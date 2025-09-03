HTTP/2 Proxy System Development Template
Using Claude Code

  Turn 1: Initial Project Setup
  Context:
  - Building enterprise-grade HTTP/2 server in C++
  - Need modern, maintainable codebase for proxy functionality
  - Target: Production-ready networking application

  Constraints:
  - Must use C++17 standard
  - Required dependencies: Boost.Asio, nghttp2, nlohmann/json
  - CMake build system mandatory
  - Linux development environment

  Guidelines:
  - Follow RAII principles for resource management
  - Use smart pointers (std::unique_ptr, std::shared_ptr)
  - Implement proper error handling with logging
  - Create modular, testable components

  Style & Code:
  - No comments unless absolutely necessary
  - Use modern C++ features (auto, range-based loops)
  - Consistent naming: snake_case for variables, PascalCase for classes
  - Header guards with #pragma once

  Completion Level:
  - CMakeLists.txt with all dependencies
  - Basic HTTP/2 server listening on configurable port
  - Session management with nghttp2 integration
  - Logging system with configurable levels
  - Build script with debug/release modes

  ---
  Turn 2: Request Handling Architecture

  Context:
  - Need extensible routing system for HTTP/2 server
  - Must support dynamic route registration
  - Prepare foundation for proxy functionality

  Constraints:
  - Use design patterns appropriately (Singleton for global handlers)
  - Thread-safe operations required
  - No blocking operations in async context

  Guidelines:
  - Single Responsibility: Each class handles one concern
  - Open/Closed: Easy to extend with new route types
  - Dependency Inversion: Depend on abstractions

  Style & Code:
  - std::function for callback types
  - std::string_view for string parameters (performance)
  - Const-correctness throughout
  - RAII for automatic cleanup

  Completion Level:
  - RequestHandler singleton with route registration
  - Type-safe callback system using std::function
  - Method/path routing with std::map storage
  - Integration with existing HTTP/2 session handling
  - Example route demonstrating the system

  ---
  Turn 3: Proxy Core Architecture

  Context:
  - Design ngrok-like proxy system
  - Backends must register dynamically
  - Client requests routed automatically

  Constraints:
  - Must follow SOLID principles strictly
  - Thread-safe concurrent access required
  - Extensible for future proxy features

  Guidelines:
  - Interface Segregation: Clean, focused interfaces
  - Liskov Substitution: Proper inheritance hierarchies
  - Single responsibility for each component

  Style & Code:
  - Forward declarations to minimize dependencies
  - std::shared_ptr for shared ownership
  - std::mutex for thread synchronization
  - Explicit constructors to prevent implicit conversions

  Completion Level:
  - BackendRegistry with thread-safe registration
  - ForwardingRule structure for backend metadata
  - ProxyRequestHandler as main coordinator
  - Clean separation between registration and forwarding
  - Path pattern matching algorithm

  ---
  Turn 4: HTTP Client Communication

  Context:
  - Proxy needs HTTP client for backend communication
  - Must handle async operations properly
  - Support connection pooling and error recovery

  Constraints:
  - Use Boost.Beast for HTTP/1.1 communication
  - All operations must be asynchronous
  - Proper error propagation required

  Guidelines:
  - Exception safety guarantee (basic/strong)
  - Resource cleanup on errors
  - Timeout handling for robustness

  Style & Code:
  - Callback-based async pattern
  - std::shared_ptr for async object lifetime
  - Beast::error_code for error handling
  - Template-friendly design

  Completion Level:
  - HttpClient with full async operation support
  - Connection management and cleanup
  - Error handling with proper error codes
  - Response parsing and header preservation
  - Integration with proxy forwarding system

  ---
  Turn 5: Server Integration & Routing

  Context:
  - Integrate proxy with existing HTTP/2 server
  - Add registration endpoints
  - Implement request routing logic

  Constraints:
  - Cannot break existing HTTP/2 functionality
  - Must maintain backward compatibility
  - Registration endpoints must be RESTful

  Guidelines:
  - Minimize changes to existing codebase
  - Use composition over inheritance
  - Clear separation of concerns

  Style & Code:
  - JSON for registration API (RESTful design)
  - Lambda functions for inline handlers
  - const std::string& for string parameters
  - Explicit error responses with proper HTTP codes

  Completion Level:
  - Registration routes (POST/DELETE /proxy/register)
  - JSON-based registration API
  - Request routing: registered routes vs proxy forwarding
  - Error handling with appropriate HTTP status codes
  - Server initialization with proxy components

  ---
  Turn 6: Client SDK Development

  Context:
  - Create developer-friendly SDK for backend registration
  - Must be easy to integrate into existing applications
  - Support both sync and async registration patterns

  Constraints:
  - Header-only or minimal dependency library
  - Platform-independent design
  - Clean API surface

  Guidelines:
  - Builder pattern for configuration
  - Callback-based async operations
  - Intuitive method naming

  Style & Code:
  - struct for configuration (simple data)
  - using aliases for callback types
  - std::function for flexibility
  - Move semantics for efficiency

  Completion Level:
  - ProxyClient with registration/unregistration
  - RegistrationRequest configuration structure
  - Callback-based async API
  - Error handling with success/failure callbacks
  - Example usage demonstrating integration

  ---
  Turn 7: Command-Line Interface

  Context:
  - Build ngrok-like CLI tool for tunnel management
  - Must provide real-time status display
  - Support command-line arguments and configuration

  Constraints:
  - Standard POSIX signal handling
  - Cross-platform compatibility
  - Intuitive command-line interface

  Guidelines:
  - Single binary for ease of deployment
  - Clear status output similar to ngrok
  - Graceful shutdown handling

  Style & Code:
  - argc/argv parsing with validation
  - Signal handlers for SIGINT/SIGTERM
  - iostream for user interface
  - std::chrono for timing operations

  Completion Level:
  - Command-line argument parsing
  - Ngrok-style status display with ASCII borders
  - Signal handling for graceful shutdown
  - Auto-generated tunnel IDs
  - Real-time tunnel status monitoring

  ---
  Turn 8: Build System Optimization

  Context:
  - Multiple executables with different purposes
  - Need flexible build options for development
  - Support different build configurations

  Constraints:
  - CMake best practices
  - Parallel build support
  - Clear documentation

  Guidelines:
  - Modular CMake targets
  - Conditional compilation options
  - Helper scripts for common tasks

  Style & Code:
  - Modern CMake (3.16+) syntax
  - target_* commands over global settings
  - find_package for dependency management
  - Clear variable naming

  Completion Level:
  - Multiple executable targets
  - Enhanced build script with options
  - Help documentation in build script
  - Dependency management with proper linking
  - Debug and release configurations

  ---
  Turn 9: Enterprise TLS Support

  Context:
  - Production deployment requires HTTPS
  - Browser compatibility essential
  - Security compliance needed

  Constraints:
  - OpenSSL integration required
  - ALPN negotiation for HTTP/2
  - Certificate management

  Guidelines:
  - Secure defaults
  - Configurable certificate paths
  - Environment-based configuration

  Style & Code:
  - boost::asio::ssl namespace usage
  - RAII for SSL context management
  - Exception safety for crypto operations
  - Clear separation of SSL and plain modes

  Completion Level:
  - SSL context configuration with certificates
  - ALPN negotiation for HTTP/2 over TLS
  - Dual-mode server (HTTP/HTTPS)
  - Environment variable configuration
  - Self-signed certificate generation

  ---
  Turn 10: Browser Compatibility Layer

  Context:
  - HTTP/2 over TLS has browser compatibility issues
  - Need fallback for development and testing
  - Maintain existing HTTP/2 functionality

  Constraints:
  - Cannot modify existing HTTP/2 code
  - Must run alongside existing server
  - Share backend registry between protocols

  Guidelines:
  - Protocol separation with bridge pattern
  - Shared state management
  - Independent server lifecycles

  Style & Code:
  - Boost.Beast for HTTP/1.1 implementation
  - Separate port for HTTP/1.1 proxy
  - namespace aliases for clarity
  - async operation chaining

  Completion Level:
  - HTTP/1.1 proxy server with Boost.Beast
  - Dual-server architecture (ports 8080/9080)
  - Shared BackendRegistry between protocols
  - Browser-compatible request handling
  - Request forwarding to registered backends

  ---
  Turn 11: Memory Safety & Lifecycle Management

  Context:
  - Async operations causing segmentation faults
  - Need robust memory management for production
  - Request/response lifetime issues in HTTP/1.1 proxy

  Constraints:
  - No memory leaks acceptable
  - Thread-safe operations required
  - Async object lifetime management

  Guidelines:
  - RAII throughout async operations
  - Shared ownership for async contexts
  - Automatic cleanup mechanisms

  Style & Code:
  - std::shared_ptr for async object lifetime
  - std::unique_ptr for single ownership
  - std::enable_shared_from_this pattern
  - std::atomic for thread-safe state

  Completion Level:
  - ActiveRequest class for lifecycle management
  - ActiveRequestManager with thread-safe tracking
  - Request state machine (Created→Completed)
  - Automatic cleanup and timeout handling
  - Memory leak prevention with RAII

  ---
  Turn 12: Process Communication Fix

  Context:
  - Forwarding client runs as separate process
  - Backend registration not visible to main server
  - Need network-based inter-process communication

  Constraints:
  - Cannot use shared memory
  - Must work across process boundaries
  - Maintain backward compatibility

  Guidelines:
  - Network communication over direct function calls
  - Error handling for network failures
  - Graceful degradation

  Style & Code:
  - HTTP requests for inter-process communication
  - JSON serialization for structured data
  - Error callbacks with detailed messages
  - Dual registration (both server types)

  Completion Level:
  - HTTP-based registration instead of direct calls
  - Registration with both HTTP/1.1 and HTTP/2 servers
  - Network error handling and retry logic
  - Status reporting for registration success/failure
  - Full browser compatibility with Django backends

  ---
  Final Project Features:

  - Dual Protocol Support: HTTP/2 (port 8080) + HTTP/1.1 (port 9080)
  - Memory Safe: ActiveRequest lifecycle management
  - Production Ready: TLS support, proper error handling
  - Scalable Architecture: SOLID principles, thread-safe operations