#pragma once

#include "common.h"
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <memory>
#include <functional>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

struct ProxyResponse {
    int status_code;
    std::string body;
    std::map<std::string, std::string> headers;
    
    ProxyResponse(int code, const std::string& response_body) 
        : status_code(code), body(response_body) {}
};

using ProxyResponseCallback = std::function<void(const ProxyResponse&, const std::string& error)>;

class HttpClient {
public:
    explicit HttpClient(boost::asio::io_context& io_context);
    ~HttpClient() = default;
    
    void send_request(const std::string& host, int port, const std::string& method,
                     const std::string& path, const std::string& body,
                     ProxyResponseCallback callback);

private:
    void handle_resolve(const boost::system::error_code& ec,
                       tcp::resolver::results_type results,
                       std::shared_ptr<tcp::socket> socket,
                       std::shared_ptr<http::request<http::string_body>> req,
                       ProxyResponseCallback callback);
                       
    void handle_connect(const boost::system::error_code& ec,
                       tcp::resolver::results_type::endpoint_type endpoint,
                       std::shared_ptr<tcp::socket> socket,
                       std::shared_ptr<http::request<http::string_body>> req,
                       ProxyResponseCallback callback);
                       
    void handle_write(const boost::system::error_code& ec,
                     std::size_t bytes_transferred,
                     std::shared_ptr<tcp::socket> socket,
                     std::shared_ptr<http::request<http::string_body>> req,
                     ProxyResponseCallback callback);
                     
    void handle_read(const boost::system::error_code& ec,
                    std::size_t bytes_transferred,
                    std::shared_ptr<tcp::socket> socket,
                    std::shared_ptr<http::response<http::string_body>> res,
                    ProxyResponseCallback callback);

private:
    boost::asio::io_context& io_context_;
    tcp::resolver resolver_;
};