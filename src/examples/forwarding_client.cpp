#include "forwarding_client.hpp"
#include "../transport/http_client.hpp"
#include "../transport/http2_client.hpp"
#include "../utils/logger.h"
#include <iostream>
#include <iomanip>
#include <signal.h>
#include <chrono>
#include <thread>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static ForwardingClient* g_client = nullptr;

ForwardingClient::ForwardingClient(boost::asio::io_context& io_context) 
    : io_context_(io_context), 
      http_client_(std::make_unique<HttpClient>(io_context)),
      http2_client_(std::make_unique<Http2Client>(io_context)) {
    g_client = this;
}

ForwardingClient::~ForwardingClient() {
    if (is_tunnel_active_) {
        std::cout << "\nShutting down tunnel..." << std::endl;
        stop_tunnel();
    }
}


void ForwardingClient::start_tunnel(const TunnelConfig& config) {
    active_tunnel_ = std::make_unique<TunnelConfig>(config);
    
    // Register based on protocol preference
    switch (config.protocol) {
        case RegistrationProtocol::HTTP1_ONLY:
            register_with_server(config, 9080);  // HTTP/1.1 proxy only
            break;
        case RegistrationProtocol::HTTP2_ONLY:
            register_with_server(config, 8080);  // HTTP/2 server only
            break;
        case RegistrationProtocol::BOTH:
        default:
            register_with_server(config, 9080);  // HTTP/1.1 proxy
            register_with_server(config, 8080);  // HTTP/2 server
            break;
    }
    
    is_tunnel_active_ = true;
    
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "Forwarding Client Started" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    display_status();
    std::cout << std::string(60, '=') << std::endl;
    std::cout << "\nPress Ctrl+C to stop forwarding...\n" << std::endl;
}

void ForwardingClient::register_with_server(const TunnelConfig& config, int port) {
    json registration_data = {
        {"backend_id", config.tunnel_id},
        {"host", config.local_host},
        {"port", config.local_port},
        {"path_pattern", config.path_pattern}
    };
    
    std::string body = registration_data.dump();
    
    if (port == 8080) {
        // Use HTTP/2 client for HTTP/2 server
        http2_client_->send_request(config.proxy_host, port, "POST", "/proxy/register", body,
            [port, tunnel_id = config.tunnel_id](const Http2Response& response, const std::string& error) {
                if (!error.empty()) {
                    std::cout << "Registration failed on port " << port << ": " << error << std::endl;
                    return;
                }
                
                if (response.status_code == 200) {
                    std::cout << "✓ Backend registered on port " << port << " (HTTP/2)" << std::endl;
                } else {
                    std::cout << "Registration failed on port " << port << " (HTTP " << response.status_code << "): " << response.body << std::endl;
                }
            });
    } else {
        // Use HTTP/1.1 client for HTTP/1.1 server
        http_client_->send_request(config.proxy_host, port, "POST", "/proxy/register", body,
            [port, tunnel_id = config.tunnel_id](const ProxyResponse& response, const std::string& error) {
                if (!error.empty()) {
                    std::cout << "Registration failed on port " << port << ": " << error << std::endl;
                    return;
                }
                
                if (response.status_code == 200) {
                    std::cout << "✓ Backend registered on port " << port << " (HTTP/1.1)" << std::endl;
                } else {
                    std::cout << "Registration failed on port " << port << " (HTTP " << response.status_code << "): " << response.body << std::endl;
                }
            });
    }
}

void ForwardingClient::stop_tunnel() {
    LOG_DEBUG("1 Perform stop tunnel id "<< active_tunnel_->tunnel_id);
    if (active_tunnel_ && is_tunnel_active_) {
        LOG_DEBUG("2 Perform stop tunnel id "<< active_tunnel_->tunnel_id);
        
        // Unregister based on protocol preference
        switch (active_tunnel_->protocol) {
            case RegistrationProtocol::HTTP1_ONLY:
                unregister_from_server(*active_tunnel_, 9080);  // HTTP/1.1 proxy only
                break;
            case RegistrationProtocol::HTTP2_ONLY:
                unregister_from_server(*active_tunnel_, 8080);  // HTTP/2 server only
                break;
            case RegistrationProtocol::BOTH:
            default:
                unregister_from_server(*active_tunnel_, 9080);  // HTTP/1.1 proxy
                unregister_from_server(*active_tunnel_, 8080);  // HTTP/2 server
                break;
        }
        
        is_tunnel_active_ = false;
        LOG_DEBUG("Tunnel stopped.");
    }
}

void ForwardingClient::unregister_from_server(const TunnelConfig& config, int port) {
    json unregistration_data = {
        {"backend_id", config.tunnel_id}
    };
    
    std::string body = unregistration_data.dump();
    LOG_DEBUG("Sending Unregister with data: " << body);
    
    if (port == 8080) {
        // Use HTTP/2 client for HTTP/2 server
        http2_client_->send_request(config.proxy_host, port, "DELETE", "/proxy/register", body,
            [port](const Http2Response& response, const std::string& error) {
                if (!error.empty()) {
                    std::cout << "Unregistration failed on port " << port << ": " << error << std::endl;
                    return;
                }
                
                if (response.status_code == 200) {
                    std::cout << "✓ Backend unregistered from port " << port << " (HTTP/2)" << std::endl;
                }
            });
    } else {
        // Use HTTP/1.1 client for HTTP/1.1 server
        http_client_->send_request(config.proxy_host, port, "DELETE", "/proxy/register", body,
            [port](const ProxyResponse& response, const std::string& error) {
                if (!error.empty()) {
                    std::cout << "Unregistration failed on port " << port << ": " << error << std::endl;
                    return;
                }
                
                if (response.status_code == 200) {
                    std::cout << "✓ Backend unregistered from port " << port << " (HTTP/1.1)" << std::endl;
                }
            });
    }
}

void ForwardingClient::display_status() const {
    if (!active_tunnel_) {
        std::cout << "No active tunnel" << std::endl;
        return;
    }
    
    std::cout << "Tunnel ID:     " << active_tunnel_->tunnel_id << std::endl;
    std::cout << "Local Backend: " << active_tunnel_->local_host << ":" << active_tunnel_->local_port << std::endl;
    std::cout << "Status:        " << (is_tunnel_active_ ? "Active" : "Inactive") << std::endl;
    
    // Show protocol-specific URLs
    std::cout << "\nPublic URLs:" << std::endl;
    switch (active_tunnel_->protocol) {
        case RegistrationProtocol::HTTP1_ONLY:
            std::cout << "  HTTP/1.1:    http://" << active_tunnel_->proxy_host << ":9080" << active_tunnel_->path_pattern << " (HTTP/1.1 only)" << std::endl;
            break;
        case RegistrationProtocol::HTTP2_ONLY:
            std::cout << "  HTTP/2:      http://" << active_tunnel_->proxy_host << ":8080" << active_tunnel_->path_pattern << " (HTTP/2 only)" << std::endl;
            break;
        case RegistrationProtocol::BOTH:
        default:
            std::cout << "  HTTP/2:      http://" << active_tunnel_->proxy_host << ":8080" << active_tunnel_->path_pattern << std::endl;
            std::cout << "  HTTP/1.1:    http://" << active_tunnel_->proxy_host << ":9080" << active_tunnel_->path_pattern << std::endl;
            break;
    }
}