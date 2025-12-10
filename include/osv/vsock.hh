/*
 * Copyright (C) 2024 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef OSV_VSOCK_HH
#define OSV_VSOCK_HH

#include <sys/socket.h>
#include <stdint.h>

/* VSock address structure */
struct sockaddr_vm {
    sa_family_t svm_family;     /* AF_VSOCK */
    unsigned short svm_reserved1;
    unsigned int svm_port;      /* Port number */
    unsigned int svm_cid;       /* Context ID */
    unsigned char svm_zero[sizeof(struct sockaddr) - sizeof(sa_family_t) - 
                          sizeof(unsigned short) - sizeof(unsigned int) - 
                          sizeof(unsigned int)];
};

/* VSock Context IDs */
#define VMADDR_CID_ANY          0xffffffff
#define VMADDR_CID_HYPERVISOR   0
#define VMADDR_CID_LOCAL        1
#define VMADDR_CID_HOST         2

/* VSock Port numbers */
#define VMADDR_PORT_ANY         0xffffffff

/* VSock socket options */
#define SO_VM_SOCKETS_BUFFER_SIZE       0
#define SO_VM_SOCKETS_BUFFER_MIN_SIZE   1
#define SO_VM_SOCKETS_BUFFER_MAX_SIZE   2
#define SO_VM_SOCKETS_CONNECT_TIMEOUT   6
#define SO_VM_SOCKETS_NONBLOCK_TXRX     7
#define SO_VM_SOCKETS_PEER_HOST_VM_ID   8
#define SO_VM_SOCKETS_TRUSTED           9

/* VSock packet types */
#define VIRTIO_VSOCK_TYPE_STREAM        1

/* VSock operations */
#define VIRTIO_VSOCK_OP_INVALID         0
#define VIRTIO_VSOCK_OP_REQUEST         1
#define VIRTIO_VSOCK_OP_RESPONSE        2
#define VIRTIO_VSOCK_OP_RST             3
#define VIRTIO_VSOCK_OP_SHUTDOWN        4
#define VIRTIO_VSOCK_OP_RW              5
#define VIRTIO_VSOCK_OP_CREDIT_UPDATE   6
#define VIRTIO_VSOCK_OP_CREDIT_REQUEST  7

/* VSock shutdown flags */
#define VIRTIO_VSOCK_SHUTDOWN_RCV       1
#define VIRTIO_VSOCK_SHUTDOWN_SEND      2

/* VSock packet header */
struct virtio_vsock_hdr {
    uint64_t src_cid;
    uint64_t dst_cid;
    uint32_t src_port;
    uint32_t dst_port;
    uint32_t len;
    uint16_t type;
    uint16_t op;
    uint32_t flags;
    uint32_t buf_alloc;
    uint32_t fwd_cnt;
} __attribute__((packed));

#endif /* OSV_VSOCK_HH */