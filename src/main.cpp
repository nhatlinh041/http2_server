#include "core/server.h"
#include "transport/request_handler.h"
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
        Logger::instance().set_level(LogLevel::Info);
        
        // Get configuration from environment
        int port = std::getenv("PORT") ? std::stoi(std::getenv("PORT")) : 8080;
        int threads = std::getenv("THREADS") ? std::stoi(std::getenv("THREADS")) : 4;
        
        LOG_INFO("Starting HTTP/2 Server");
        LOG_INFO("Port: " << port << ", Threads: " << threads);
        
        // Create io_context
        boost::asio::io_context io_context(threads);
        g_io_context = &io_context;
        
        // Register custom routes
        auto& handler = RequestHandler::instance();
        handler.register_route("GET", "/test", [](std::string_view p_method, std::string_view p_path, std::string_view p_body, int32_t p_stream_id, ResponseSender p_sender) {
            p_sender(p_stream_id, HttpResponse(204, ""));
        });

        // Create server
        Server server(io_context, port);
        server.set_request_handler([](std::string_view method, std::string_view path, std::string_view body, int32_t stream_id, std::function<void(int32_t, const HttpResponse&)> sender) {
            RequestHandler::instance().handle_request(method, path, body, stream_id, sender);
        });
        
        // Start server
        server.start();
        
        // Run with multiple threads
        std::vector<std::thread> thread_pool;
        thread_pool.reserve(threads);
        
        for (int i = 0; i < threads - 1; ++i) {
            thread_pool.emplace_back([&io_context]() {
                io_context.run();
            });
        }
        
        LOG_INFO("Server ready and listening on port " << port);
        
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