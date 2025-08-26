#include "server.h"
#include "../utils/logger.h"
#include "transport/session.h"

using boost::asio::ip::tcp;

Server::Server(boost::asio::io_context& p_io_context, short p_port)
                : acceptor_(p_io_context, tcp::endpoint(tcp::v4(), p_port)),
                  io_context_(p_io_context) {

}

void Server::start() {
    LOG_INFO("Starting server");
    accept_connection();
}

void Server::set_request_handler(RequestCB handler) {
    request_handler_ = std::move(handler);
}

void Server::accept_connection() {
    acceptor_.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
        if (!ec) {
            LOG_INFO("New connection accepted");
            std::make_shared<Session>(std::move(socket), request_handler_)->start();
        } else {
            LOG_ERROR("Accept error: " << ec.message());
        }
        accept_connection();
    });
}