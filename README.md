# HTTP/2 Proxy Server (Similar to ngrok)

This is a proxy server implementation that allows backend services to register themselves with the proxy and have client requests forwarded to them automatically.

## Architecture

1. **BackendRegistry**: Manages registered backend services using the Singleton pattern
2. **ForwardingHandler**: Handles request forwarding to appropriate backends
3. **ProxyRequestHandler**: Main entry point for proxy functionality
4. **HttpClient**: HTTP client for making requests to backend services
5. **ProxyClient**: Client library for backend registration

## Key Features

- **Backend Registration**: Backends can register themselves with specific path patterns
- **Request Forwarding**: Automatic forwarding of client requests to registered backends
- **Path Matching**: Simple path pattern matching for routing
- **Error Handling**: Proper error responses when backends are unavailable
- **C++17 Compliant**: Uses modern C++ features and follows OOP principles

## Building
run the script
scripts/build.sh

This creates two executables:
- `http2-boost-server`: The main proxy server
- `forwarding_client`: Client for creating tunnels to local backends

## Usage

### 1. Start the Proxy Server

```bash
./http2-boost-server
```

The server starts with dual protocol support:
- **HTTP/2 server**: Port 8080 (configurable via `PORT` environment variable)  
- **HTTP/1.1 proxy**: Port 9080 (configurable via `HTTP1_PORT` environment variable)

Backends register with both protocols automatically for maximum compatibility.

### 2. Register a Backend

Backends register by sending a POST request to `/proxy/register`:

```json
{
    "backend_id": "service-1",
    "host": "localhost", 
    "port": 3000,
    "path_pattern": "/api/"
}
```

### 3. Using the ProxyClient API

```cpp
#include "transport/proxy_client.hpp"

boost::asio::io_context io_context;
ProxyClient client(io_context);

RegistrationRequest request("backend-1", "localhost", 3000, "/api/");
client.register_backend("localhost", 8080, request,
    [](bool success, const std::string& message) {
        if (success) {
            LOG_INFO("Registration successful: " << message);
        } else {
            LOG_ERROR("Registration failed: " << message);
        }
    });
```

### 4. Forwarding Client

Run the forwarding client to create a tunnel:

```bash
./forwarding_client --path /api/ 3000
```

This creates a public tunnel from the proxy to your local service running on port 3000.

### 5. Client Requests

Once backends are registered, clients can access them via either protocol:

**HTTP/2 clients (native):**
```bash
curl --http2 http://localhost:8080/api/users
```

**HTTP/1.1 clients (browsers):**
```bash
curl http://localhost:9080/api/users
```

Both requests route to the same registered backend service automatically.

## Registration API

### POST /proxy/register
Register a new backend service.

**Request:**
```json
{
    "backend_id": "unique-service-id",
    "host": "backend-hostname",
    "port": 3000,
    "path_pattern": "/api/v1/"
}
```

**Response:**
```json
{
    "status": "success",
    "backend_id": "unique-service-id",
    "message": "Backend registered successfully"
}
```

### DELETE /proxy/register
Unregister a backend service.

**Request:**
```json
{
    "backend_id": "unique-service-id"
}
```

**Response:**
```json
{
    "status": "success", 
    "backend_id": "unique-service-id",
    "message": "Backend unregistered successfully"
}
```

## Design Principles

The proxy system follows SOLID principles:

- **Single Responsibility**: Each class has a focused responsibility
- **Open/Closed**: Extensible through inheritance and composition
- **Liskov Substitution**: Proper inheritance hierarchies
- **Interface Segregation**: Clean, focused interfaces
- **Dependency Inversion**: Dependencies on abstractions, not concretions

## Error Handling

- **404**: No backend found for requested path
- **502**: Backend request failed (connection error, timeout)
- **400**: Invalid registration data
- **405**: Method not allowed for registration endpoints
- **500**: Proxy initialization errors