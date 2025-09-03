#include "core/server.h"
#include "transport/request_handler.h"
#include "transport/proxy_handler.hpp"
#include "transport/http1_proxy.hpp"
#include "transport/session.h"
#include "utils/logger.h"
#include <boost/asio.hpp>
#include <iostream>
#include <signal.h>
#include <cstdlib>

boost::asio::io_context* g_io_context = nullptr;

void signal_handler(int signum) {
    LOG_INFO("Received signal " << signum << ", shutting down...");
    if (g_io_context) {
        g_io_context->stop();
    }
}

int main() {
    try {
        // Setup signal handling
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        
        // Configure logging
        Logger::instance().set_level(LogLevel::Debug);
        
        // Get configuration from environment
        int port = std::getenv("PORT") ? std::stoi(std::getenv("PORT")) : 8080;
        int http1_port = std::getenv("HTTP1_PORT") ? std::stoi(std::getenv("HTTP1_PORT")) : 9080;
        int threads = std::getenv("THREADS") ? std::stoi(std::getenv("THREADS")) : 4;
        bool use_ssl = std::getenv("USE_SSL") ? (std::string(std::getenv("USE_SSL")) == "1") : false;
        std::string cert_file = std::getenv("CERT_FILE") ? std::getenv("CERT_FILE") : "certs/server.crt";
        std::string key_file = std::getenv("KEY_FILE") ? std::getenv("KEY_FILE") : "certs/server.key";
        
        LOG_INFO("Starting HTTP/2 Server");
        LOG_INFO("HTTP/2 Port: " << port << ", HTTP/1.1 Port: " << http1_port << ", Threads: " << threads);
        LOG_INFO("SSL: " << (use_ssl ? "Enabled" : "Disabled"));
        
        // Create io_context
        boost::asio::io_context io_context(threads);
        g_io_context = &io_context;
        
        // Initialize proxy handler
        auto& proxy_handler = ProxyRequestHandler::instance();
        proxy_handler.initialize(io_context);
        
        // Register custom routes
        auto& handler = RequestHandler::instance();
        handler.register_route("GET", "/test", [](std::string_view p_method, std::string_view p_path, std::string_view p_body, 
                                                    int32_t p_stream_id, ResponseSender p_sender) {
            LOG_INFO("Processing GET /test service");
            p_sender(p_stream_id, HttpResponse(204, ""));
        });
        
        // Register proxy routes
        handler.register_route("POST", "/proxy/register", [&proxy_handler](std::string_view p_method, std::string_view p_path, std::string_view p_body, 
                                                    int32_t p_stream_id, ResponseSender p_sender) {
            proxy_handler.handle_registration_request(p_method, p_path, p_body, p_stream_id, p_sender);
        });
        
        handler.register_route("DELETE", "/proxy/register", [&proxy_handler](std::string_view p_method, std::string_view p_path, std::string_view p_body, 
                                                    int32_t p_stream_id, ResponseSender p_sender) {
            proxy_handler.handle_registration_request(p_method, p_path, p_body, p_stream_id, p_sender);
        });

        // Create server
        Server server = use_ssl ? 
            Server(io_context, port, cert_file, key_file) :
            Server(io_context, port);
        server.set_request_handler([&proxy_handler](std::string_view method, std::string_view path, std::string_view body, 
                                    int32_t stream_id, std::function<void(int32_t, const HttpResponse&)> sender) {
            // Try registered routes first
            if (path == "/test" || path == "/proxy/register") {
                RequestHandler::instance().handle_request(method, path, body, stream_id, sender);
            } else {
                // Forward to proxy for backend lookup
                proxy_handler.handle_proxy_request(method, path, body, stream_id, sender);
            }
        });
        
        // Create HTTP/1.1 proxy server for browser access
        Http1ProxyServer http1_proxy(io_context, http1_port);
        
        // Start both servers
        server.start();
        http1_proxy.start();
        
        // Run with multiple threads
        std::vector<std::thread> thread_pool;
        thread_pool.reserve(threads);
        
        for (int i = 0; i < threads - 1; ++i) {
            thread_pool.emplace_back([&io_context]() {
                io_context.run();
            });
        }
        
        LOG_INFO("HTTP/2 server ready on port " << port);
        LOG_INFO("HTTP/1.1 proxy ready on port " << http1_port << " (for browsers)");
        
        // Run on main thread
        io_context.run();
        
        // Wait for all threads
        for (auto& t : thread_pool) {
            if (t.joinable()) {
                t.join();
            }
        }
        
        LOG_INFO("Server shutdown complete");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Server error: " << e.what());
        return 1;
    }
    
    return 0;
}