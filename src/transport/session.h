#pragma once

#include "common.h"
#include "../utils/logger.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <nghttp2/nghttp2.h>
#include <functional>



class Session : public std::enable_shared_from_this<Session> {
    public:
        explicit Session(boost::asio::ip::tcp::socket p_socket, RequestCB p_request_cb);
        explicit Session(boost::asio::ip::tcp::socket p_socket, boost::asio::ssl::context& ssl_context, RequestCB p_request_cb);
        ~Session();

        void start();
    private:
        // nghttp2 callbacks
        static int on_frame_recv_cb(nghttp2_session* session, const nghttp2_frame* frame,
                                     void* user_data);
        static int on_header_cb(nghttp2_session* session, const nghttp2_frame* frame,
                                 const uint8_t* name, size_t namelen, const uint8_t* value, size_t valuelen,
                                 uint8_t flags, void* user_data);
        static int on_data_chunk_recv_cb(nghttp2_session* session, uint8_t flags,
                                         int32_t stream_id, const uint8_t* data,
                                         size_t len, void* user_data);
        static int on_stream_close_cb(nghttp2_session* session, int32_t stream_id,
                                       uint32_t error_code, void* user_data);
        static ssize_t send_cb(nghttp2_session* session, const uint8_t* data,
                               size_t length, int flags, void* user_data);

        // Session management
        void setup_nghttp2();
        void read_data();
        void write_data();
        void send_response(int32_t p_stream_id, const HttpResponse& p_response);
        void handle_ssl_handshake();

        // Request handling
        struct StreamData
        {
            std::string method;
            std::string path;
            std::string body;
        };
        
    private:
        nghttp2_session* session_ = nullptr;
        std::unique_ptr<boost::asio::ip::tcp::socket> plain_socket_;
        std::unique_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> ssl_socket_;
        RequestCB request_cb_;
        std::array<uint8_t, 8192> read_buffer_;
        std::unordered_map<int32_t, StreamData> streams_data_;
        bool use_ssl_ = false;
};