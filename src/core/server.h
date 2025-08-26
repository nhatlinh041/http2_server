#pragma once

#include "../transport/common.h"

#include <boost/asio.hpp>
#include <memory>

class Session;

class Server {
public:
    explicit Server(boost::asio::io_context& p_io_context, short p_port);
    ~Server() = default;

    void start();
    void set_request_handler(RequestCB p_handler);
private:
    void accept_connection();

private:
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::io_context& io_context_;
    RequestCB request_handler_;
};