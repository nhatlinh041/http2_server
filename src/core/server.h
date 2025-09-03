#pragma once

#include "../transport/common.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <memory>

class Session;

class Server {
public:
    explicit Server(boost::asio::io_context& p_io_context, short p_port);
    explicit Server(boost::asio::io_context& p_io_context, short p_port, 
                   const std::string& cert_file, const std::string& key_file);
    ~Server() = default;

    void start();
    void set_request_handler(RequestCB p_handler);
    bool is_ssl_enabled() const { return use_ssl_; }
private:
    void accept_connection();
    void setup_ssl_context(const std::string& cert_file, const std::string& key_file);

private:
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::io_context& io_context_;
    RequestCB request_handler_;
    std::unique_ptr<boost::asio::ssl::context> ssl_context_;
    bool use_ssl_ = false;
};