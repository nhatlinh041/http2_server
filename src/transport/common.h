#pragma once

#include <nlohmann/json.hpp>
#include <nghttp2/nghttp2.h>
#include <functional>
#include <string>
using json = nlohmann::json;

struct HttpResponse {
    int status_code = 200;
    std::string body;
    std::string content_type = "application/json";
    
    HttpResponse() = default;
    HttpResponse(int status, const std::string& response_body, const std::string& type = "application/json") 
        : status_code(status), body(response_body), content_type(type) {}
};

using ResponseSender = std::function<void(int32_t stream_id, const HttpResponse& response)>;
using RequestCB = std::function<void(std::string_view p_method,
                                            std::string_view p_path,
                                            std::string_view p_body,
                                            int32_t p_stream_id,
                                            ResponseSender p_sender)>;
using RouteKey = std::pair<std::string, std::string>; // {method, path}

template <size_t N>
nghttp2_nv make_nv_ls(const char (&name)[N], const std::string& value) {
    return {(uint8_t*)name, (uint8_t*)value.c_str(), (uint16_t)(N - 1),
            (uint16_t)value.size(), NGHTTP2_NV_FLAG_NONE};
}