#pragma once

#include <string>
#include <functional>
#include <memory>
#include <boost/asio.hpp>

struct ProxyResponse;

struct RegistrationRequest {
    std::string backend_id;
    std::string host;
    int port;
    std::string path_pattern;
    
    RegistrationRequest(const std::string& id, const std::string& h, int p, const std::string& pattern)
        : backend_id(id), host(h), port(p), path_pattern(pattern) {}
};

using RegistrationCallback = std::function<void(bool success, const std::string& message)>;

class HttpClient;

class ProxyClient {
public:
    explicit ProxyClient(boost::asio::io_context& io_context);
    ~ProxyClient();
    
    void register_backend(const std::string& proxy_host, int proxy_port,
                         const RegistrationRequest& request, RegistrationCallback callback);
    
    void unregister_backend(const std::string& proxy_host, int proxy_port,
                           const std::string& backend_id, RegistrationCallback callback);

private:
    boost::asio::io_context& io_context_;
    std::unique_ptr<HttpClient> http_client_;
};