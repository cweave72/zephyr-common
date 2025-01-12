/*******************************************************************************
 *  @file: eth_serial.c
 *  
 *  @brief: Source for uart-pipe based ethernet device.
*******************************************************************************/
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>    // for CONTAINER_OF() macro
#include <zephyr/net/net_core.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/drivers/uart_pipe.h>
#include <zephyr/random/random.h>
#include "eth_serial.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(eth_serial, CONFIG_ETH_SERIAL_LOG_LEVEL);

#define ETH_SERIAL_MTU  1500
#define MAX_ETHERNET_FRAME_SIZE     (1500 + 18)

#if defined(CONFIG_ETH_SERIAL_SLIP)
#include "slip.h"
static slip_deframer_ctx deframer_state;

#elif defined(CONFIG_ETH_SERIAL_COBS)
#include "Cobs_frame.h"
static Cobs_Deframer deframer_state;
#endif

/** @brief Maximum buffer size for inbound deframed ethernet packet. */
static uint8_t buf_deframed[MAX_ETHERNET_FRAME_SIZE];
/** @brief Max framed/byte stuffed buffer to serial port. */
static uint8_t buf_framed[2*MAX_ETHERNET_FRAME_SIZE];

static uint8_t pktbuf_aggregate[MAX_ETHERNET_FRAME_SIZE];
static int packet_count = 0;

#define THREAD_PRIORITY K_PRIO_PREEMPT(8)

/* RX thread */
static K_THREAD_STACK_DEFINE(rx_stack, 1024);
static struct k_thread rx_thread_data;

struct fifo_data_item {
    void *fifo_rsvd;
    uint16_t len;
    uint8_t data[ETH_SERIAL_BUFFER_SIZE];
};

/* Memory slab for received data:  block size           num align */
K_MEM_SLAB_DEFINE_STATIC(rx_pool, sizeof(struct fifo_data_item), 5, 4);
/* RX data fifo */
K_FIFO_DEFINE(rx_fifo);

/******************************************************************************
    recv_cb
*//**
    @brief Callback for uart-pipe receive data.
    @param[in] buf  Buffer holding received data.
    @param[out] off  Running buffer pointer offset. 
******************************************************************************/
static uint8_t *
recv_cb(uint8_t *buf, size_t *off)
{
    struct eth_serial_context *ctx =
        CONTAINER_OF(buf, struct eth_serial_context, serial_buf[0]);
    uint32_t len = *off;
    struct fifo_data_item *item;
    int ret;

    /* We always consume all the data, reset the offset for the next call.*/
    *off = 0;

    if (!ctx->init_done)
    {
        goto done;
    }

    if ((ret = k_mem_slab_alloc(&rx_pool, (void **)&item, K_NO_WAIT)) < 0)
    {
        LOG_ERR("Error allocating from slab: %d", ret);
        goto done; 
    }

    item->len = len;
    memcpy(item->data, buf, len);

    k_fifo_put(&rx_fifo, item);
    
done:
    return buf;
}

/******************************************************************************
    [docimport eth_serial_send]
*//**
    @brief Sends raw ethernet packet via uart pipe.
******************************************************************************/
int
eth_serial_send(const struct device *dev, struct net_pkt *pkt)
{
    struct eth_serial_context *ctx = dev->data;
    struct net_buf *buf;
    uint16_t k = 0;

    ARG_UNUSED(dev);

    if (!pkt->buffer)
    {
        return -ENODATA;
    }

    for (buf = pkt->buffer; buf; buf = buf->frags)
    {
        uint16_t i;
        uint8_t *ptr = buf->data;

        LOG_DBG("Send fragment buffer %u bytes.", buf->len);
        for (i = 0; i < buf->len; i++)
        {
            pktbuf_aggregate[k++] = *ptr++;
        }
    }

    int size = ctx->framer(pktbuf_aggregate, k, buf_framed, sizeof(buf_framed));
    if (size <= 0)
    {
        return 0;
    }

    LOG_DBG("Wrote framed %u bytes.", size);
    uart_pipe_send(buf_framed, size);
    
    return 0;
}

/******************************************************************************
    rx_thread
*//**
    @brief Thread which handles processing received raw ethernet frames.
******************************************************************************/
static void
rx_thread(void *p1, void *p2, void *p3)
{
    struct eth_serial_context *ctx = (struct eth_serial_context *)p1;

    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1)
    {
        struct fifo_data_item *item;
        struct net_pkt *pkt;
        struct net_buf *pkt_buf;
        struct net_eth_hdr *hdr;
        uint8_t *ip;
        int size;
        int ret;

        /* Wait for new data to arrive. */
        item = k_fifo_get(&rx_fifo, K_FOREVER);

        /** @brief Push received byte into the deframer. */
        size = ctx->deframer(
            ctx->deframer_state,
            item->data, item->len,
            buf_deframed, sizeof(buf_deframed));

        k_mem_slab_free(&rx_pool, (void *)item);

        if (size <= 0)
        {
            /* No complete frame yet. */
            continue;
        }

        LOG_DBG("Received frame %d bytes (%u).", size, packet_count++);
        //LOG_HEXDUMP_DBG(buf_deframed, size, "deframed");

        pkt = net_pkt_rx_alloc_on_iface(ctx->iface, K_NO_WAIT);
        if (!pkt)
        {
            LOG_ERR("Error allocating network packet.");
            continue;
        }

        pkt_buf = net_pkt_get_frag(pkt, ETH_SERIAL_MTU, K_NO_WAIT);
        if (!pkt_buf)
        {
            LOG_ERR("Error allocating network buffer from packet.");
            net_pkt_unref(pkt);
            continue;
        }

        net_pkt_append_buffer(pkt, pkt_buf);

        /* Load L2 header */
        hdr = NET_ETH_HDR(pkt);
        memcpy(hdr, buf_deframed, sizeof(struct net_eth_hdr));

        /* Load remaining packet */
        ip = (uint8_t *)net_pkt_data(pkt) + sizeof(struct net_eth_hdr);
        memcpy(ip, buf_deframed + sizeof(struct net_eth_hdr),
            size - sizeof(struct net_eth_hdr));
        
        /* Push packet into the stack. */
        pkt->buffer->len = size;
        if ((ret = net_recv_data(ctx->iface, pkt)) < 0)
        {
            LOG_ERR("Network layer error: %d", ret);
            net_pkt_unref(pkt);
        }
    }
}

/******************************************************************************
    [docimport eth_serial_init]
*//**
    @brief Initializer for the eth_serial driver.
    @param[in] dev  Pointer to ethernet device object.
******************************************************************************/
int
eth_serial_init(const struct device *dev)
{
    struct eth_serial_context *ctx = dev->data;
    int ret;

    LOG_INF("Initializing eth_serial driver.");

    ctx->deframer_state = &deframer_state;

#if defined(CONFIG_ETH_SERIAL_SLIP)
    LOG_INF("Using SLIP framing.");
    ctx->framer         = slip_framer;
    ctx->deframer       = slip_deframer;
    if ((ret = slip_deframer_init(&deframer_state, 2048)) < 0)
    {
        LOG_ERR("Error initializing slip deframer: %d", ret);
        return -1;
    }
#elif defined(CONFIG_ETH_SERIAL_COBS)
    LOG_INF("Using COBS framing.");
    ctx->framer         = Cobs_framer;
    ctx->deframer       = Cobs_deframer;
    if ((ret = Cobs_deframer_init(&deframer_state, 2048)) < 0)
    {
        LOG_ERR("Error initializing Cobs deframer: %d", ret);
        return -1;
    }
#endif

    uart_pipe_register(ctx->serial_buf, sizeof(ctx->serial_buf), recv_cb);

    k_thread_create(&rx_thread_data, rx_stack,
                    K_THREAD_STACK_SIZEOF(rx_stack),
                    rx_thread,
                    ctx, NULL, NULL, THREAD_PRIORITY, 0, K_NO_WAIT);

    return 0;
}

/******************************************************************************
    [docimport eth_serial_iface_init]
*//**
    @brief Initializes the interface.
******************************************************************************/
void
eth_serial_iface_init(struct net_if *iface)
{
    struct eth_serial_context *ctx = net_if_get_device(iface)->data;
    struct net_linkaddr ll_addr;

    LOG_DBG("Initializing eth_serial iface.");

    ethernet_init(iface);

    if (ctx->init_done)
    {
        return;
    }

    ctx->iface = iface;
    ll_addr.addr = ctx->mac_addr;
    ll_addr.len = sizeof(ctx->mac_addr);
    
    ctx->mac_addr[0] = 0x00;
    ctx->mac_addr[1] = 0x00;
    ctx->mac_addr[2] = 0x5E;
    ctx->mac_addr[3] = 0x00;
    ctx->mac_addr[4] = 0x53;
    ctx->mac_addr[5] = sys_rand8_get();

    net_if_set_link_addr(iface, ll_addr.addr, ll_addr.len, NET_LINK_ETHERNET);

    ctx->init_done = true;
}

static struct eth_serial_context eth_serial_ctx_data = { .init_done = false };

static enum ethernet_hw_caps eth_capabilities(const struct device *dev)
{
	ARG_UNUSED(dev);
	return 0;
}

static const struct ethernet_api eth_serial_api = {
	.iface_api.init = eth_serial_iface_init,

	.get_capabilities = eth_capabilities,
	.send = eth_serial_send,
};


ETH_NET_DEVICE_INIT(eth_serial, "eth_serial",
		    eth_serial_init, NULL,
		    &eth_serial_ctx_data, NULL,
		    CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
		    &eth_serial_api, ETH_SERIAL_MTU);
