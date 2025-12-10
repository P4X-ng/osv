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

#include <osv/vsock.hh>

extern "C" {

// Forward declarations from vsock_proto.cc
extern struct pr_usrreqs vsock_usrreqs;

// VSock protocol switch table
static struct protosw vsocksw[] = {
{
    .pr_type = SOCK_STREAM,
    .pr_domain = nullptr, // Will be set during initialization
    .pr_protocol = 0,
    .pr_flags = PR_CONNREQUIRED | PR_WANTRCVD,
    .pr_usrreqs = &vsock_usrreqs,
},
{
    .pr_type = SOCK_DGRAM,
    .pr_domain = nullptr, // Will be set during initialization  
    .pr_protocol = 0,
    .pr_flags = PR_ATOMIC | PR_ADDR,
    .pr_usrreqs = &vsock_usrreqs,
}
};

// VSock domain structure
struct domain vsockdomain = {
    .dom_family = AF_VSOCK,
    .dom_name = "vsock",
    .dom_protosw = vsocksw,
    .dom_protoswNPROTOSW = &vsocksw[sizeof(vsocksw)/sizeof(vsocksw[0])],
    .dom_rtattach = nullptr,
    .dom_rtoffset = 0,
    .dom_maxrtkey = 0,
    .dom_ifattach = nullptr,
    .dom_ifdetach = nullptr,
};

// Initialize domain
static void
vsock_domain_init(void *arg)
{
    // Set domain pointer in protocol switch entries
    for (int i = 0; i < sizeof(vsocksw)/sizeof(vsocksw[0]); i++) {
        vsocksw[i].pr_domain = &vsockdomain;
    }
    
    // Register domain
    domain_add(&vsockdomain);
}

} // extern "C"

// Register domain initialization
VNET_DOMAIN_SET(vsock);