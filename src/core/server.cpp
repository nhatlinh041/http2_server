#include "server.h"
#include "../utils/logger.h"
#include "transport/session.h"

using boost::asio::ip::tcp;
namespace ssl = boost::asio::ssl;

Server::Server(boost::asio::io_context& p_io_context, short p_port)
                : acceptor_(p_io_context, tcp::endpoint(tcp::v4(), p_port)),
                  io_context_(p_io_context), use_ssl_(false) {
}

Server::Server(boost::asio::io_context& p_io_context, short p_port, 
               const std::string& cert_file, const std::string& key_file)
                : acceptor_(p_io_context, tcp::endpoint(tcp::v4(), p_port)),
                  io_context_(p_io_context), use_ssl_(true) {
    setup_ssl_context(cert_file, key_file);
}

void Server::start() {
    LOG_INFO("Starting server");
    accept_connection();
}

void Server::set_request_handler(RequestCB handler) {
    request_handler_ = std::move(handler);
}

void Server::setup_ssl_context(const std::string& cert_file, const std::string& key_file) {
    ssl_context_ = std::make_unique<ssl::context>(ssl::context::tlsv12);
    
    ssl_context_->set_options(
        ssl::context::default_workarounds |
        ssl::context::no_sslv2 |
        ssl::context::no_sslv3 |
        ssl::context::single_dh_use
    );
    
    ssl_context_->use_certificate_chain_file(cert_file);
    ssl_context_->use_private_key_file(key_file, ssl::context::pem);
    
    // Set ALPN for HTTP/2
    SSL_CTX_set_alpn_select_cb(ssl_context_->native_handle(), 
        [](SSL* ssl, const unsigned char** out, unsigned char* outlen,
           const unsigned char* in, unsigned int inlen, void* arg) -> int {
            const unsigned char h2[] = "\x02h2";
            if (SSL_select_next_proto((unsigned char**)out, outlen, h2, sizeof(h2) - 1, in, inlen) != OPENSSL_NPN_NEGOTIATED) {
                return SSL_TLSEXT_ERR_NOACK;
            }
            return SSL_TLSEXT_ERR_OK;
        }, nullptr);
    
    LOG_INFO("SSL context configured with certificate: " << cert_file);
}

void Server::accept_connection() {
    acceptor_.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
        if (!ec) {
            LOG_INFO("New connection accepted");
            if (use_ssl_) {
                std::make_shared<Session>(std::move(socket), *ssl_context_, request_handler_)->start();
            } else {
                std::make_shared<Session>(std::move(socket), request_handler_)->start();
            }
        } else {
            LOG_ERROR("Accept error: " << ec.message());
        }
        accept_connection();
    });
}