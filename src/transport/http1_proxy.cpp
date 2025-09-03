#include "http1_proxy.hpp"
#include "proxy_handler.hpp"
#include "../utils/logger.h"
#include <iostream>

Http1ProxySession::Http1ProxySession(tcp::socket socket) 
    : resolver_(socket.get_executor()) {
    auto socket_ptr = std::make_shared<tcp::socket>(std::move(socket));
    active_request_ = ActiveRequestManager::instance().create_request(socket_ptr);
    active_request_->set_buffer(std::make_shared<beast::flat_buffer>());
}

void Http1ProxySession::start() {
    read_request();
}

void Http1ProxySession::read_request() {
    auto self = shared_from_this();
    auto request = std::make_shared<http::request<http::string_body>>();
    
    active_request_->set_state(RequestState::Parsing);
    
    http::async_read(*active_request_->get_client_socket(), *active_request_->get_buffer(), *request,
        [self, request](beast::error_code ec, std::size_t bytes_transferred) {
            if (!ec) {
                self->active_request_->set_request(request);
                self->handle_request();
            } else {
                LOG_ERROR("HTTP/1.1 read error: " << ec.message());
                self->active_request_->set_state(RequestState::Failed);
                ActiveRequestManager::instance().complete_request(self->active_request_->get_id());
            }
        });
}

void Http1ProxySession::handle_request() {
    auto request = active_request_->get_request();
    if (!request) {
        LOG_ERROR("No request available for handling");
        return;
    }
    
    std::string method = std::string(request->method_string());
    std::string path = std::string(request->target());
    std::string body = request->body();
    
    LOG_INFO("HTTP/1.1 " << method << " " << path);
    
    // Handle registration requests directly
    if (path == "/proxy/register") {
        if (method == "POST" || method == "DELETE") {
            try {
                auto request_json = json::parse(body);
                
                if (method == "POST") {
                    std::string backend_id = request_json["backend_id"];
                    std::string host = request_json["host"];
                    int port = request_json["port"];
                    std::string path_pattern = request_json["path_pattern"];
                    
                    BackendRegistry::instance().register_backend(backend_id, host, port, path_pattern);
                    send_response(200, R"({"status":"success","message":"Backend registered"})");
                } else {
                    std::string backend_id = request_json["backend_id"];
                    BackendRegistry::instance().unregister_backend(backend_id);
                    send_response(200, R"({"status":"success","message":"Backend unregistered"})");
                }
                return;
            } catch (const std::exception& e) {
                send_response(400, R"({"error":"Invalid JSON"})");
                return;
            }
        }
    }
    
    // Forward other requests to registered backends
    forward_to_backend();
}

void Http1ProxySession::forward_to_backend() {
    auto request = active_request_->get_request();
    std::string path = std::string(request->target());
    auto rule = BackendRegistry::instance().find_backend(path);
    
    if (!rule) {
        send_response(404, R"({"error":"No backend found for this path"})");
        return;
    }
    
    LOG_INFO("Forwarding to backend: " << rule->target_host << ":" << rule->target_port);
    active_request_->set_state(RequestState::Forwarding);
    
    auto backend_socket = std::make_shared<tcp::socket>(active_request_->get_client_socket()->get_executor());
    active_request_->set_backend_socket(backend_socket);
    
    resolver_.async_resolve(rule->target_host, std::to_string(rule->target_port),
        [self = shared_from_this(), rule](beast::error_code ec, tcp::resolver::results_type results) {
            if (ec) {
                self->send_response(502, R"({"error":"Backend resolve failed"})");
                return;
            }
            
            beast::get_lowest_layer(*self->active_request_->get_backend_socket()).async_connect(*results,
                [self, rule](beast::error_code ec) {
                    if (ec) {
                        self->send_response(502, R"({"error":"Backend connection failed"})");
                        return;
                    }
                    
                    self->active_request_->set_state(RequestState::WaitingBackend);
                    
                    // Forward the request to backend
                    http::async_write(*self->active_request_->get_backend_socket(), *self->active_request_->get_request(),
                        [self](beast::error_code ec, std::size_t) {
                            if (ec) {
                                self->send_response(502, R"({"error":"Backend write failed"})");
                                return;
                            }
                            
                            // Read response from backend
                            auto response_buffer = std::make_shared<beast::flat_buffer>();
                            auto response = std::make_shared<http::response<http::string_body>>();
                            
                            http::async_read(*self->active_request_->get_backend_socket(), *response_buffer, *response,
                                [self, response_buffer, response](beast::error_code ec, std::size_t) {
                                    if (ec) {
                                        self->send_response(502, R"({"error":"Backend read failed"})");
                                        return;
                                    }
                                    
                                    // Store response in active request
                                    self->active_request_->set_response(response);
                                    self->active_request_->set_state(RequestState::SendingResponse);
                                    
                                    // Forward backend response to client
                                    http::async_write(*self->active_request_->get_client_socket(), *response,
                                        [self](beast::error_code ec, std::size_t) {
                                            if (ec) {
                                                LOG_ERROR("Client write error: " << ec.message());
                                                self->active_request_->set_state(RequestState::Failed);
                                            } else {
                                                self->active_request_->set_state(RequestState::Completed);
                                            }
                                            ActiveRequestManager::instance().complete_request(self->active_request_->get_id());
                                        });
                                });
                        });
                });
        });
}

void Http1ProxySession::send_response(int status_code, const std::string& body, const std::string& content_type) {
    auto request = active_request_->get_request();
    auto response = std::make_shared<http::response<http::string_body>>(static_cast<http::status>(status_code), request->version());
    
    response->set(http::field::server, "HTTP1-Proxy/1.0");
    response->set(http::field::content_type, content_type);
    response->body() = body;
    response->prepare_payload();
    
    active_request_->set_response(response);
    active_request_->set_state(RequestState::SendingResponse);
    
    http::async_write(*active_request_->get_client_socket(), *response,
        [self = shared_from_this()](beast::error_code ec, std::size_t) {
            if (ec) {
                LOG_ERROR("Response write error: " << ec.message());
                self->active_request_->set_state(RequestState::Failed);
            } else {
                self->active_request_->set_state(RequestState::Completed);
            }
            ActiveRequestManager::instance().complete_request(self->active_request_->get_id());
        });
}

Http1ProxyServer::Http1ProxyServer(net::io_context& io_context, int port)
    : io_context_(io_context), acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {}

void Http1ProxyServer::start() {
    LOG_INFO("Starting HTTP/1.1 proxy server");
    accept_connections();
}

void Http1ProxyServer::accept_connections() {
    acceptor_.async_accept(
        [this](beast::error_code ec, tcp::socket socket) {
            if (!ec) {
                LOG_INFO("HTTP/1.1 connection accepted");
                std::make_shared<Http1ProxySession>(std::move(socket))->start();
            } else {
                LOG_ERROR("HTTP/1.1 accept error: " << ec.message());
            }
            accept_connections();
        });
}