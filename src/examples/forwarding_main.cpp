#include "forwarding_client.hpp"
#include "../utils/logger.h"
#include <boost/asio.hpp>
#include <iostream>
#include <thread>

void print_usage() {
    std::cout << "Usage: forwarding_client [OPTIONS] <local_port>\n";
    std::cout << "Options:\n";
    std::cout << "  --host <host>      Local backend host (default: localhost)\n";
    std::cout << "  --proxy <host>     Proxy server host (default: localhost)\n";
    std::cout << "  --proxy-port <port> Proxy server port (default: 8080)\n";
    std::cout << "  --path <pattern>   Path pattern to forward (default: /)\n";
    std::cout << "  --protocol <proto> Registration protocol: http1, http2, or both (default: both)\n";
    std::cout << "  -h, --help         Show this help\n";
    std::cout << "\nExamples:\n";
    std::cout << "  forwarding_client 9999                    # Register with both HTTP/1.1 and HTTP/2\n";
    std::cout << "  forwarding_client --protocol http1 9999   # Register only with HTTP/1.1 server\n";
    std::cout << "  forwarding_client --protocol http2 9999   # Register only with HTTP/2 server\n";
    std::cout << "  forwarding_client --path /api/ 3000       # Register with custom path pattern\n";
}

int main(int argc, char* argv[]) {
    try {
        Logger::instance().set_level(LogLevel::Info);
        
        TunnelConfig config;
        
        // Parse command line arguments
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            
            if (arg == "-h" || arg == "--help") {
                print_usage();
                return 0;
            }
            else if (arg == "--host" && i + 1 < argc) {
                config.local_host = argv[++i];
            }
            else if (arg == "--proxy" && i + 1 < argc) {
                config.proxy_host = argv[++i];
            }
            else if (arg == "--proxy-port" && i + 1 < argc) {
                config.proxy_port = std::stoi(argv[++i]);
            }
            else if (arg == "--path" && i + 1 < argc) {
                config.path_pattern = argv[++i];
            }
            else if (arg == "--protocol" && i + 1 < argc) {
                std::string proto = argv[++i];
                if (proto == "http1") {
                    config.protocol = RegistrationProtocol::HTTP1_ONLY;
                } else if (proto == "http2") {
                    config.protocol = RegistrationProtocol::HTTP2_ONLY;
                } else if (proto == "both") {
                    config.protocol = RegistrationProtocol::BOTH;
                } else {
                    std::cerr << "Invalid protocol: " << proto << ". Use http1, http2, or both" << std::endl;
                    return 1;
                }
            }
            else if (arg[0] != '-') {
                config.local_port = std::stoi(arg);
            }
            else {
                std::cerr << "Unknown option: " << arg << std::endl;
                print_usage();
                return 1;
            }
        }
        
        boost::asio::io_context io_context;
        ForwardingClient client(io_context);
        
        client.start_tunnel(config);
        
        // Keep running until signal
        std::thread io_thread([&io_context]() {
            io_context.run();
        });
        
        // Wait indefinitely until Ctrl+C
        while (client.is_tunnel_active()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        io_context.stop();
        if (io_thread.joinable()) {
            io_thread.join();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}