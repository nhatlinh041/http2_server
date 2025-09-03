#include "proxy_client.hpp"
#include "http_client.hpp"
#include "../utils/logger.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

ProxyClient::ProxyClient(boost::asio::io_context& io_context) 
    : io_context_(io_context), http_client_(std::make_unique<HttpClient>(io_context)) {}

ProxyClient::~ProxyClient() = default;

void ProxyClient::register_backend(const std::string& proxy_host, int proxy_port,
                                  const RegistrationRequest& request, RegistrationCallback callback) {
    json registration_data = {
        {"backend_id", request.backend_id},
        {"host", request.host},
        {"port", request.port},
        {"path_pattern", request.path_pattern}
    };
    
    std::string body = registration_data.dump();
    
    LOG_INFO("Registering backend " << request.backend_id << " with proxy at " << proxy_host << ":" << proxy_port);
    
    http_client_->send_request(proxy_host, proxy_port, "POST", "/proxy/register", body,
        [callback](const ProxyResponse& response, const std::string& error) {
            if (!error.empty()) {
                LOG_ERROR("Registration failed: " << error);
                callback(false, error);
                return;
            }
            
            if (response.status_code == 200) {
                LOG_INFO("Backend registered successfully");
                callback(true, "Registration successful");
            } else {
                LOG_WARN("Registration failed with status: " << response.status_code);
                callback(false, "Registration failed: " + response.body);
            }
        });
}

void ProxyClient::unregister_backend(const std::string& proxy_host, int proxy_port,
                                    const std::string& backend_id, RegistrationCallback callback) {
    json unregistration_data = {
        {"backend_id", backend_id}
    };
    
    std::string body = unregistration_data.dump();
    
    LOG_INFO("Unregistering backend " << backend_id << " from proxy at " << proxy_host << ":" << proxy_port);
    
    http_client_->send_request(proxy_host, proxy_port, "DELETE", "/proxy/register", body,
        [callback, backend_id](const ProxyResponse& response, const std::string& error) {
            if (!error.empty()) {
                LOG_ERROR("Unregistration failed: " << error);
                callback(false, error);
                return;
            }
            
            if (response.status_code == 200) {
                LOG_INFO("Backend unregistered successfully");
                callback(true, "Unregistration successful");
            } else {
                LOG_WARN("Unregistration failed with status: " << response.status_code);
                callback(false, "Unregistration failed: " + response.body);
            }
        });
}