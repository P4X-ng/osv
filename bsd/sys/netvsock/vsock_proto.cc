/*
 * Copyright (C) 2024 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <bsd/sys/sys/param.h>
#include <bsd/sys/sys/socket.h>
#include <bsd/sys/sys/socketvar.h>
#include <bsd/sys/sys/protosw.h>
#include <bsd/sys/sys/domain.h>
#include <bsd/sys/sys/mbuf.h>
#include <bsd/sys/sys/errno.h>
#include <bsd/sys/sys/proc.h>

#include <osv/vsock.hh>
#include <drivers/virtio-vsock.hh>

extern "C" {

// Forward declarations
static int vsock_attach(struct socket *so, int proto, struct thread *td);
static int vsock_detach(struct socket *so);
static int vsock_bind(struct socket *so, struct sockaddr *nam, struct thread *td);
static int vsock_connect(struct socket *so, struct sockaddr *nam, struct thread *td);
static int vsock_disconnect(struct socket *so);
static int vsock_listen(struct socket *so, int backlog, struct thread *td);
static int vsock_accept(struct socket *so, struct sockaddr **nam);
static int vsock_send(struct socket *so, int flags, struct mbuf *m,
                     struct sockaddr *addr, struct mbuf *control, struct thread *td);
static int vsock_shutdown(struct socket *so, int how);
static int vsock_sockaddr(struct socket *so, struct sockaddr **nam);
static int vsock_peeraddr(struct socket *so, struct sockaddr **nam);

// VSock protocol control block
struct vsock_pcb {
    uint32_t local_port;
    uint32_t remote_port;
    uint64_t local_cid;
    uint64_t remote_cid;
    int state;
    struct socket *socket;
};

#define VSOCK_STATE_UNBOUND     0
#define VSOCK_STATE_BOUND       1
#define VSOCK_STATE_CONNECTING  2
#define VSOCK_STATE_CONNECTED   3
#define VSOCK_STATE_LISTENING   4
#define VSOCK_STATE_CLOSING     5

// Protocol user requests
static struct pr_usrreqs vsock_usrreqs = {
    .pru_attach = vsock_attach,
    .pru_detach = vsock_detach,
    .pru_bind = vsock_bind,
    .pru_connect = vsock_connect,
    .pru_disconnect = vsock_disconnect,
    .pru_listen = vsock_listen,
    .pru_accept = vsock_accept,
    .pru_send = vsock_send,
    .pru_shutdown = vsock_shutdown,
    .pru_sockaddr = vsock_sockaddr,
    .pru_peeraddr = vsock_peeraddr,
};

static int
vsock_attach(struct socket *so, int proto, struct thread *td)
{
    struct vsock_pcb *pcb;
    int error = 0;

    if (so->so_pcb != NULL)
        return EISCONN;

    pcb = (struct vsock_pcb *)malloc(sizeof(struct vsock_pcb), M_PCB, M_WAITOK | M_ZERO);
    if (pcb == NULL)
        return ENOBUFS;

    pcb->socket = so;
    pcb->state = VSOCK_STATE_UNBOUND;
    pcb->local_port = VMADDR_PORT_ANY;
    pcb->remote_port = VMADDR_PORT_ANY;
    pcb->local_cid = VMADDR_CID_ANY;
    pcb->remote_cid = VMADDR_CID_ANY;

    so->so_pcb = pcb;
    
    // Set socket buffer sizes
    error = soreserve(so, 65536, 65536);
    if (error) {
        free(pcb, M_PCB);
        so->so_pcb = NULL;
        return error;
    }

    return 0;
}

static int
vsock_detach(struct socket *so)
{
    struct vsock_pcb *pcb = (struct vsock_pcb *)so->so_pcb;

    if (pcb == NULL)
        return EINVAL;

    free(pcb, M_PCB);
    so->so_pcb = NULL;
    return 0;
}

static int
vsock_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
    struct vsock_pcb *pcb = (struct vsock_pcb *)so->so_pcb;
    struct sockaddr_vm *addr = (struct sockaddr_vm *)nam;

    if (pcb == NULL)
        return EINVAL;

    if (nam->sa_family != AF_VSOCK)
        return EAFNOSUPPORT;

    if (nam->sa_len != sizeof(struct sockaddr_vm))
        return EINVAL;

    // Get guest CID from driver
    auto driver = virtio::get_vsock_driver();
    if (!driver)
        return ENODEV;

    pcb->local_cid = driver->get_guest_cid();
    pcb->local_port = addr->svm_port;
    pcb->state = VSOCK_STATE_BOUND;

    return 0;
}

static int
vsock_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
    struct vsock_pcb *pcb = (struct vsock_pcb *)so->so_pcb;
    struct sockaddr_vm *addr = (struct sockaddr_vm *)nam;

    if (pcb == NULL)
        return EINVAL;

    if (nam->sa_family != AF_VSOCK)
        return EAFNOSUPPORT;

    if (nam->sa_len != sizeof(struct sockaddr_vm))
        return EINVAL;

    auto driver = virtio::get_vsock_driver();
    if (!driver)
        return ENODEV;

    // Set up connection
    if (pcb->state == VSOCK_STATE_UNBOUND) {
        pcb->local_cid = driver->get_guest_cid();
        pcb->local_port = 0; // Auto-assign port
    }

    pcb->remote_cid = addr->svm_cid;
    pcb->remote_port = addr->svm_port;
    pcb->state = VSOCK_STATE_CONNECTING;

    // Send connection request
    virtio_vsock_hdr hdr = {};
    hdr.src_cid = pcb->local_cid;
    hdr.dst_cid = pcb->remote_cid;
    hdr.src_port = pcb->local_port;
    hdr.dst_port = pcb->remote_port;
    hdr.type = VIRTIO_VSOCK_TYPE_STREAM;
    hdr.op = VIRTIO_VSOCK_OP_REQUEST;
    hdr.len = 0;

    int ret = driver->send_packet(hdr, nullptr, 0);
    if (ret < 0)
        return EIO;

    // TODO: Wait for response
    pcb->state = VSOCK_STATE_CONNECTED;
    soisconnected(so);

    return 0;
}

static int
vsock_disconnect(struct socket *so)
{
    struct vsock_pcb *pcb = (struct vsock_pcb *)so->so_pcb;

    if (pcb == NULL)
        return EINVAL;

    if (pcb->state == VSOCK_STATE_CONNECTED) {
        auto driver = virtio::get_vsock_driver();
        if (driver) {
            // Send disconnect packet
            virtio_vsock_hdr hdr = {};
            hdr.src_cid = pcb->local_cid;
            hdr.dst_cid = pcb->remote_cid;
            hdr.src_port = pcb->local_port;
            hdr.dst_port = pcb->remote_port;
            hdr.type = VIRTIO_VSOCK_TYPE_STREAM;
            hdr.op = VIRTIO_VSOCK_OP_RST;
            hdr.len = 0;

            driver->send_packet(hdr, nullptr, 0);
        }
    }

    pcb->state = VSOCK_STATE_UNBOUND;
    soisdisconnected(so);
    return 0;
}

static int
vsock_listen(struct socket *so, int backlog, struct thread *td)
{
    struct vsock_pcb *pcb = (struct vsock_pcb *)so->so_pcb;

    if (pcb == NULL)
        return EINVAL;

    if (pcb->state != VSOCK_STATE_BOUND)
        return EINVAL;

    pcb->state = VSOCK_STATE_LISTENING;
    solisten(so, backlog, td);
    return 0;
}

static int
vsock_accept(struct socket *so, struct sockaddr **nam)
{
    struct vsock_pcb *pcb = (struct vsock_pcb *)so->so_pcb;
    struct sockaddr_vm *addr;

    if (pcb == NULL)
        return EINVAL;

    addr = (struct sockaddr_vm *)malloc(sizeof(struct sockaddr_vm), M_SONAME, M_WAITOK | M_ZERO);
    addr->svm_family = AF_VSOCK;
    addr->svm_cid = pcb->remote_cid;
    addr->svm_port = pcb->remote_port;

    *nam = (struct sockaddr *)addr;
    return 0;
}

static int
vsock_send(struct socket *so, int flags, struct mbuf *m,
          struct sockaddr *addr, struct mbuf *control, struct thread *td)
{
    struct vsock_pcb *pcb = (struct vsock_pcb *)so->so_pcb;
    int error = 0;

    if (pcb == NULL) {
        error = EINVAL;
        goto bad;
    }

    if (pcb->state != VSOCK_STATE_CONNECTED) {
        error = ENOTCONN;
        goto bad;
    }

    auto driver = virtio::get_vsock_driver();
    if (!driver) {
        error = ENODEV;
        goto bad;
    }

    // Send data packet
    virtio_vsock_hdr hdr = {};
    hdr.src_cid = pcb->local_cid;
    hdr.dst_cid = pcb->remote_cid;
    hdr.src_port = pcb->local_port;
    hdr.dst_port = pcb->remote_port;
    hdr.type = VIRTIO_VSOCK_TYPE_STREAM;
    hdr.op = VIRTIO_VSOCK_OP_RW;
    hdr.len = m->m_pkthdr.len;

    // Copy mbuf data to contiguous buffer
    char *data = (char *)malloc(hdr.len, M_TEMP, M_WAITOK);
    m_copydata(m, 0, hdr.len, data);

    int ret = driver->send_packet(hdr, data, hdr.len);
    free(data, M_TEMP);

    if (ret < 0) {
        error = EIO;
        goto bad;
    }

bad:
    if (m)
        m_freem(m);
    if (control)
        m_freem(control);
    return error;
}

static int
vsock_shutdown(struct socket *so, int how)
{
    struct vsock_pcb *pcb = (struct vsock_pcb *)so->so_pcb;

    if (pcb == NULL)
        return EINVAL;

    if (pcb->state == VSOCK_STATE_CONNECTED) {
        auto driver = virtio::get_vsock_driver();
        if (driver) {
            // Send shutdown packet
            virtio_vsock_hdr hdr = {};
            hdr.src_cid = pcb->local_cid;
            hdr.dst_cid = pcb->remote_cid;
            hdr.src_port = pcb->local_port;
            hdr.dst_port = pcb->remote_port;
            hdr.type = VIRTIO_VSOCK_TYPE_STREAM;
            hdr.op = VIRTIO_VSOCK_OP_SHUTDOWN;
            hdr.flags = 0;
            if (how & SHUT_RD)
                hdr.flags |= VIRTIO_VSOCK_SHUTDOWN_RCV;
            if (how & SHUT_WR)
                hdr.flags |= VIRTIO_VSOCK_SHUTDOWN_SEND;
            hdr.len = 0;

            driver->send_packet(hdr, nullptr, 0);
        }
    }

    socantsendmore(so);
    return 0;
}

static int
vsock_sockaddr(struct socket *so, struct sockaddr **nam)
{
    struct vsock_pcb *pcb = (struct vsock_pcb *)so->so_pcb;
    struct sockaddr_vm *addr;

    if (pcb == NULL)
        return EINVAL;

    addr = (struct sockaddr_vm *)malloc(sizeof(struct sockaddr_vm), M_SONAME, M_WAITOK | M_ZERO);
    addr->svm_family = AF_VSOCK;
    addr->svm_cid = pcb->local_cid;
    addr->svm_port = pcb->local_port;

    *nam = (struct sockaddr *)addr;
    return 0;
}

static int
vsock_peeraddr(struct socket *so, struct sockaddr **nam)
{
    struct vsock_pcb *pcb = (struct vsock_pcb *)so->so_pcb;
    struct sockaddr_vm *addr;

    if (pcb == NULL)
        return EINVAL;

    addr = (struct sockaddr_vm *)malloc(sizeof(struct sockaddr_vm), M_SONAME, M_WAITOK | M_ZERO);
    addr->svm_family = AF_VSOCK;
    addr->svm_cid = pcb->remote_cid;
    addr->svm_port = pcb->remote_port;

    *nam = (struct sockaddr *)addr;
    return 0;
}

} // extern "C"