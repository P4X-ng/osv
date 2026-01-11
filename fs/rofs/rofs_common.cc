/*
 * Copyright (c) 2015 Carnegie Mellon University.
 * All Rights Reserved.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS," WITH NO WARRANTIES WHATSOEVER. CARNEGIE
 * MELLON UNIVERSITY EXPRESSLY DISCLAIMS TO THE FULLEST EXTENT PERMITTEDBY LAW
 * ALL EXPRESS, IMPLIED, AND STATUTORY WARRANTIES, INCLUDING, WITHOUT
 * LIMITATION, THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, AND NON-INFRINGEMENT OF PROPRIETARY RIGHTS.
 *
 * Released under a modified BSD license. For full terms, please see mfs.txt in
 * the licenses folder or contact permi...@sei.cmu.edu.
 *
 * DM-0002621
 *
 * Based on https://github.com/jdroot/mfs
 *
 * Copyright (C) 2017 Waldemar Kozaczuk
 * Inspired by original MFS implementation by James Root from 2015
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include "rofs.hh"
#include <osv/device.h>
#include <osv/bio.h>
#include <algorithm>

#if defined(ROFS_DIAGNOSTICS_ENABLED)
extern std::atomic<long> rofs_block_read_count;
extern std::atomic<long> rofs_block_read_ms;
#endif

void rofs_set_vnode(struct vnode *vnode, struct rofs_inode *inode)
{
    off_t size = 0;
    if (vnode == nullptr || inode == nullptr) {
        return;
    }

    vnode->v_data = inode;
    vnode->v_ino = inode->inode_no;

    // Set type
    if (S_ISDIR(inode->mode)) {
        size = ROFS_INODE_SIZE; //TODO: Revisit
        vnode->v_type = VDIR;
    } else if (S_ISREG(inode->mode)) {
        size = inode->file_size;
        vnode->v_type = VREG;
    } else if (S_ISLNK(inode->mode)) {
        size = 512; // TODO: Revisit
        vnode->v_type = VLNK;
    }

    vnode->v_mode = 0555;
    vnode->v_size = size;
}

int
rofs_read_blocks(struct device *device, uint64_t starting_block, uint64_t blocks_count, void *buf)
{
    ROFS_STOPWATCH_START
    
    // Calculate total bytes to read
    uint64_t total_bytes = blocks_count * BSIZE;
    uint64_t max_request_bytes = device->max_io_size;
    
    // If the request fits within device limits, use the original single-request approach
    if (total_bytes <= max_request_bytes) {
        struct bio *bio = alloc_bio();
        if (!bio)
            return ENOMEM;

        bio->bio_cmd = BIO_READ;
        bio->bio_dev = device;
        bio->bio_data = buf;
        bio->bio_offset = starting_block << 9;
        bio->bio_bcount = total_bytes;

        bio->bio_dev->driver->devops->strategy(bio);
        int error = bio_wait(bio);

        destroy_bio(bio);

#if defined(ROFS_DIAGNOSTICS_ENABLED)
        rofs_block_read_count += blocks_count;
#endif
        ROFS_STOPWATCH_END(rofs_block_read_ms)
        return error;
    }
    
    // Split large request into smaller chunks
    uint64_t max_blocks_per_request = max_request_bytes / BSIZE;
    uint64_t current_block = starting_block;
    uint64_t remaining_blocks = blocks_count;
    char *current_buf = static_cast<char*>(buf);
    int error = 0;
    
    while (remaining_blocks > 0 && error == 0) {
        uint64_t blocks_this_request = std::min(remaining_blocks, max_blocks_per_request);
        uint64_t bytes_this_request = blocks_this_request * BSIZE;
        
        struct bio *bio = alloc_bio();
        if (!bio) {
            error = ENOMEM;
            break;
        }

        bio->bio_cmd = BIO_READ;
        bio->bio_dev = device;
        bio->bio_data = current_buf;
        bio->bio_offset = current_block << 9;
        bio->bio_bcount = bytes_this_request;

        bio->bio_dev->driver->devops->strategy(bio);
        error = bio_wait(bio);

        destroy_bio(bio);
        
        if (error == 0) {
            current_block += blocks_this_request;
            remaining_blocks -= blocks_this_request;
            current_buf += bytes_this_request;
        }
    }

#if defined(ROFS_DIAGNOSTICS_ENABLED)
    if (error == 0) {
        rofs_block_read_count += blocks_count;
    }
#endif
    ROFS_STOPWATCH_END(rofs_block_read_ms)

    return error;
}
