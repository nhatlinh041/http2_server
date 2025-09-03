#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

enum class RequestState {
    Created,
    Parsing,
    Forwarding,
    WaitingBackend,
    SendingResponse,
    Completed,
    Failed
};

class ActiveRequest : public std::enable_shared_from_this<ActiveRequest> {
public:
    using RequestType = http::request<http::string_body>;
    using ResponseType = http::response<http::string_body>;
    using SocketPtr = std::shared_ptr<tcp::socket>;
    using BufferPtr = std::shared_ptr<beast::flat_buffer>;
    
    explicit ActiveRequest(uint64_t id, SocketPtr client_socket);
    ~ActiveRequest();
    
    uint64_t get_id() const { return request_id_; }
    RequestState get_state() const { return state_; }
    void set_state(RequestState state);
    
    // Request handling
    void set_request(std::shared_ptr<RequestType> request);
    std::shared_ptr<RequestType> get_request() const { return request_; }
    
    // Response handling  
    void set_response(std::shared_ptr<ResponseType> response);
    std::shared_ptr<ResponseType> get_response() const { return response_; }
    
    // Socket management
    SocketPtr get_client_socket() const { return client_socket_; }
    void set_backend_socket(SocketPtr backend_socket) { backend_socket_ = backend_socket; }
    SocketPtr get_backend_socket() const { return backend_socket_; }
    
    // Buffer management
    void set_buffer(BufferPtr buffer) { buffer_ = buffer; }
    BufferPtr get_buffer() const { return buffer_; }
    
    // Timing
    std::chrono::steady_clock::time_point get_start_time() const { return start_time_; }
    
private:
    uint64_t request_id_;
    std::atomic<RequestState> state_;
    std::chrono::steady_clock::time_point start_time_;
    
    SocketPtr client_socket_;
    SocketPtr backend_socket_;
    BufferPtr buffer_;
    
    std::shared_ptr<RequestType> request_;
    std::shared_ptr<ResponseType> response_;
};

class ActiveRequestManager {
public:
    static ActiveRequestManager& instance();
    
    std::shared_ptr<ActiveRequest> create_request(std::shared_ptr<tcp::socket> client_socket);
    std::shared_ptr<ActiveRequest> get_request(uint64_t request_id);
    void complete_request(uint64_t request_id);
    void cleanup_expired_requests();
    
    size_t get_active_count() const;
    void log_statistics() const;

private:
    ActiveRequestManager() = default;
    uint64_t generate_request_id();
    
    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, std::shared_ptr<ActiveRequest>> active_requests_;
    std::atomic<uint64_t> next_request_id_{1};
};