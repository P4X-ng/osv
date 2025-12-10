/*
 * Copyright (C) 2024 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef VIRTIO_VSOCK_DRIVER_H
#define VIRTIO_VSOCK_DRIVER_H

#include "drivers/virtio.hh"
#include "drivers/virtio-device.hh"
#include <osv/vsock.hh>
#include <osv/mutex.h>
#include <osv/waitqueue.hh>
#include <list>
#include <memory>

namespace virtio {

class vsock : public virtio_driver {
public:
    struct vsock_config {
        uint64_t guest_cid;
    } __attribute__((packed));

    explicit vsock(virtio_device& dev);
    virtual ~vsock();

    virtual std::string get_name() const override { return "virtio-vsock"; }
    virtual void read_config() override;

    // VSock operations
    int send_packet(const virtio_vsock_hdr& hdr, const void* data, size_t len);
    void recv_packet();
    
    // Configuration
    uint64_t get_guest_cid() const { return _guest_cid; }

    // Driver probe function
    static hw_driver* probe(hw_device* dev);

    // Queue management
    static const int RX_QUEUE = 0;
    static const int TX_QUEUE = 1;
    static const int EVENT_QUEUE = 2;

private:
    void setup_queues();
    void fill_rx_ring();
    void handle_rx();
    void handle_tx();
    void handle_event();
    
    // Configuration
    uint64_t _guest_cid;
    
    // Packet buffers
    struct rx_buffer {
        void* data;
        size_t len;
        virtio_vsock_hdr hdr;
    };
    
    std::list<rx_buffer> _rx_buffers;
    mutex _rx_mutex;
    waitqueue _rx_wq;
    
    // Statistics
    uint64_t _tx_packets;
    uint64_t _rx_packets;
    uint64_t _tx_bytes;
    uint64_t _rx_bytes;
};

// Global function for protocol layer access
vsock* get_vsock_driver();

}

#endif /* VIRTIO_VSOCK_DRIVER_H */