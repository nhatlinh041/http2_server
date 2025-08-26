#include "request_handler.h"
#include "../utils/logger.h"

#include <chrono>


RequestHandler& RequestHandler::instance() {
    static RequestHandler instance_;
    return instance_;
}

void RequestHandler::register_route(std::string_view p_method,
                                    std::string_view p_path,
                                    RequestCB p_callback) {
    RouteKey key = {p_method.data(), p_path.data()};
    route_handlers_[key] = p_callback;
    LOG_INFO("Registered route: " << p_method << " " << p_path);
}

void RequestHandler::handle_request(std::string_view p_method,
                                    std::string_view p_path,
                                    std::string_view p_body,
                                    int32_t p_stream_id,
                                    ResponseSender p_sender) {
    LOG_INFO("Processing " << p_method << " " << p_path);
    try
    {
        RouteKey key = {p_method.data(), p_path.data()};
        auto it = route_handlers_.find(key);
        if (it != route_handlers_.end()) {
            LOG_INFO("Found route: " << p_method << " " << p_path);
            it->second(p_method, p_path, p_body, p_stream_id, p_sender);
            return;
        }
        LOG_WARN("No route found for: " << p_method << " " << p_path << ", using default handler");
        handle_default_routes(p_method, p_path, p_body, p_stream_id, p_sender);
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        json error = create_error_response(500, "Internal server error");
        p_sender(p_stream_id, HttpResponse(500, error.dump()));
    }
    
}

json RequestHandler::create_error_response(int code, const std::string& message)
{
    return {
        {"error", true},
        {"code", code},
        {"message", message}
    };
}

json RequestHandler::create_success_response(const json& data)
{
    json response = {
        {"success", true}
    };
    
    for (auto& [key, value] : data.items()) {
        response[key] = value;
    }
    
    return response;
}

void RequestHandler::handle_default_routes(std::string_view p_method,
                                            std::string_view p_path,
                                            std::string_view p_body,
                                            int32_t stream_id,
                                            ResponseSender sender) {
    if (p_method == "GET" && p_path == "/health") {
        json response = {
            {"status", "ok"}
        };
        sender(stream_id, HttpResponse(200, response.dump()));
    }
    else {
        json error = create_error_response(404, "Route not found");
        sender(stream_id, HttpResponse(404, error.dump()));
    }
}