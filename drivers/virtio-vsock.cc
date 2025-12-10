/*
 * Copyright (C) 2024 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include "drivers/virtio-vsock.hh"
#include "drivers/virtio-vring.hh"
#include <osv/mempool.hh>
#include <osv/mmu.hh>
#include <osv/debug.hh>
#include <string.h>

using namespace memory;

namespace virtio {

static vsock* s_vsock_driver = nullptr;

vsock::vsock(virtio_device& dev)
    : virtio_driver(dev)
    , _guest_cid(0)
    , _tx_packets(0)
    , _rx_packets(0)
    , _tx_bytes(0)
    , _rx_bytes(0)
{
    _driver_name = "virtio-vsock";
    virtio_i("VIRTIO VSOCK INSTANCE");
    
    // Read configuration
    read_config();
    
    // Setup queues
    setup_queues();
    
    // Fill RX ring with buffers
    fill_rx_ring();
    
    // Set global driver instance
    s_vsock_driver = this;
    
    // Enable device
    add_dev_status(VIRTIO_CONFIG_S_DRIVER_OK);
}

vsock::~vsock()
{
    s_vsock_driver = nullptr;
}

void vsock::read_config()
{
    virtio_conf_read(offsetof(vsock_config, guest_cid), &_guest_cid, sizeof(_guest_cid));
    virtio_i("Guest CID: %llu", _guest_cid);
}

void vsock::setup_queues()
{
    // Setup RX queue
    auto rx_vq = get_virt_queue(RX_QUEUE);
    if (!rx_vq) {
        virtio_e("Failed to get RX queue");
        return;
    }
    
    rx_vq->set_handler([this] { handle_rx(); });
    
    // Setup TX queue  
    auto tx_vq = get_virt_queue(TX_QUEUE);
    if (!tx_vq) {
        virtio_e("Failed to get TX queue");
        return;
    }
    
    tx_vq->set_handler([this] { handle_tx(); });
    
    // Setup event queue
    auto event_vq = get_virt_queue(EVENT_QUEUE);
    if (!event_vq) {
        virtio_e("Failed to get event queue");
        return;
    }
    
    event_vq->set_handler([this] { handle_event(); });
}

void vsock::fill_rx_ring()
{
    auto rx_vq = get_virt_queue(RX_QUEUE);
    if (!rx_vq) {
        return;
    }
    
    // Fill RX ring with buffers
    for (int i = 0; i < rx_vq->size(); i++) {
        rx_buffer buf;
        buf.len = 4096; // Standard buffer size
        buf.data = memory::alloc_page();
        if (!buf.data) {
            virtio_e("Failed to allocate RX buffer");
            break;
        }
        
        // Add buffer to ring
        vring_desc* desc = rx_vq->alloc_desc();
        if (!desc) {
            memory::free_page(buf.data);
            break;
        }
        
        desc->addr = mmu::virt_to_phys(buf.data);
        desc->len = buf.len;
        desc->flags = VRING_DESC_F_WRITE;
        
        rx_vq->add_buf_wait(desc, 1, 0, buf.data);
        _rx_buffers.push_back(buf);
    }
    
    rx_vq->kick();
}

void vsock::handle_rx()
{
    auto rx_vq = get_virt_queue(RX_QUEUE);
    if (!rx_vq) {
        return;
    }
    
    vring_used_elem* used;
    void* cookie;
    
    while ((used = rx_vq->get_buf_elem(&cookie)) != nullptr) {
        // Process received packet
        virtio_vsock_hdr* hdr = static_cast<virtio_vsock_hdr*>(cookie);
        
        virtio_d("RX packet: src_cid=%llu, dst_cid=%llu, src_port=%u, dst_port=%u, op=%u, len=%u",
                hdr->src_cid, hdr->dst_cid, hdr->src_port, hdr->dst_port, hdr->op, hdr->len);
        
        _rx_packets++;
        _rx_bytes += used->len;
        
        // TODO: Forward packet to protocol layer
        
        // Refill buffer
        rx_buffer buf;
        buf.len = 4096;
        buf.data = memory::alloc_page();
        if (buf.data) {
            vring_desc* desc = rx_vq->alloc_desc();
            if (desc) {
                desc->addr = mmu::virt_to_phys(buf.data);
                desc->len = buf.len;
                desc->flags = VRING_DESC_F_WRITE;
                
                rx_vq->add_buf_wait(desc, 1, 0, buf.data);
                _rx_buffers.push_back(buf);
            } else {
                memory::free_page(buf.data);
            }
        }
        
        rx_vq->get_buf_finalize();
    }
    
    rx_vq->kick();
    _rx_wq.wake_all();
}

void vsock::handle_tx()
{
    auto tx_vq = get_virt_queue(TX_QUEUE);
    if (!tx_vq) {
        return;
    }
    
    vring_used_elem* used;
    void* cookie;
    
    while ((used = tx_vq->get_buf_elem(&cookie)) != nullptr) {
        // Free transmitted buffer
        memory::free_page(cookie);
        tx_vq->get_buf_finalize();
    }
}

void vsock::handle_event()
{
    auto event_vq = get_virt_queue(EVENT_QUEUE);
    if (!event_vq) {
        return;
    }
    
    // Handle events (not implemented yet)
    virtio_d("VSock event received");
}

int vsock::send_packet(const virtio_vsock_hdr& hdr, const void* data, size_t len)
{
    auto tx_vq = get_virt_queue(TX_QUEUE);
    if (!tx_vq) {
        return -1;
    }
    
    // Allocate buffer for header + data
    size_t total_len = sizeof(hdr) + len;
    void* buf = memory::alloc_page();
    if (!buf) {
        return -1;
    }
    
    // Copy header and data
    memcpy(buf, &hdr, sizeof(hdr));
    if (data && len > 0) {
        memcpy(static_cast<char*>(buf) + sizeof(hdr), data, len);
    }
    
    // Add to TX queue
    vring_desc* desc = tx_vq->alloc_desc();
    if (!desc) {
        memory::free_page(buf);
        return -1;
    }
    
    desc->addr = mmu::virt_to_phys(buf);
    desc->len = total_len;
    desc->flags = 0; // Read-only for device
    
    tx_vq->add_buf_wait(desc, 1, 0, buf);
    tx_vq->kick();
    
    _tx_packets++;
    _tx_bytes += total_len;
    
    virtio_d("TX packet: src_cid=%llu, dst_cid=%llu, src_port=%u, dst_port=%u, op=%u, len=%u",
            hdr.src_cid, hdr.dst_cid, hdr.src_port, hdr.dst_port, hdr.op, hdr.len);
    
    return 0;
}

void vsock::recv_packet()
{
    // Wait for packets
    WITH_LOCK(_rx_mutex) {
        _rx_wq.wait(_rx_mutex);
    }
}

// Global functions for protocol layer access
vsock* get_vsock_driver()
{
    return s_vsock_driver;
}

// Driver probe function
hw_driver* vsock::probe(hw_device* dev)
{
    if (auto virtio_dev = dynamic_cast<virtio_device*>(dev)) {
        if (virtio_dev->get_id() == hw_device_id(VIRTIO_VENDOR_ID, VIRTIO_ID_VSOCK)) {
            return aligned_new<vsock>(*virtio_dev);
        }
    }
    return nullptr;
}

}

// Register driver
static __attribute__((constructor(init_prio::virtio_vsock)))
void virtio_vsock_init()
{
    driver_manager::register_driver(virtio::vsock::probe);
}