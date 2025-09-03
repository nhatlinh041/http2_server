#pragma once

#include <boost/asio.hpp>
#include <nghttp2/nghttp2.h>
#include <memory>
#include <functional>
#include <string>
#include <unordered_map>

#include "common.h"

using tcp = boost::asio::ip::tcp;

struct Http2Response {
    int status_code = 0;
    std::string body;
    
    Http2Response() = default;
    Http2Response(int code, const std::string& b) : status_code(code), body(b) {}
};

using Http2ResponseCallback = std::function<void(const Http2Response&, const std::string&)>;

class Http2Client {
public:
    explicit Http2Client(boost::asio::io_context& io_context);
    ~Http2Client();
    
    void send_request(const std::string& host, int port, const std::string& method,
                     const std::string& path, const std::string& body,
                     Http2ResponseCallback callback);

private:
    // nghttp2 callbacks
    static int on_frame_recv_cb(nghttp2_session* session, const nghttp2_frame* frame, void* user_data);
    static int on_data_chunk_recv_cb(nghttp2_session* session, uint8_t flags, int32_t stream_id,
                                   const uint8_t* data, size_t len, void* user_data);
    static int on_stream_close_cb(nghttp2_session* session, int32_t stream_id,
                                uint32_t error_code, void* user_data);
    static int on_header_cb(nghttp2_session* session, const nghttp2_frame* frame,
                          const uint8_t* name, size_t namelen, const uint8_t* value, size_t valuelen,
                          uint8_t flags, void* user_data);
    static ssize_t send_cb(nghttp2_session* session, const uint8_t* data, size_t length,
                          int flags, void* user_data);

    // Connection management
    void connect_and_send(const std::string& host, int port, const std::string& method,
                         const std::string& path, const std::string& body,
                         Http2ResponseCallback callback);
    void handle_connect(const boost::system::error_code& ec, tcp::resolver::results_type results,
                       std::shared_ptr<tcp::socket> socket, const std::string& method,
                       const std::string& path, const std::string& body, Http2ResponseCallback callback);
    void setup_nghttp2_client();
    void send_http2_request(const std::string& method, const std::string& path, const std::string& body, Http2ResponseCallback callback);
    void read_data();
    void write_data();

    // Stream data management
    struct StreamData {
        Http2ResponseCallback callback;
        Http2Response response;
        std::string body_buffer;
    };

    boost::asio::io_context& io_context_;
    tcp::resolver resolver_;
    std::shared_ptr<tcp::socket> socket_;
    nghttp2_session* session_ = nullptr;
    std::array<uint8_t, 8192> read_buffer_;
    std::unordered_map<int32_t, StreamData> streams_;
    std::unordered_map<int32_t, std::shared_ptr<nghttp2_data_provider>> data_providers_;
    std::string request_body_storage_;
    std::string current_host_;
    int current_port_ = 0;
    int32_t next_stream_id_ = 1;
};