#pragma once

#include "active_request.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <memory>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

class Http1ProxySession : public std::enable_shared_from_this<Http1ProxySession> {
public:
    explicit Http1ProxySession(tcp::socket socket);
    void start();

private:
    void read_request();
    void handle_request();
    void forward_to_backend();
    void send_response(int status_code, const std::string& body, const std::string& content_type = "application/json");

    std::shared_ptr<ActiveRequest> active_request_;
    tcp::resolver resolver_;
};

class Http1ProxyServer {
public:
    explicit Http1ProxyServer(net::io_context& io_context, int port);
    void start();

private:
    void accept_connections();

    net::io_context& io_context_;
    tcp::acceptor acceptor_;
};