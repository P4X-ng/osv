/*
 * Copyright (C) 2024 OSv Contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef GDB_TRANSPORT_HH
#define GDB_TRANSPORT_HH

#include "gdb_stub.hh"
#include <string>
#include <memory>

namespace gdb_stub {

// TCP transport implementation
class tcp_transport : public transport {
private:
    int port_;
    int server_socket_;
    int client_socket_;
    bool connected_;
    
public:
    tcp_transport(int port);
    virtual ~tcp_transport();
    
    virtual bool initialize() override;
    virtual void shutdown() override;
    virtual bool is_connected() override;
    virtual ssize_t read(void* buffer, size_t size) override;
    virtual ssize_t write(const void* buffer, size_t size) override;
    virtual bool wait_for_connection() override;
    
private:
    void close_client_connection();
};

// Serial transport implementation
class serial_transport : public transport {
private:
    std::string device_path_;
    int serial_fd_;
    bool connected_;
    
public:
    serial_transport(const std::string& device);
    virtual ~serial_transport();
    
    virtual bool initialize() override;
    virtual void shutdown() override;
    virtual bool is_connected() override;
    virtual ssize_t read(void* buffer, size_t size) override;
    virtual ssize_t write(const void* buffer, size_t size) override;
    virtual bool wait_for_connection() override;
    
private:
    bool configure_serial_port();
};

// VSock transport implementation
class vsock_transport : public transport {
private:
    int port_;
    int server_socket_;
    int client_socket_;
    bool connected_;
    
public:
    vsock_transport(int port);
    virtual ~vsock_transport();
    
    virtual bool initialize() override;
    virtual void shutdown() override;
    virtual bool is_connected() override;
    virtual ssize_t read(void* buffer, size_t size) override;
    virtual ssize_t write(const void* buffer, size_t size) override;
    virtual bool wait_for_connection() override;
    
private:
    void close_client_connection();
};

} // namespace gdb_stub

#endif // GDB_TRANSPORT_HH