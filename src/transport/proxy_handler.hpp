#pragma once

#include "common.h"
#include <boost/asio.hpp>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <functional>

struct ForwardingRule {
    std::string backend_id;
    std::string target_host;
    int target_port;
    std::string path_pattern;
    
    ForwardingRule(const std::string& id, const std::string& host, int port, const std::string& pattern)
        : backend_id(id), target_host(host), target_port(port), path_pattern(pattern) {}
};

class BackendRegistry {
public:
    static BackendRegistry& instance();
    
    void register_backend(const std::string& backend_id, const std::string& host, int port, const std::string& path_pattern);
    void unregister_backend(const std::string& backend_id);
    std::shared_ptr<ForwardingRule> find_backend(std::string_view path) const;
    
private:
    BackendRegistry() = default;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<ForwardingRule>> backends_;
};

class HttpClient;

class ForwardingHandler {
public:
    explicit ForwardingHandler(boost::asio::io_context& io_context);
    
    void forward_request(std::string_view method, std::string_view path, std::string_view body,
                        int32_t stream_id, ResponseSender sender);

private:
    bool matches_pattern(std::string_view path, std::string_view pattern) const;
    
    boost::asio::io_context& io_context_;
    std::unique_ptr<HttpClient> http_client_;
};

class ProxyRequestHandler {
public:
    static ProxyRequestHandler& instance();
    
    void initialize(boost::asio::io_context& io_context);
    void handle_proxy_request(std::string_view method, std::string_view path, std::string_view body,
                             int32_t stream_id, ResponseSender sender);
    void handle_registration_request(std::string_view method, std::string_view path, std::string_view body,
                                   int32_t stream_id, ResponseSender sender);

private:
    ProxyRequestHandler() = default;
    std::unique_ptr<ForwardingHandler> forwarding_handler_;
    boost::asio::io_context* io_context_ = nullptr;
};