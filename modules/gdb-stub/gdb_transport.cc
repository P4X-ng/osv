/*
 * Copyright (C) 2024 OSv Contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include "gdb_transport.hh"
#include "gdb_stub.hh"
#include <osv/debug.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <cstring>

namespace gdb_stub {

// TCP Transport Implementation
tcp_transport::tcp_transport(int port)
    : port_(port)
    , server_socket_(-1)
    , client_socket_(-1)
    , connected_(false)
{
}

tcp_transport::~tcp_transport()
{
    shutdown();
}

bool tcp_transport::initialize()
{
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0) {
        debug("gdb-stub: Failed to create TCP socket: %s\n", strerror(errno));
        return false;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        debug("gdb-stub: Failed to set socket options: %s\n", strerror(errno));
        close(server_socket_);
        server_socket_ = -1;
        return false;
    }
    
    // Bind to port
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);
    
    if (bind(server_socket_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        debug("gdb-stub: Failed to bind to port %d: %s\n", port_, strerror(errno));
        close(server_socket_);
        server_socket_ = -1;
        return false;
    }
    
    // Listen for connections
    if (listen(server_socket_, 1) < 0) {
        debug("gdb-stub: Failed to listen on socket: %s\n", strerror(errno));
        close(server_socket_);
        server_socket_ = -1;
        return false;
    }
    
    debug("gdb-stub: TCP transport listening on port %d\n", port_);
    return true;
}

void tcp_transport::shutdown()
{
    close_client_connection();
    
    if (server_socket_ >= 0) {
        close(server_socket_);
        server_socket_ = -1;
    }
}

bool tcp_transport::is_connected()
{
    return connected_ && client_socket_ >= 0;
}

bool tcp_transport::wait_for_connection()
{
    if (server_socket_ < 0) {
        return false;
    }
    
    debug("gdb-stub: Waiting for GDB connection on port %d\n", port_);
    
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    client_socket_ = accept(server_socket_, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
    if (client_socket_ < 0) {
        debug("gdb-stub: Failed to accept connection: %s\n", strerror(errno));
        return false;
    }
    
    connected_ = true;
    debug("gdb-stub: GDB client connected from %s\n", inet_ntoa(client_addr.sin_addr));
    return true;
}

ssize_t tcp_transport::read(void* buffer, size_t size)
{
    if (!is_connected()) {
        return -1;
    }
    
    ssize_t bytes_read = recv(client_socket_, buffer, size, 0);
    if (bytes_read <= 0) {
        if (bytes_read == 0) {
            debug("gdb-stub: Client disconnected\n");
        } else {
            debug("gdb-stub: Read error: %s\n", strerror(errno));
        }
        close_client_connection();
    }
    
    return bytes_read;
}

ssize_t tcp_transport::write(const void* buffer, size_t size)
{
    if (!is_connected()) {
        return -1;
    }
    
    ssize_t bytes_written = send(client_socket_, buffer, size, 0);
    if (bytes_written < 0) {
        debug("gdb-stub: Write error: %s\n", strerror(errno));
        close_client_connection();
    }
    
    return bytes_written;
}

void tcp_transport::close_client_connection()
{
    if (client_socket_ >= 0) {
        close(client_socket_);
        client_socket_ = -1;
    }
    connected_ = false;
}

// Serial Transport Implementation
serial_transport::serial_transport(const std::string& device)
    : device_path_(device)
    , serial_fd_(-1)
    , connected_(false)
{
}

serial_transport::~serial_transport()
{
    shutdown();
}

bool serial_transport::initialize()
{
    serial_fd_ = open(device_path_.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (serial_fd_ < 0) {
        debug("gdb-stub: Failed to open serial device %s: %s\n", 
              device_path_.c_str(), strerror(errno));
        return false;
    }
    
    if (!configure_serial_port()) {
        close(serial_fd_);
        serial_fd_ = -1;
        return false;
    }
    
    connected_ = true;
    debug("gdb-stub: Serial transport initialized on %s\n", device_path_.c_str());
    return true;
}

void serial_transport::shutdown()
{
    if (serial_fd_ >= 0) {
        close(serial_fd_);
        serial_fd_ = -1;
    }
    connected_ = false;
}

bool serial_transport::is_connected()
{
    return connected_ && serial_fd_ >= 0;
}

bool serial_transport::wait_for_connection()
{
    // Serial is always "connected" once initialized
    return is_connected();
}

ssize_t serial_transport::read(void* buffer, size_t size)
{
    if (!is_connected()) {
        return -1;
    }
    
    return ::read(serial_fd_, buffer, size);
}

ssize_t serial_transport::write(const void* buffer, size_t size)
{
    if (!is_connected()) {
        return -1;
    }
    
    return ::write(serial_fd_, buffer, size);
}

bool serial_transport::configure_serial_port()
{
    struct termios tty;
    
    if (tcgetattr(serial_fd_, &tty) != 0) {
        debug("gdb-stub: Failed to get serial attributes: %s\n", strerror(errno));
        return false;
    }
    
    // Configure for raw mode
    cfmakeraw(&tty);
    
    // Set baud rate to 115200
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);
    
    // 8N1 configuration
    tty.c_cflag &= ~PARENB;   // No parity
    tty.c_cflag &= ~CSTOPB;   // One stop bit
    tty.c_cflag &= ~CSIZE;    // Clear size bits
    tty.c_cflag |= CS8;       // 8 data bits
    
    // No flow control
    tty.c_cflag &= ~CRTSCTS;
    
    // Enable receiver
    tty.c_cflag |= CREAD | CLOCAL;
    
    // Apply settings
    if (tcsetattr(serial_fd_, TCSANOW, &tty) != 0) {
        debug("gdb-stub: Failed to set serial attributes: %s\n", strerror(errno));
        return false;
    }
    
    return true;
}

// Factory functions
std::unique_ptr<transport> create_tcp_transport(int port)
{
    return std::make_unique<tcp_transport>(port);
}

std::unique_ptr<transport> create_serial_transport(const std::string& device)
{
    return std::make_unique<serial_transport>(device);
}

std::unique_ptr<transport> create_vsock_transport(int port)
{
    // VSock implementation would go here
    // For now, return nullptr as it's not implemented
    debug("gdb-stub: VSock transport not yet implemented\n");
    return nullptr;
}

} // namespace gdb_stub