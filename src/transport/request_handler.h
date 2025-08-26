#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <map>

#include "utils/logger.h"
#include "common.h"

struct HttpResponse;

class RequestHandler {
    public:
        static RequestHandler& instance(); // singleton instance

        void register_route(std::string_view p_method,
                            std::string_view p_path,
                            RequestCB p_callback);
        void handle_request(std::string_view p_method,
                                    std::string_view p_path,
                                    std::string_view p_body,
                                    int32_t p_stream_id,
                                    ResponseSender p_sender);
        static json create_error_response(int code, const std::string& message);
        static json create_success_response(const json& data);
    private:
        RequestHandler() = default; // private constructor for singleton
        void handle_default_routes(std::string_view p_method,
                                            std::string_view p_path,
                                            std::string_view p_body,
                                            int32_t stream_id,
                                            ResponseSender sender);
    
    private:
        std::map<RouteKey, RequestCB> route_handlers_;
};