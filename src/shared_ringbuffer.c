/*
 * Copyright 2022, UNSW
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "shared_ringbuffer.h"
#include "printf.h"

#include <dev/wscons/wsksymdef.h>

void ring_init(ring_handle_t *ring, ring_buffer_t *free, ring_buffer_t *used, notify_fn notify, int buffer_init)
{
    printf("Test 1\n");
    ring->free_ring = free;
    printf("Test 2\n");
    ring->used_ring = used;
    printf("Test 3\n");
    ring->notify = notify;
    printf("Test 4\n");

    if (buffer_init) {
        printf("Test 5\n");
        ring->free_ring->write_idx = 0;
        printf("Test 6\n");
        ring->free_ring->read_idx = 0;
        printf("Test 7\n");
        ring->used_ring->write_idx = 0;
        printf("Test 8\n");
        ring->used_ring->read_idx = 0;
        printf("Test 9\n");
    }
}
