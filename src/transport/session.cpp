#include "session.h"

Session::Session(boost::asio::ip::tcp::socket p_socket, RequestCB p_request_cb)
    : request_cb_(std::move(p_request_cb)), use_ssl_(false) {
    plain_socket_ = std::make_unique<boost::asio::ip::tcp::socket>(std::move(p_socket));
}

Session::Session(boost::asio::ip::tcp::socket p_socket, boost::asio::ssl::context& ssl_context, RequestCB p_request_cb)
    : request_cb_(std::move(p_request_cb)), use_ssl_(true) {
    ssl_socket_ = std::make_unique<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(std::move(p_socket), ssl_context);
}

Session::~Session() {
    if (session_) {
        nghttp2_session_del(session_);
    }
}

void Session::start() {
    LOG_DEBUG("Start nghttp2 session");
    if (use_ssl_) {
        handle_ssl_handshake();
    } else {
        setup_nghttp2();
        read_data();
    }
}

void Session::handle_ssl_handshake() {
    auto self(shared_from_this());
    ssl_socket_->async_handshake(boost::asio::ssl::stream_base::server,
        [this, self](boost::system::error_code ec) {
            if (!ec) {
                LOG_DEBUG("SSL handshake completed");
                setup_nghttp2();
                read_data();
            } else {
                LOG_ERROR("SSL handshake failed: " << ec.message());
            }
        });
}

void Session::setup_nghttp2() {
    nghttp2_session_callbacks* callbacks;
    nghttp2_session_callbacks_new(&callbacks);

    nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, on_frame_recv_cb);
    nghttp2_session_callbacks_set_on_header_callback(callbacks, on_header_cb);
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, on_data_chunk_recv_cb);
    nghttp2_session_callbacks_set_on_stream_close_callback(callbacks, on_stream_close_cb);
    nghttp2_session_callbacks_set_send_callback(callbacks, send_cb);

    nghttp2_session_server_new(&session_, callbacks, this);
    nghttp2_session_callbacks_del(callbacks);

    // Send initial SETTINGS frame
    nghttp2_settings_entry iv[1] = {
        {NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100}
    };
    nghttp2_submit_settings(session_, NGHTTP2_FLAG_NONE, iv, 1);
}

void Session::read_data() {
    auto self(shared_from_this());
    
    if (use_ssl_) {
        ssl_socket_->async_read_some(boost::asio::buffer(read_buffer_),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    ssize_t read = nghttp2_session_mem_recv(session_, read_buffer_.data(), length);
                    if (read < 0) {
                        LOG_ERROR("nghttp2_session_mem_recv error: " << nghttp2_strerror((int)read));
                        return;
                    }
                    write_data();
                    read_data();
                }
            });
    } else {
        plain_socket_->async_read_some(boost::asio::buffer(read_buffer_),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    ssize_t read = nghttp2_session_mem_recv(session_, read_buffer_.data(), length);
                    if (read < 0) {
                        LOG_ERROR("nghttp2_session_mem_recv error: " << nghttp2_strerror((int)read));
                        return;
                    }
                    write_data();
                    read_data();
                }
            });
    }
}

void Session::write_data() {
    int rv = nghttp2_session_send(session_);
    if (rv != 0) {
        LOG_ERROR("nghttp2_session_send failed: " << nghttp2_strerror(rv));
    }
}

void Session::send_response(int32_t p_stream_id, const HttpResponse& p_response) {
    std::string status = std::to_string(p_response.status_code);
    std::string content_len = std::to_string(p_response.body.size());

    std::vector<nghttp2_nv> headers;
    headers.push_back(make_nv_ls(":status", status));
    if (p_response.body.empty()) {
        // No response body, send only headers
        nghttp2_submit_headers(session_, NGHTTP2_FLAG_END_STREAM, 
                                p_stream_id, nullptr,
                                headers.data(), headers.size(),
                                nullptr);
    }
    else {
        headers.push_back(make_nv_ls("content-type", p_response.content_type));
        headers.push_back(make_nv_ls("content-length", content_len));
        nghttp2_data_provider data_prd;
        data_prd.source.ptr = (void*)p_response.body.c_str();
        data_prd.read_callback = [](nghttp2_session* session, int32_t stream_id, uint8_t* buf,
                                   size_t length, uint32_t* data_flags, nghttp2_data_source* source,
                                   void* user_data) -> ssize_t {
            const char* data = (const char*)source->ptr;
            size_t len = strlen(data);
            
            if (len > length) len = length;
            memcpy(buf, data, len);
            *data_flags |= NGHTTP2_DATA_FLAG_EOF;
            return len;
        };
        nghttp2_submit_response(session_, p_stream_id, headers.data(), headers.size(), &data_prd);
    }
    LOG_DEBUG("Response sent on stream " << p_stream_id << " with status " << status);
}

// Static callbacks
int Session::on_frame_recv_cb(nghttp2_session* session, const nghttp2_frame* frame,
                                    void* user_data) {
    Session* sess = static_cast<Session*>(user_data);

    if(frame->hd.type == NGHTTP2_HEADERS && frame->headers.cat == NGHTTP2_HCAT_REQUEST) {
        LOG_DEBUG("Received request headers on stream " << frame->hd.stream_id);
        // If this is the end, then process it
        if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
            auto it = sess->streams_data_.find(frame->hd.stream_id);
            if(it != sess->streams_data_.end()) {
                // Process the request
                auto& stream_data = it->second;
                LOG_INFO("Processing complete request " << stream_data.method << " " << stream_data.path);

                // Create sender function that captures the session and sends the response
                auto sender = [sess](int32_t stream_id, const HttpResponse& response) {
                    sess->send_response(stream_id, response);
                    sess->write_data();
                };
                // Call the request callback with the sender
                // Capture the session and stream ID for each request
                sess->request_cb_(stream_data.method, stream_data.path, stream_data.body, frame->hd.stream_id, sender);
            }
        }
    }
    return 0;                                
}
                                
int Session::on_header_cb(nghttp2_session* session, const nghttp2_frame* frame,
                            const uint8_t* name, size_t namelen, const uint8_t* value, size_t valuelen,
                            uint8_t flags, void* user_data) {
    Session* sess = static_cast<Session*>(user_data);
    if(frame->hd.type == NGHTTP2_HEADERS && frame->headers.cat == NGHTTP2_HCAT_REQUEST) {
        auto& stream_data = sess->streams_data_[frame->hd.stream_id];
        LOG_DEBUG("Processing request headers for stream " << frame->hd.stream_id);
        auto header_name = std::string(reinterpret_cast<const char*>(name), namelen);
        auto header_value = std::string(reinterpret_cast<const char*>(value), valuelen);
        if(header_name == ":method")
            stream_data.method = header_value;
        else if(header_name == ":path")
            stream_data.path = header_value;
        // Other headers will be stored later
    }

    return 0;
}
int Session::on_data_chunk_recv_cb(nghttp2_session* session, uint8_t flags,
                                    int32_t stream_id, const uint8_t* data,
                                    size_t len, void* user_data) {
    Session* sess = static_cast<Session*>(user_data);
    LOG_DEBUG("on_data_chunk_recv_cb called: stream=" << stream_id << ", len=" << len << ", flags=" << static_cast<int>(flags));
    
    sess->streams_data_[stream_id].body.append((const char*)data, len);
    
    LOG_DEBUG("Received " << len << " bytes of data on stream " << stream_id << ", flags: " << static_cast<int>(flags) << " (END_STREAM=" << (flags & NGHTTP2_FLAG_END_STREAM ? "YES" : "NO") << ")");
    LOG_DEBUG("Total body so far: " << sess->streams_data_[stream_id].body.length() << " bytes");
    
    // Check if this is the end of the stream
    if (flags & NGHTTP2_FLAG_END_STREAM) {
        auto it = sess->streams_data_.find(stream_id);
        if(it != sess->streams_data_.end()) {
            // Process the request
            auto& stream_data = it->second;
            LOG_INFO("Processing complete request with body " << stream_data.method << " " << stream_data.path << " (body: " << stream_data.body.length() << " bytes)");

            // Create sender function that captures the session and sends the response
            auto sender = [sess](int32_t stream_id, const HttpResponse& response) {
                sess->send_response(stream_id, response);
                sess->write_data();
            };
            // Call the request callback with the sender
            sess->request_cb_(stream_data.method, stream_data.path, stream_data.body, stream_id, sender);
        }
    }
    
    return 0;
}
int Session::on_stream_close_cb(nghttp2_session* session, int32_t stream_id,
                                uint32_t error_code, void* user_data) {
    Session* sess = static_cast<Session*>(user_data);
    sess->streams_data_.erase(stream_id);
    LOG_DEBUG("Stream " << stream_id << " closed");
    return 0;
}

ssize_t Session::send_cb(nghttp2_session* session, const uint8_t* data,
                         size_t length, int flags, void* user_data) {
    Session* sess = static_cast<Session*>(user_data);
    boost::system::error_code ec;
    
    size_t written;
    if (sess->use_ssl_) {
        written = boost::asio::write(*sess->ssl_socket_, 
                                   boost::asio::buffer(data, length), 
                                   ec);
    } else {
        written = boost::asio::write(*sess->plain_socket_, 
                                   boost::asio::buffer(data, length), 
                                   ec);
    }
    
    if (ec) {
        LOG_ERROR("Write error: " << ec.message());
        return NGHTTP2_ERR_CALLBACK_FAILURE;
    }
    
    return written;
}
