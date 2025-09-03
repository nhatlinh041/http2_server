#include "http2_client.hpp"
#include "../utils/logger.h"
#include <vector>

Http2Client::Http2Client(boost::asio::io_context& io_context) 
    : io_context_(io_context), resolver_(io_context) {}

Http2Client::~Http2Client() {
    if (session_) {
        nghttp2_session_del(session_);
    }
}

void Http2Client::send_request(const std::string& host, int port, const std::string& method,
                              const std::string& path, const std::string& body,
                              Http2ResponseCallback callback) {
    connect_and_send(host, port, method, path, body, callback);
}

void Http2Client::connect_and_send(const std::string& host, int port, const std::string& method,
                                  const std::string& path, const std::string& body,
                                  Http2ResponseCallback callback) {
    // Store connection info for authority header
    current_host_ = host;
    current_port_ = port;
    
    socket_ = std::make_shared<tcp::socket>(io_context_);
    
    resolver_.async_resolve(host, std::to_string(port),
        [this, host, method, path, body, callback]
        (const boost::system::error_code& ec, tcp::resolver::results_type results) {
            if (ec) {
                callback(Http2Response(0, ""), "Failed to resolve host: " + ec.message());
                return;
            }
            handle_connect(ec, results, socket_, method, path, body, callback);
        });
}

void Http2Client::handle_connect(const boost::system::error_code& ec, tcp::resolver::results_type results,
                                std::shared_ptr<tcp::socket> socket, const std::string& method,
                                const std::string& path, const std::string& body, Http2ResponseCallback callback) {
    if (ec) {
        callback(Http2Response(0, ""), "Failed to resolve: " + ec.message());
        return;
    }
    
    boost::asio::async_connect(*socket, results,
        [this, socket, method, path, body, callback]
        (const boost::system::error_code& ec, const tcp::endpoint&) {
            if (ec) {
                callback(Http2Response(0, ""), "Failed to connect: " + ec.message());
                return;
            }
            
            // Setup HTTP/2 session
            setup_nghttp2_client();
            
            // Send HTTP/2 connection preface and request
            send_http2_request(method, path, body, callback);
            
            // Start reading responses
            read_data();
        });
}

void Http2Client::setup_nghttp2_client() {
    nghttp2_session_callbacks* callbacks;
    nghttp2_session_callbacks_new(&callbacks);
    
    nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, on_frame_recv_cb);
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, on_data_chunk_recv_cb);
    nghttp2_session_callbacks_set_on_stream_close_callback(callbacks, on_stream_close_cb);
    nghttp2_session_callbacks_set_on_header_callback(callbacks, on_header_cb);
    nghttp2_session_callbacks_set_send_callback(callbacks, send_cb);
    
    int rv = nghttp2_session_client_new(&session_, callbacks, this);
    nghttp2_session_callbacks_del(callbacks);
    
    if (rv != 0) {
        LOG_ERROR("Failed to create nghttp2 session: " << nghttp2_strerror(rv));
        return;
    }
    
    // Submit initial settings
    nghttp2_settings_entry settings[] = {
        {NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100},
        {NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE, 65535}
    };
    
    rv = nghttp2_submit_settings(session_, NGHTTP2_FLAG_NONE, settings, 2);
    if (rv != 0) {
        LOG_ERROR("Failed to submit settings: " << nghttp2_strerror(rv));
    }
    write_data();
    LOG_DEBUG("HTTP/2 client session initialized");
}

void Http2Client::send_http2_request(const std::string& method, const std::string& path, const std::string& body, Http2ResponseCallback callback) {
    std::vector<nghttp2_nv> headers;
    
    // Build authority header from stored host and port
    std::string authority = current_host_;
    if (current_port_ != 80 && current_port_ != 0) {
        authority += ":" + std::to_string(current_port_);
    }
    
    // Required pseudo-headers for HTTP/2
    std::string scheme = "http";
    headers.push_back(make_nv_ls(":method", method));
    headers.push_back(make_nv_ls(":path", path));
    headers.push_back(make_nv_ls(":scheme", scheme));
    headers.push_back(make_nv_ls(":authority", authority));
    
    // Content headers for POST/PUT requests with body
    std::string content_length_val;
    if (!body.empty()) {
        content_length_val = std::to_string(body.size());
        headers.push_back(make_nv_ls("content-length", content_length_val));
        headers.push_back(make_nv_ls("content-type", "application/json"));
    }
    
    // Prepare data provider for request body
    nghttp2_data_provider* data_prd = nullptr;
    std::shared_ptr<nghttp2_data_provider> data_provider_ptr;
    
    if (!body.empty()) {
        // Store the request body in the stream data for this request
        request_body_storage_ = body;
        
        data_provider_ptr = std::make_shared<nghttp2_data_provider>();
        data_provider_ptr->source.ptr = const_cast<char*>(request_body_storage_.c_str());
        data_provider_ptr->read_callback = [](nghttp2_session* /*session*/, int32_t stream_id,
                                       uint8_t* buf, size_t length, uint32_t* data_flags,
                                       nghttp2_data_source* source, void* user_data) -> ssize_t {
            Http2Client* client = static_cast<Http2Client*>(user_data);
            const char* body_data = static_cast<const char*>(source->ptr);
            size_t body_len = client->request_body_storage_.length();
            
            LOG_DEBUG("Data callback triggered for stream " << stream_id << ", body length: " << body_len);
            
            if (body_len == 0) {
                *data_flags |= NGHTTP2_DATA_FLAG_EOF;
                LOG_DEBUG("Data callback: sending 0 bytes with EOF flag");
                return 0;
            }
            
            size_t copy_len = std::min(length, body_len);
            memcpy(buf, body_data, copy_len);
            
            // Set EOF flag to indicate this is the complete body
            *data_flags |= NGHTTP2_DATA_FLAG_EOF;
            LOG_DEBUG("Data callback: sending " << copy_len << " bytes of request body with EOF flag");
            
            return static_cast<ssize_t>(copy_len);
        };
        data_prd = data_provider_ptr.get();
        
        // Store the data provider to keep it alive
        data_providers_[next_stream_id_] = data_provider_ptr;
    }
    
    // Submit the request
    int32_t stream_id = nghttp2_submit_request(session_, nullptr, headers.data(), headers.size(), data_prd, this);
    
    if (stream_id < 0) {
        LOG_ERROR("Failed to submit HTTP/2 request: " << nghttp2_strerror(stream_id));
        return;
    }
    
    LOG_DEBUG("Submitted HTTP/2 " << method << " request on stream " << stream_id);
    next_stream_id_ = stream_id + 2;  // Client stream IDs are odd
    
    // Store the callback for this stream
    streams_[stream_id].callback = callback;
    
    // Send the request immediately
    write_data();
}

void Http2Client::read_data() {
    socket_->async_read_some(boost::asio::buffer(read_buffer_),
        [this](const boost::system::error_code& ec, size_t bytes_transferred) {
            if (ec) {
                LOG_ERROR("Read error: " << ec.message());
                return;
            }
            
            ssize_t rv = nghttp2_session_mem_recv(session_, read_buffer_.data(), bytes_transferred);
            if (rv < 0) {
                LOG_ERROR("nghttp2_session_mem_recv error: " << nghttp2_strerror(static_cast<int>(rv)));
                return;
            }
            
            write_data();
            read_data();  // Continue reading
        });
}

void Http2Client::write_data() {
    LOG_DEBUG("Calling nghttp2_session_send");
    int rv = nghttp2_session_send(session_);
    if (rv != 0) {
        LOG_ERROR("nghttp2_session_send error: " << nghttp2_strerror(rv));
    } else {
        LOG_DEBUG("nghttp2_session_send completed successfully");
    }
}

// nghttp2 callbacks
int Http2Client::on_frame_recv_cb(nghttp2_session* /*session*/, const nghttp2_frame* frame, void* /*user_data*/) {
    const char* frame_name = "UNKNOWN";
    switch(frame->hd.type) {
        case NGHTTP2_DATA: frame_name = "DATA"; break;
        case NGHTTP2_HEADERS: frame_name = "HEADERS"; break;
        case NGHTTP2_PRIORITY: frame_name = "PRIORITY"; break;
        case NGHTTP2_RST_STREAM: frame_name = "RST_STREAM"; break;
        case NGHTTP2_SETTINGS: frame_name = "SETTINGS"; break;
        case NGHTTP2_PUSH_PROMISE: frame_name = "PUSH_PROMISE"; break;
        case NGHTTP2_PING: frame_name = "PING"; break;
        case NGHTTP2_GOAWAY: frame_name = "GOAWAY"; break;
        case NGHTTP2_WINDOW_UPDATE: frame_name = "WINDOW_UPDATE"; break;
    }
    
    LOG_DEBUG("Received " << frame_name << " frame on stream " << frame->hd.stream_id);
    
    if (frame->hd.type == NGHTTP2_HEADERS && frame->headers.cat == NGHTTP2_HCAT_RESPONSE) {
        LOG_DEBUG("Response headers received for stream " << frame->hd.stream_id);
    }
    
    if (frame->hd.type == NGHTTP2_GOAWAY) {
        LOG_ERROR("Server sent GOAWAY - connection will be closed");
    }
    
    return 0;
}

int Http2Client::on_data_chunk_recv_cb(nghttp2_session* /*session*/, uint8_t /*flags*/, int32_t stream_id,
                                     const uint8_t* data, size_t len, void* user_data) {
    Http2Client* client = static_cast<Http2Client*>(user_data);
    
    auto it = client->streams_.find(stream_id);
    if (it != client->streams_.end()) {
        it->second.body_buffer.append(reinterpret_cast<const char*>(data), len);
    }
    
    return 0;
}

int Http2Client::on_stream_close_cb(nghttp2_session* /*session*/, int32_t stream_id,
                                  uint32_t /*error_code*/, void* user_data) {
    Http2Client* client = static_cast<Http2Client*>(user_data);
    
    auto it = client->streams_.find(stream_id);
    if (it != client->streams_.end()) {
        StreamData& stream_data = it->second;
        stream_data.response.body = stream_data.body_buffer;
        
        // Call the callback with the response
        if (stream_data.callback) {
            stream_data.callback(stream_data.response, "");
        }
        
        client->streams_.erase(it);
    }
    
    // Clean up data provider for this stream
    client->data_providers_.erase(stream_id);
    
    return 0;
}

int Http2Client::on_header_cb(nghttp2_session* /*session*/, const nghttp2_frame* frame,
                            const uint8_t* name, size_t namelen, const uint8_t* value, size_t valuelen,
                            uint8_t /*flags*/, void* user_data) {
    Http2Client* client = static_cast<Http2Client*>(user_data);
    
    if (frame->hd.type == NGHTTP2_HEADERS && frame->headers.cat == NGHTTP2_HCAT_RESPONSE) {
        std::string header_name(reinterpret_cast<const char*>(name), namelen);
        std::string header_value(reinterpret_cast<const char*>(value), valuelen);
        
        auto it = client->streams_.find(frame->hd.stream_id);
        if (it != client->streams_.end()) {
            if (header_name == ":status") {
                it->second.response.status_code = std::stoi(header_value);
            }
        }
    }
    
    return 0;
}

ssize_t Http2Client::send_cb(nghttp2_session* /*session*/, const uint8_t* data, size_t length,
                           int /*flags*/, void* user_data) {
    Http2Client* client = static_cast<Http2Client*>(user_data);
    
    LOG_DEBUG("Sending " << length << " bytes to server");
    
    boost::system::error_code ec;
    size_t bytes_written = boost::asio::write(*client->socket_, boost::asio::buffer(data, length), ec);
    
    if (ec) {
        LOG_ERROR("Send error: " << ec.message());
        return NGHTTP2_ERR_CALLBACK_FAILURE;
    }
    
    LOG_DEBUG("Successfully sent " << bytes_written << " bytes");
    return static_cast<ssize_t>(bytes_written);
}