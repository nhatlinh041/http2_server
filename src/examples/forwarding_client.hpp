#pragma once

#include <boost/asio.hpp>
#include <string>
#include <memory>
#include <random>

class HttpClient;
class Http2Client;

enum class RegistrationProtocol {
    HTTP1_ONLY,
    HTTP2_ONLY, 
    BOTH
};

struct TunnelConfig {
    std::string local_host = "localhost";
    int local_port = 9999;
    std::string proxy_host = "192.168.80.132";
    int proxy_port = 9080;
    std::string path_pattern = "/";
    std::string tunnel_id;
    RegistrationProtocol protocol = RegistrationProtocol::BOTH;
    
    TunnelConfig() {
        tunnel_id = generate_tunnel_id();
    }
    
    TunnelConfig(int port, const std::string& pattern = "/") 
        : local_port(port), path_pattern(pattern) {
        tunnel_id = generate_tunnel_id();
    }

private:
    std::string generate_tunnel_id() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1000, 9999);
        return "tunnel-" + std::to_string(dis(gen));
    }
};

class ForwardingClient {
public:
    explicit ForwardingClient(boost::asio::io_context& io_context);
    ~ForwardingClient();
    
    void start_tunnel(const TunnelConfig& config);
    void stop_tunnel();
    void display_status() const;
    bool is_tunnel_active() const { return is_tunnel_active_; }
    
private:
    void register_with_server(const TunnelConfig& config, int port);
    void unregister_from_server(const TunnelConfig& config, int port);
    
    boost::asio::io_context& io_context_;
    std::unique_ptr<TunnelConfig> active_tunnel_;
    std::unique_ptr<HttpClient> http_client_;
    std::unique_ptr<Http2Client> http2_client_;
    bool is_tunnel_active_ = false;
};