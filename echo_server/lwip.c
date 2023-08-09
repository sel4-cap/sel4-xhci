/*
 * Copyright 2022, UNSW
 * SPDX-License-Identifier: BSD-2-Clause
 */

// #include <stdbool.h>
#include <string.h>
#include <sel4cp.h>

#include "lwip/init.h"
#include "netif/etharp.h"
#include "lwip/prot/ethernet.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "lwip/sys.h"
#include "lwip/dhcp.h"
#include "lwip/prot/etharp.h"

#include "kmem.h"

#include "../../xhci_stub/include/dma/shared_ringbuffer.h"
#include "include/echo.h"
#include "include/timer.h"

#define IRQ    1
#define TX_CH  2
#define RX_CH  2
#define INIT   4

#define LINK_SPEED 1000000000 // Gigabit
#define ETHER_MTU 1500
#define NUM_BUFFERS 512
#define BUF_SIZE 2048

/* Memory regions. These all have to be here to keep compiler happy */
uintptr_t eth_rx_free;
uintptr_t eth_rx_used;
uintptr_t eth_tx_free;
uintptr_t eth_tx_used;
uintptr_t copy_rx;
uintptr_t eth_dma_vaddr;
uintptr_t uart_base;
uintptr_t heap_base;

typedef enum {
    ORIGIN_RX_QUEUE,
    ORIGIN_TX_QUEUE,
} ethernet_buffer_origin_t;

typedef struct ethernet_buffer {
    /* The acutal underlying memory of the buffer */
    uintptr_t buffer;
    /* The physical size of the buffer */
    size_t size;
    /* Queue from which the buffer was allocated */
    char origin;
    /* Index into buffer_metadata array */
    unsigned int index;
    /* in use */
    bool in_use;
} ethernet_buffer_t;

typedef struct state {
    struct netif netif;
    /* mac address for this client */
    uint8_t mac[6];

    /* Pointers to shared buffers */
    ring_handle_t rx_ring;
    ring_handle_t tx_ring;
    /*
     * Metadata associated with buffers
     */
    ethernet_buffer_t buffer_metadata[NUM_BUFFERS * 2];
} state_t;
state_t state;
// state_t state = {
//     .mac[0] = 12,
//     .mac[1] = 34,
//     .mac[2] = 56,
//     .mac[3] = 78,
//     .mac[4] = 90,
//     .mac[5] = 12
// };

/* LWIP mempool declare literally just initialises an array big enough with the correct alignment */
typedef struct lwip_custom_pbuf { 
    struct pbuf_custom custom;
    ethernet_buffer_t *buffer;
    state_t *state;
} lwip_custom_pbuf_t;
LWIP_MEMPOOL_DECLARE(
    RX_POOL,
    NUM_BUFFERS * 2,
    sizeof(lwip_custom_pbuf_t),
    "Zero-copy RX pool"
);

static inline void return_buffer(state_t *state, ethernet_buffer_t *buffer)
{
    /* As the rx free ring is the size of the number of buffers we have,
    the ring should never be full. */
    enqueue_free(&(state->rx_ring), buffer->buffer, BUF_SIZE, buffer);
}

/**
 * Free a pbuf. This also returns the underlying buffer to
 * the appropriate place.
 *
 * @param buf pbuf to free.
 *
 */
static void interface_free_buffer(struct pbuf *buf)
{
    SYS_ARCH_DECL_PROTECT(old_level);

    lwip_custom_pbuf_t *custom_pbuf = (lwip_custom_pbuf_t *) buf;

    SYS_ARCH_PROTECT(old_level);
    return_buffer(custom_pbuf->state, custom_pbuf->buffer);
    LWIP_MEMPOOL_FREE(RX_POOL, custom_pbuf);
    SYS_ARCH_UNPROTECT(old_level);
}

/**
 * Create a pbuf structure to pass to the network interface.
 *
 * @param state client state data.
 * @param buffer ethernet buffer containing metadata for the actual buffer
 * @param length length of data
 * 
 * @return the newly created pbuf.
 */
static struct pbuf *create_interface_buffer(state_t *state, ethernet_buffer_t *buffer, size_t length)
{
    lwip_custom_pbuf_t *custom_pbuf = (lwip_custom_pbuf_t *) LWIP_MEMPOOL_ALLOC(RX_POOL);

    custom_pbuf->state = state;
    custom_pbuf->buffer = buffer;
    custom_pbuf->custom.custom_free_function = interface_free_buffer;

    return pbuf_alloced_custom(
        PBUF_RAW,
        length,
        PBUF_REF,
        &custom_pbuf->custom,
        (void *)buffer->buffer,
        buffer->size
    );
}

/**
 * Allocate an empty TX buffer from the empty pool
 *
 * @param state client state data.
 * @param length length of buffer required
 *
 */
static inline ethernet_buffer_t *alloc_tx_buffer(state_t *state, size_t length)
{   
    if (BUF_SIZE < length) {
        sel4cp_dbg_puts("Requested buffer size too large.");
        return NULL;
    }

    uintptr_t addr;
    unsigned int len;
    ethernet_buffer_t *buffer;

    dequeue_free(&state->tx_ring, &addr, &len, (void **)&buffer);
    if (!buffer) {
        printf("lwip: dequeued a null ethernet buffer\n");
    }

    return buffer;
}

static err_t lwip_eth_send(struct netif *netif, struct pbuf *p)
{
    /* Grab an available TX buffer, copy pbuf data over,
    add to used tx ring, notify server */
    err_t ret = ERR_OK;

    if (p->tot_len > BUF_SIZE) {
        return ERR_MEM;
    }

    state_t *state = (state_t *)netif->state;

    ethernet_buffer_t *buffer = alloc_tx_buffer(state, p->tot_len);
    if (buffer == NULL) {
        return ERR_MEM;
    }
    unsigned char *frame = (unsigned char *)buffer->buffer;

    /* Copy all buffers that need to be copied */
    unsigned int copied = 0;
    for (struct pbuf *curr = p; curr != NULL; curr = curr->next) {
        unsigned char *buffer_dest = &frame[copied];
        if ((uintptr_t)buffer_dest != (uintptr_t)curr->payload) {
            /* Don't copy memory back into the same location */
            memcpy(buffer_dest, curr->payload, curr->len);
        }
        copied += curr->len;
    }

    int err = seL4_ARM_VSpace_Clean_Data(3, (uintptr_t)frame, (uintptr_t)frame + copied);
    if (err) {
        printf("ARM Vspace clean failed\n");
        printf(err);
    }

    /* insert into the used tx queue */
    int error = enqueue_used(&state->tx_ring, (uintptr_t)frame, copied, buffer);
    if (error) {
        printf("TX used ring full\n");
        enqueue_free(&(state->tx_ring), (uintptr_t)frame, BUF_SIZE, buffer);
        return ERR_MEM;
    }

    /* Notify the server for next time we recv() */
    have_signal = true;
    signal_msg = seL4_MessageInfo_new(0, 0, 0, 0);
    signal = (BASE_OUTPUT_NOTIFICATION_CAP + TX_CH);
    /* NOTE: If driver is passive, we want to Call instead. */

    return ret;
}

void process_rx_queue(void) 
{
    printf("process rx queue calling ring_empty\n");
    while(!ring_empty(state.rx_ring.used_ring)) {
        uintptr_t addr;
        unsigned int len;
        ethernet_buffer_t *buffer;

        dequeue_used(&state.rx_ring, &addr, &len, (void **)&buffer);

        if (addr != buffer->buffer) {
            printf("sanity check failed\n");
        }

        /* Invalidate the memory */
        int err = seL4_ARM_VSpace_Invalidate_Data(3, buffer->buffer, buffer->buffer + ETHER_MTU);
        if (err) {
            printf("ARM Vspace invalidate failed\n");
            printf(err);
        }

        struct pbuf *p = create_interface_buffer(&state, (void *)buffer, len);

        if (state.netif.input(p, &state.netif) != ERR_OK) {
            // If it is successfully received, the receiver controls whether or not it gets freed.
            printf("netif.input() != ERR_OK");
            pbuf_free(p);
        }
    }
}

/**
 * Initialise the network interface data structure.
 *
 * @param netif network interface data structuer.
 */
static err_t ethernet_init(struct netif *netif)
{
    printf("ethernet_init\n");
    if (netif->state == NULL) {
        return ERR_ARG;
    }

    printf("ethernet_init 1\n");
    state_t *data = netif->state;

    netif->hwaddr[0] = data->mac[0];
    netif->hwaddr[1] = data->mac[1];
    netif->hwaddr[2] = data->mac[2];
    netif->hwaddr[3] = data->mac[3];
    netif->hwaddr[4] = data->mac[4];
    netif->hwaddr[5] = data->mac[5];
    netif->mtu = ETHER_MTU;
    netif->hwaddr_len = ETHARP_HWADDR_LEN;
    printf("ethernet_init 2\n");
    netif->output = etharp_output;
    printf("ethernet_init 3\n");
    netif->linkoutput = lwip_eth_send;
    NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, LINK_SPEED);
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_IGMP;

    return ERR_OK;
}

static void netif_status_callback(struct netif *netif)
{
    if (dhcp_supplied_address(netif)) {
        printf("DHCP request finished, IP address for netif ");
        printf(netif->name);
        printf(" is: ");
        printf(ip4addr_ntoa(netif_ip4_addr(netif)));
        printf("\n");
    }
}

static void get_mac(void)
{
    sel4cp_dbg_puts("lwip get mac\n");
    // sel4cp_dbg_puts((char) state.mac[0]);

    sel4cp_ppcall(INIT, sel4cp_msginfo_new(0, 0));
    uint32_t palr = sel4cp_mr_get(0);
    uint32_t paur = sel4cp_mr_get(1);
    state.mac[0] = palr >> 24;
    state.mac[1] = palr >> 16 & 0xff;
    state.mac[2] = palr >> 8 & 0xff;
    state.mac[3] = palr & 0xff;
    state.mac[4] = paur >> 24;
    state.mac[5] = paur >> 16 & 0xff;
}

void init_post(void)
{   
    printf("init post lwip\n");
    netif_set_status_callback(&(state.netif), netif_status_callback);
    printf("init post 2 lwip\n");
    netif_set_up(&(state.netif));

    if (dhcp_start(&(state.netif))) {
        sel4cp_dbg_puts("failed to start DHCP negotiation\n");
    }

    setup_udp_socket();
    setup_utilization_socket();

    sel4cp_dbg_puts(sel4cp_name);
    sel4cp_dbg_puts(": init complete -- waiting for notification\n");
}

void init(void)
{
    sel4cp_dbg_puts(sel4cp_name);
    sel4cp_dbg_puts(": elf PD init function running\n");

    // state.rx_ring = kmem_alloc(sizeof(*state.rx_ring), 0);
    // state.tx_ring = kmem_alloc(sizeof(*state.tx_ring), 0);
    /* Set up shared memory regions */
    ring_init(&state.rx_ring, (ring_buffer_t *)eth_rx_free, (ring_buffer_t *)eth_rx_used, NULL, 1);
    ring_init(&state.tx_ring, (ring_buffer_t *)eth_tx_free, (ring_buffer_t *)eth_tx_used, NULL, 1);

    for (int i = 0; i < NUM_BUFFERS - 1; i++) {
        ethernet_buffer_t *buffer = &state.buffer_metadata[i];
        *buffer = (ethernet_buffer_t) {
            .buffer = eth_dma_vaddr + (BUF_SIZE * i),
            .size = BUF_SIZE,
            .origin = ORIGIN_RX_QUEUE,
            .index = i,
            .in_use = false,
        };
        enqueue_free(&state.rx_ring, buffer->buffer, BUF_SIZE, buffer);
    }

    for (int i = 0; i < NUM_BUFFERS - 1; i++) {
        ethernet_buffer_t *buffer = &state.buffer_metadata[i + NUM_BUFFERS];
        *buffer = (ethernet_buffer_t) {
            .buffer = eth_dma_vaddr + (BUF_SIZE * (i + NUM_BUFFERS)),
            .size = BUF_SIZE,
            .origin = ORIGIN_TX_QUEUE,
            .index = i + NUM_BUFFERS,
            .in_use = false,
        };

        enqueue_free(&state.tx_ring, buffer->buffer, BUF_SIZE, buffer);
    }

    lwip_init();

    gpt_init();

    LWIP_MEMPOOL_INIT(RX_POOL);

    get_mac();

    /* Set some dummy IP configuration values to get lwIP bootstrapped  */
    struct ip4_addr netmask, ipaddr, gw, multicast;
    ipaddr_aton("0.0.0.0", &gw);
    ipaddr_aton("0.0.0.0", &ipaddr);
    ipaddr_aton("0.0.0.0", &multicast);
    ipaddr_aton("255.255.255.0", &netmask);

    state.netif.name[0] = 'e';
    state.netif.name[1] = '0';

    sel4cp_dbg_puts("About to do netif_add\n");

    if (!netif_add(&(state.netif), &ipaddr, &netmask, &gw, &state,
              ethernet_init, netif_input)) { // This block was commented out but needs to be initialised
        sel4cp_dbg_puts("Netif add returned NULL\n");
    }

    sel4cp_dbg_puts("Done netif_add\n");

    netif_set_default(&(state.netif));

    sel4cp_notify(INIT);
}

void notified(sel4cp_channel ch)
{
    switch(ch) {
        case RX_CH:
            process_rx_queue();
            return;
        case INIT:
            init_post();
            return;
        case IRQ:
            /* Timer */
            irq(ch);
            sel4cp_irq_ack(ch);
            return;
        default:
            sel4cp_dbg_puts("lwip: received notification on unexpected channel\n");
            break;
    }
}