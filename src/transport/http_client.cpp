#include "http_client.hpp"
#include "../utils/logger.h"

HttpClient::HttpClient(boost::asio::io_context& io_context) 
    : io_context_(io_context), resolver_(io_context) {}

void HttpClient::send_request(const std::string& host, int port, const std::string& method,
                             const std::string& path, const std::string& body,
                             ProxyResponseCallback callback) {
    auto req = std::make_shared<http::request<http::string_body>>();
    req->version(11);
    req->method(http::string_to_verb(method));
    req->target(path);
    req->set(http::field::host, host);
    req->set(http::field::user_agent, "Proxy/1.0");
    
    if (!body.empty()) {
        req->body() = body;
        req->set(http::field::content_length, std::to_string(body.size()));
        req->set(http::field::content_type, "application/json");
    }
    
    req->prepare_payload();
    
    auto socket = std::make_shared<tcp::socket>(io_context_);
    
    resolver_.async_resolve(host, std::to_string(port),
        [this, socket, req, callback](const boost::system::error_code& ec, tcp::resolver::results_type results) {
            handle_resolve(ec, results, socket, req, callback);
        });
}

void HttpClient::handle_resolve(const boost::system::error_code& ec,
                               tcp::resolver::results_type results,
                               std::shared_ptr<tcp::socket> socket,
                               std::shared_ptr<http::request<http::string_body>> req,
                               ProxyResponseCallback callback) {
    if (ec) {
        callback(ProxyResponse(0, ""), "Failed to resolve host: " + ec.message());
        return;
    }

    beast::get_lowest_layer(*socket).async_connect(
        results->endpoint(),
        [this, socket, req, callback, results](const boost::system::error_code& ec) {
            handle_connect(ec, results->endpoint(), socket, req, callback);
        });
}

void HttpClient::handle_connect(const boost::system::error_code& ec,
                               tcp::resolver::results_type::endpoint_type endpoint,
                               std::shared_ptr<tcp::socket> socket,
                               std::shared_ptr<http::request<http::string_body>> req,
                               ProxyResponseCallback callback) {
    if (ec) {
        callback(ProxyResponse(0, ""), "Failed to connect: " + ec.message());
        return;
    }

    http::async_write(*socket, *req,
        [this, socket, req, callback](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            handle_write(ec, bytes_transferred, socket, req, callback);
        });
}

void HttpClient::handle_write(const boost::system::error_code& ec,
                             std::size_t bytes_transferred,
                             std::shared_ptr<tcp::socket> socket,
                             std::shared_ptr<http::request<http::string_body>> req,
                             ProxyResponseCallback callback) {
    if (ec) {
        callback(ProxyResponse(0, ""), "Failed to write request: " + ec.message());
        return;
    }

    auto buffer = std::make_shared<beast::flat_buffer>();
    auto res = std::make_shared<http::response<http::string_body>>();

    http::async_read(*socket, *buffer, *res,
        [this, socket, res, callback, buffer](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            handle_read(ec, bytes_transferred, socket, res, callback);
        });
}

void HttpClient::handle_read(const boost::system::error_code& ec,
                            std::size_t bytes_transferred,
                            std::shared_ptr<tcp::socket> socket,
                            std::shared_ptr<http::response<http::string_body>> res,
                            ProxyResponseCallback callback) {
    if (ec) {
        callback(ProxyResponse(0, ""), "Failed to read response: " + ec.message());
        return;
    }

    ProxyResponse proxy_response(static_cast<int>(res->result_int()), res->body());
    
    for (const auto& header : *res) {
        proxy_response.headers[std::string(header.name_string())] = std::string(header.value());
    }
    
    callback(proxy_response, "");
}