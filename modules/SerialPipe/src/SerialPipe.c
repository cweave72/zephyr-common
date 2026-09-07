/*******************************************************************************
 *  @file: SerialPipe.c
 *   
 *  @brief: Byte pipe over the chosen zephyr,uart-pipe UART.
*******************************************************************************/
#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/uart_pipe.h>

#include "CheckCond.h"
#include "SerialPipe.h"
#include "SwFifo.h"

/** @brief The uart-pipe driver keeps a single global receive callback with no
    way to de-register it or to query the current owner, so a second consumer
    cannot be detected at runtime - it silently steals the callback. Catch it
    here instead. CONFIG_NET_SLIP_TAP is the one to watch: it defaults to y on
    QEMU targets, pulling in Zephyr's own slip driver.
*/
#if defined(CONFIG_ETH_SERIAL) || defined(CONFIG_NET_SLIP_TAP) || \
    defined(CONFIG_IEEE802154_UART_PIPE)
#error "SerialPipe requires exclusive ownership of the uart-pipe callback."
#endif

/** @brief Initialize the logging module. */
LOG_MODULE_REGISTER(SerialPipe, CONFIG_SERIALPIPE_LOG_LEVEL);

/** @brief Buffer registered with the uart-pipe driver. Written from the ISR. */
static uint8_t serial_buf[CONFIG_SERIALPIPE_RX_BUF_SIZE];

/** @brief Flag indicating SerialPipe_init() has completed. */
static bool initialized;

/** @brief Gate holding the transmit thread until initialization completes. */
static K_SEM_DEFINE(init_sem, 0, 1);

/** @brief Transmit fifo item. */
struct tx_item
{
    /** @brief Reserved for use by the kernel fifo. Must be the first member. */
    void *fifo_rsvd;
    /** @brief Number of valid bytes in data. */
    uint16_t len;
    /** @brief Bytes to transmit. */
    uint8_t data[CONFIG_SERIALPIPE_TX_ITEM_SIZE];
};

/*  Received bytes accumulate here as a byte stream rather than as discrete ISR
    chunks: chunk boundaries are an artifact of interrupt timing and carry no
    meaning in a module that does no framing. Keeping them would force the
    caller's buffer to be at least as large as a chunk. */
static SwFifo rx_fifo;
static uint8_t rx_fifo_mem[SwFifo_getMemAllocSize(CONFIG_SERIALPIPE_RX_FIFO_DEPTH, 1)];

/** @brief Set by the receive callback when bytes were dropped; reported and
    cleared by SerialPipe_rxGet(). volatile as it is written in interrupt
    context and read in the caller's thread. */
static volatile bool rx_overrun;

/* Memory slab for data to transmit. */
K_MEM_SLAB_DEFINE_STATIC(tx_pool, sizeof(struct tx_item),
    CONFIG_SERIALPIPE_TX_DEPTH, 4);
/* Transmit data fifo. */
K_FIFO_DEFINE(tx_fifo);

/******************************************************************************
    recv_cb
*//**
    @brief Callback for uart-pipe receive data. Runs in ISR context.

    Everything available is consumed on every call: the offset is reset and the
    same buffer handed back, so the driver never accumulates across calls and
    len can never exceed the registered buffer size.

    @param[in] buf  Buffer holding received data.
    @param[in,out] off  Running buffer offset. Reset to 0 on return.
    @return Returns the buffer to use for the next receive.
******************************************************************************/
static uint8_t *
recv_cb(uint8_t *buf, size_t *off)
{
    uint32_t len = (uint32_t)*off;
    uint32_t num;

    /* We always consume all the data, reset the offset for the next call. */
    *off = 0;

    if (!initialized || (len == 0))
    {
        return buf;
    }

    /*  Write what fits rather than failing the whole chunk, so an overflow
        costs only the bytes that could not be stored. */
    num = MIN(len, SwFifo_getAvail(&rx_fifo));
    if (num > 0)
    {
        SwFifo_write(&rx_fifo, buf, num);
    }

    /*  Record the drop for rxGet() to report. Warning from here would mean a
        console write inside the receive interrupt under
        CONFIG_LOG_MODE_IMMEDIATE, at exactly the moment the system is already
        failing to drain the fifo. */
    if (num < len)
    {
        rx_overrun = true;
    }

    return buf;
}

/******************************************************************************
    tx_thread
*//**
    @brief Thread which drains the transmit fifo.

    Being the only caller of uart_pipe_send() means concurrent senders cannot
    interleave their bytes on the wire.
******************************************************************************/
static void
tx_thread(void *p1, void *p2, void *p3)
{
    (void)p1;
    (void)p2;
    (void)p3;

    /* Hold off until initialized, then re-post so any thread added later also
       passes through. */
    k_sem_take(&init_sem, K_FOREVER);
    k_sem_give(&init_sem);

    while (1)
    {
        struct tx_item *item = k_fifo_get(&tx_fifo, K_FOREVER);

        uart_pipe_send(item->data, item->len);

        k_mem_slab_free(&tx_pool, (void *)item);
    }
}

K_THREAD_DEFINE(
    serpipe_tx,
    CONFIG_SERIALPIPE_TX_STACK_SIZE,
    tx_thread,
    NULL,
    NULL,
    NULL,
    CONFIG_SERIALPIPE_TX_THREAD_PRIO,
    0,
    0);

/******************************************************************************
    [docimport SerialPipe_send]
*//**
    @brief Queues bytes for transmission.

    The bytes are copied into a transmit item and queued. An internal thread
    drains the queue and performs the (blocking) uart-pipe write, so this
    function does not stall the caller for the duration of the transfer. Safe to
    call from an ISR.

    @param[in] data  Pointer to the bytes to send.
    @param[in] len  Number of bytes to send.
    @return Returns 0 on success, -EINVAL on a bad argument, -EAGAIN if not yet
    initialized, -EMSGSIZE if len exceeds CONFIG_SERIALPIPE_TX_ITEM_SIZE,
    -ENOMEM if the transmit pool is exhausted.
******************************************************************************/
int
SerialPipe_send(const uint8_t *data, uint16_t len)
{
    struct tx_item *item;

    CHECK_COND_RETURN_MSG(!data || (len == 0), -EINVAL, "Bad argument.");
    CHECK_COND_RETURN_MSG(!initialized, -EAGAIN, "Not initialized.");
    CHECK_COND_RETURN_MSG(len > sizeof(item->data), -EMSGSIZE, "Send too large.");

    if (k_mem_slab_alloc(&tx_pool, (void **)&item, K_NO_WAIT) < 0)
    {
        LOG_ERR("Transmit pool exhausted, dropping %u bytes.", len);
        return -ENOMEM;
    }

    item->len = len;
    memcpy(item->data, data, len);

    k_fifo_put(&tx_fifo, item);

    return 0;
}

/******************************************************************************
    [docimport SerialPipe_sendSize]
*//**
    @brief Gets the largest transfer SerialPipe_send() will accept.

    Provided so callers can size their buffers up front rather than discovering
    the limit from a failed send.

    @return Returns the maximum number of bytes accepted by a single send.
******************************************************************************/
size_t
SerialPipe_sendSize(void)
{
    return (size_t)CONFIG_SERIALPIPE_TX_ITEM_SIZE;
}

/******************************************************************************
    [docimport SerialPipe_rxGet]
*//**
    @brief Takes whatever received bytes are waiting, without blocking.

    Received bytes accumulate in a byte fifo, so the caller may take as many or
    as few as it likes - any buffer size works, down to a single byte. Returns
    immediately in every case: a caller with nothing to do until more arrives
    sleeps and asks again, which keeps the waiting policy where the caller can
    see it rather than buried in here.

    Single consumer only: one thread should call this. A byte stream split
    between several readers, each getting an arbitrary fragment, has no meaning.

    A receive fifo that fills drops the bytes that do not fit, and this function
    logs a warning the next time it is called. The caller is not told in the
    return value, so bytes may be missing from the middle of the stream. Size
    CONFIG_SERIALPIPE_RX_FIFO_DEPTH so this does not happen.

    @param[in,out] data  Buffer to copy the received bytes into.
    @param[in] len  Size of the caller's buffer.
    @return Returns the number of bytes copied (0 if none were waiting), -EINVAL
    on a bad argument, -EAGAIN if not yet initialized.
******************************************************************************/
int
SerialPipe_rxGet(uint8_t *data, size_t len)
{
    CHECK_COND_RETURN_MSG(!data || (len == 0), -EINVAL, "Bad argument.");
    CHECK_COND_RETURN_MSG(!initialized, -EAGAIN, "Not initialized.");

    /*  Report what the callback recorded, in thread context. Cleared before
        logging, so a receive landing during the log call is reported by the next
        one rather than lost. */
    if (rx_overrun)
    {
        rx_overrun = false;
        LOG_WRN("Receive fifo overrun, bytes were dropped.");
    }

    return (int)SwFifo_read(&rx_fifo, data, len);
}

/******************************************************************************
    [docimport SerialPipe_rxAvail]
*//**
    @brief Gets the number of received bytes waiting to be read.
    @return Returns the number of bytes available.
******************************************************************************/
uint32_t
SerialPipe_rxAvail(void)
{
    return SwFifo_getCount(&rx_fifo);
}

/******************************************************************************
    [docimport SerialPipe_flush]
*//**
    @brief Discards all pending received items.
******************************************************************************/
void
SerialPipe_flush(void)
{
    SwFifo_flush(&rx_fifo);

    /* The gap it refers to is in data the caller just discarded. */
    rx_overrun = false;
}

/******************************************************************************
    [docimport SerialPipe_init]
*//**
    @brief Initializes the serial pipe.

    Registers the receive callback with the uart-pipe driver and releases the
    internal transmit thread. Must be called before any other SerialPipe
    function.

    @return Returns 0 on success, -EALREADY if already initialized.
******************************************************************************/
int
SerialPipe_init(void)
{
    int ret;

    CHECK_COND_RETURN_MSG(initialized, -EALREADY, "Already initialized.");

    LOG_INF("Initializing SerialPipe (rx fifo %u bytes, tx %u x %u).",
            (uint32_t)CONFIG_SERIALPIPE_RX_FIFO_DEPTH,
            (uint32_t)CONFIG_SERIALPIPE_TX_DEPTH,
            (uint32_t)CONFIG_SERIALPIPE_TX_ITEM_SIZE);

    /*  Threadsafe: written from the uart-pipe ISR and read from the caller's
        thread. SwFifo takes a spinlock for us, so no locking is needed here. */
    ret = SwFifo_init(&rx_fifo, "spipe_rx", CONFIG_SERIALPIPE_RX_FIFO_DEPTH,
                      sizeof(uint8_t), rx_fifo_mem, sizeof(rx_fifo_mem), true);
    CHECK_COND_RETURN_MSG(ret < 0, ret, "Error initializing receive fifo.");

    /* Set before registering so the callback never observes a false flag. */
    initialized = true;

    uart_pipe_register(serial_buf, sizeof(serial_buf), recv_cb);

    /* Release the transmit thread. */
    k_sem_give(&init_sem);

    return 0;
}
