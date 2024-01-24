/* This work is Crown Copyright NCSC, 2023. */
/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
/* Author: alex.kroh@nicta.com.au */

#include <dma.h>
#include <printf.h>
#include <wrapper.h>
#include <tinyalloc.h>
#include <sys/kmem.h>

uintptr_t phys_base;
uintptr_t virt_base;
uintptr_t dma_limit;
uintptr_t allocated_dma;

/* #define DMA_DEBUG */

#ifdef DMA_DEBUG
#define dma_print(...) printf(__VA_ARGS__)
#else
#define dma_print(...) 0
#endif

void sel4_dma_init(uintptr_t pbase, uintptr_t vbase, uintptr_t limit) {
    phys_base = pbase;
    allocated_dma = vbase;
    virt_base = vbase;
    dma_limit = limit;
    dma_print("init phys_base: %p, vbase: %p\n", phys_base, virt_base);
}

int sel4_dma_map_create(bus_dmamap_t *dmap, bus_size_t s, bus_size_t mxs) {
    (*dmap) = kmem_zalloc(sizeof(**dmap),0);
	(*dmap)->dm_maxsegsz = mxs;
	(*dmap)->dm_mapsize = s;
	(*dmap)->dm_nsegs = 1;
    return 0;
}

int sel4_dma_map_load(bus_dmamap_t dmam, void* buf, bus_size_t s) {
    dmam->dm_segs[0].ds_addr = (bus_addr_t) buf;
    dmam->dm_segs[0].ds_len = s;
    return 0;
}

int sel4_dma_map(void **k, bus_dma_segment_t *sg) {
	*k = (void*) sg->ds_addr;
    return 0;
}

int sel4_dma_alloc(size_t size, bus_dma_segment_t *sg) {
    if (allocated_dma + size >= dma_limit) {
        dma_print("DMA_ERROR: out of memory\n");
        return 0;
    }
    uintptr_t start_addr = allocated_dma;
    allocated_dma += size;
    dma_print("Alloced at %p size %p\n", start_addr, size);
    sg->ds_addr = (bus_addr_t) start_addr;
    sg->ds_len = size;
    return 0;
}

uintptr_t* getPhys(void* virt) {
    int offset = (uint64_t)virt - (int)virt_base;
    dma_print("offset = %d\n", offset);
    dma_print("getting phys of %p: %p\n", virt, phys_base+offset);
    return (uintptr_t*)(phys_base+offset);
}

uintptr_t* getVirt(void* paddr) {
    uintptr_t *offset = paddr - phys_base;
    dma_print("getting virt of %p: %p\n", paddr, virt_base+offset);
    return (virt_base + offset);
}
