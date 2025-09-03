#include "active_request.hpp"
#include "../utils/logger.h"

ActiveRequest::ActiveRequest(uint64_t id, SocketPtr client_socket)
    : request_id_(id), state_(RequestState::Created), 
      start_time_(std::chrono::steady_clock::now()),
      client_socket_(std::move(client_socket)) {
    LOG_DEBUG("Created ActiveRequest " << request_id_);
}

ActiveRequest::~ActiveRequest() {
    LOG_DEBUG("Destroyed ActiveRequest " << request_id_);
}

void ActiveRequest::set_state(RequestState state) {
    state_ = state;
    LOG_DEBUG("Request " << request_id_ << " state: " << static_cast<int>(state));
}

void ActiveRequest::set_request(std::shared_ptr<RequestType> request) {
    request_ = std::move(request);
}

void ActiveRequest::set_response(std::shared_ptr<ResponseType> response) {
    response_ = std::move(response);
}

ActiveRequestManager& ActiveRequestManager::instance() {
    static ActiveRequestManager instance;
    return instance;
}

std::shared_ptr<ActiveRequest> ActiveRequestManager::create_request(std::shared_ptr<tcp::socket> client_socket) {
    uint64_t id = generate_request_id();
    auto request = std::make_shared<ActiveRequest>(id, std::move(client_socket));
    
    std::lock_guard<std::mutex> lock(mutex_);
    active_requests_[id] = request;
    
    LOG_DEBUG("Created request " << id << ", total active: " << active_requests_.size());
    return request;
}

std::shared_ptr<ActiveRequest> ActiveRequestManager::get_request(uint64_t request_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = active_requests_.find(request_id);
    return (it != active_requests_.end()) ? it->second : nullptr;
}

void ActiveRequestManager::complete_request(uint64_t request_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = active_requests_.find(request_id);
    if (it != active_requests_.end()) {
        it->second->set_state(RequestState::Completed);
        active_requests_.erase(it);
        LOG_DEBUG("Completed request " << request_id << ", remaining active: " << active_requests_.size());
    }
}

void ActiveRequestManager::cleanup_expired_requests() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(30);
    
    for (auto it = active_requests_.begin(); it != active_requests_.end();) {
        if (now - it->second->get_start_time() > timeout) {
            LOG_WARN("Cleaning up expired request " << it->second->get_id());
            it->second->set_state(RequestState::Failed);
            it = active_requests_.erase(it);
        } else {
            ++it;
        }
    }
}

size_t ActiveRequestManager::get_active_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_requests_.size();
}

void ActiveRequestManager::log_statistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    LOG_INFO("Active requests: " << active_requests_.size());
}

uint64_t ActiveRequestManager::generate_request_id() {
    return next_request_id_.fetch_add(1);
}