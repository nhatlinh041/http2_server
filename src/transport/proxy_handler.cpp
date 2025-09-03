#include "proxy_handler.hpp"
#include "http_client.hpp"
#include "../utils/logger.h"
#include <algorithm>

BackendRegistry& BackendRegistry::instance() {
    static BackendRegistry instance;
    return instance;
}

void BackendRegistry::register_backend(const std::string& backend_id, const std::string& host, 
                                      int port, const std::string& path_pattern) {
    std::lock_guard<std::mutex> lock(mutex_);
    backends_[backend_id] = std::make_shared<ForwardingRule>(backend_id, host, port, path_pattern);
    LOG_INFO("Registered backend: " << backend_id << " -> " << host << ":" << port << " pattern: " << path_pattern);
}

void BackendRegistry::unregister_backend(const std::string& backend_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = backends_.find(backend_id);
    if (it != backends_.end()) {
        LOG_INFO("Unregistered backend: " << backend_id);
        backends_.erase(it);
    }
}

std::shared_ptr<ForwardingRule> BackendRegistry::find_backend(std::string_view path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& [id, rule] : backends_) {
        if (path.find(rule->path_pattern) == 0) {
            return rule;
        }
    }
    return nullptr;
}

ForwardingHandler::ForwardingHandler(boost::asio::io_context& io_context) 
    : io_context_(io_context), http_client_(std::make_unique<HttpClient>(io_context)) {}

void ForwardingHandler::forward_request(std::string_view method, std::string_view path, 
                                       std::string_view body, int32_t stream_id, ResponseSender sender) {
    auto rule = BackendRegistry::instance().find_backend(path);
    if (!rule) {
        LOG_WARN("No backend found for path: " << path);
        sender(stream_id, HttpResponse(404, R"({"error": "No backend found for this path"})"));
        return;
    }

    LOG_INFO("Forwarding request to backend: " << rule->backend_id << " at " << rule->target_host << ":" << rule->target_port);
    
    http_client_->send_request(rule->target_host, rule->target_port, 
                              std::string(method), std::string(path), std::string(body),
                              [sender, stream_id](const ProxyResponse& response, const std::string& error) {
        if (!error.empty()) {
            LOG_ERROR("Backend request failed: " << error);
            sender(stream_id, HttpResponse(502, R"({"error": "Backend request failed"})"));
            return;
        }
        
        LOG_INFO("Backend response: " << response.status_code);
        sender(stream_id, HttpResponse(response.status_code, response.body));
    });
}

bool ForwardingHandler::matches_pattern(std::string_view path, std::string_view pattern) const {
    return path.find(pattern) == 0;
}

ProxyRequestHandler& ProxyRequestHandler::instance() {
    static ProxyRequestHandler instance;
    return instance;
}

void ProxyRequestHandler::initialize(boost::asio::io_context& io_context) {
    io_context_ = &io_context;
    forwarding_handler_ = std::make_unique<ForwardingHandler>(io_context);
}

void ProxyRequestHandler::handle_proxy_request(std::string_view method, std::string_view path, 
                                              std::string_view body, int32_t stream_id, ResponseSender sender) {
    if (!forwarding_handler_) {
        sender(stream_id, HttpResponse(500, R"({"error": "Proxy not initialized"})"));
        return;
    }
    
    if (path.substr(0, 15) == "/proxy/register") {
        handle_registration_request(method, path, body, stream_id, sender);
        return;
    }
    
    forwarding_handler_->forward_request(method, path, body, stream_id, sender);
}

void ProxyRequestHandler::handle_registration_request(std::string_view method, std::string_view path, 
                                                    std::string_view body, int32_t stream_id, ResponseSender sender) {
    try {
        auto request_json = json::parse(body);
        
        if (method == "POST") {
            std::string backend_id = request_json["backend_id"];
            std::string host = request_json["host"];
            int port = request_json["port"];
            std::string path_pattern = request_json["path_pattern"];
            
            BackendRegistry::instance().register_backend(backend_id, host, port, path_pattern);
            
            json response = {
                {"status", "success"},
                {"backend_id", backend_id},
                {"message", "Backend registered successfully"}
            };
            
            sender(stream_id, HttpResponse(200, response.dump()));
        } 
        else if (method == "DELETE") {
            std::string backend_id = request_json["backend_id"];
            
            BackendRegistry::instance().unregister_backend(backend_id);
            
            json response = {
                {"status", "success"},
                {"backend_id", backend_id},
                {"message", "Backend unregistered successfully"}
            };
            
            sender(stream_id, HttpResponse(200, response.dump()));
        }
        else {
            sender(stream_id, HttpResponse(405, R"({"error": "Method not allowed"})"));
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Registration operation failed: " << e.what());
        sender(stream_id, HttpResponse(400, R"({"error": "Invalid request data"})"));
    }
}