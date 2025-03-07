/*******************************************************************************
 *  @file: CircBuffer.c
 *  
 *  @brief: A circular buffer implementation for items of different sizes.
 *  Writes to this circular buffer preserve the boundaries of previously written
 *  items. When a write to the buffer causes a wrap, the oldest item is
 *  discarded.
*******************************************************************************/
#include <stdbool.h>
#include <zephyr/kernel.h>
#include "CircBuffer.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(CircBuffer, CONFIG_CIRCBUFFER_LOG_LEVEL);

/** @brief Increments the write index with circular wrap. */
#define inc_wr_idx(pc, n)                   \
do {                                        \
    (pc)->wr_idx += (n);                    \
    if (pc->wr_idx > pc->buf_size) {        \
        pc->wr_idx -= (pc->buf_size + 1);   \
    }                                       \
} while (0)

/** @brief Increments the read index with circular wrap. */
#define inc_rd_idx(pc, n)                  \
do {                                       \
    (pc)->rd_idx += (n);                   \
    if (pc->rd_idx > pc->buf_size) {       \
        pc->rd_idx -= (pc->buf_size + 1);  \
    }                                      \
} while (0)

/** @brief Macros to get pointers into memory for write and read. */
#define getMemPtr_wr(pc)   ((pc)->buf + (pc)->wr_idx)
#define getMemPtr_rd(pc)   ((pc)->buf + (pc)->rd_idx)

/** @brief Macro for current fifo count (number of items in fifo). */
#define count(pc)                                                     \
    (((pc)->wr_idx >= (pc)->rd_idx) ? ((pc)->wr_idx - (pc)->rd_idx) : \
    (((pc)->buf_size - (pc)->rd_idx) + (pc)->wr_idx + 1))

/** @brief Macro for checking fifo space available. */
#define avail(pc)        ((pc)->buf_size - count((pc)))

/** @brief Macro for checkinf fifo empty status. */
#define isEmpty(pc)      (((pc)->wr_idx == (pc)->rd_idx) ? 1 : 0)

/** @brief Macro for checking fifo full status. */
#define isFull(pc)       ((count((pc)) == (pc)->buf_size) ? 1 : 0)

#define lock(pc)    k_mutex_lock(&(pc)->mtx, K_FOREVER)
#define unlock(pc)  k_mutex_unlock(&(pc)->mtx)

#define CIRC_MIN(a, b)  ((a < b) ? (a) : (b))

/******************************************************************************
    circ_write
*//**
    @brief Writes data to circular array.
******************************************************************************/
static void
circ_write(CircBuffer *circ, void *data, uint32_t size)
{
    uint32_t numToWrap;
    uint8_t *p = getMemPtr_wr(circ);

    /* Check if the write is expected to wrap. */
    if (circ->wr_idx + size > circ->buf_size)
    {
        /* Number of writes to get to the ending address of the buffer. */
        numToWrap = circ->buf_size - circ->wr_idx + 1;
        /* Write to the last avail memory location prior to wrap. */
        memcpy(p, data, numToWrap);

        /* Complete the write from the beginning of the circular buf. */
        if (size - numToWrap > 0)
        {
            memcpy(circ->buf, (uint8_t *)data + numToWrap, size - numToWrap);
        }
    }
    else
    {
        /* Standard write with no wrap. */
        memcpy(p, data, size);
    }
}

/******************************************************************************
    circ_read
*//**
    @brief Reads data from circular array.
******************************************************************************/
static void
circ_read(CircBuffer *circ, void *data, uint32_t num)
{
    uint32_t numToWrap;
    uint8_t *p = getMemPtr_rd(circ);

    /* Check if the write is expected to wrap. */
    if (circ->rd_idx + num > circ->buf_size)
    {
        /* Number of reads to get to the address just prior to wrap. */
        numToWrap = circ->buf_size - circ->rd_idx + 1;
        /* Read up to the last avail memory location prior to wrap. */
        memcpy(data, p, numToWrap);
        /* Complete the read from the beginning of the circular mem. */
        memcpy((uint8_t *)data + numToWrap, circ->buf, num - numToWrap);
    }
    else
    {
        /* Standard write with no wrap. */
        memcpy(data, p, num);
    }
}

/******************************************************************************
    [docimport CircBuffer_flush]
*//**
    @brief Flushes the buffer.
    @param[in] circ  Pointer to CircBuffer instance.
******************************************************************************/
void
CircBuffer_flush(CircBuffer *circ)
{
    lock(circ);
    SwFifo_flush(&circ->hist_fifo);
    circ->wr_idx = 0;
    circ->rd_idx = 0;
    unlock(circ);
}

/******************************************************************************
    [docimport CircBuffer_write]
*//**
    @brief Writes to the circular buffer.  As the buffer wraps, old items are
    purged to make room available for new data.
    @param[in] circ  Pointer to CircBuffer instance.
    @param[in] data  Pointer to data to write.
    @param[in] size  Size of data to write.
******************************************************************************/
int
CircBuffer_write(CircBuffer *circ, uint8_t *data, uint16_t size)
{
    SwFifo *fifo = &circ->hist_fifo;
    int ret = 0;

    lock(circ);

    LOG_DBG("Write %4u bytes: wr_idx: %4u; rd_idx: %4u; count: %4u; avail: %4u; items: %4u",
            size, circ->wr_idx, circ->rd_idx, count(circ), avail(circ),
            SwFifo_getCount(fifo));

    if (SwFifo_isFull(&circ->hist_fifo))
    {
        LOG_ERR("CircBuffer history fifo is full: (%u)",
            SwFifo_getCount(&circ->hist_fifo));
        ret = -1;
        goto exit;
    }

    /*  If the requested size is greater than the available space, we need
        to pop old items from the buffer (effectivly throwing out old items)
        until there is space to fit the new data. */
    while (size > avail(circ))
    {
        uint16_t oldest_size;
        SwFifo_read(fifo, &oldest_size, 1);
        inc_rd_idx(circ, oldest_size);
        LOG_DBG("Popped %u bytes (now avail: %u).", oldest_size, avail(circ));
    }

    /* Now copy the data to memory at the current wr_idx location */
    circ_write(circ, data, size);

    /* Advance the write index, accounting for wrap. */
    inc_wr_idx(circ, size);

    /* Push newest write size into the history fifo. */
    SwFifo_write(fifo, &size, 1);
    
exit:
    unlock(circ);
    return ret;
}

/******************************************************************************
    [docimport CircBuffer_getCount]
*//**
    @brief Returns the number of bytes in the buffer.
    @param[in] circ  Pointer to CircBuffer instance.
******************************************************************************/
uint32_t
CircBuffer_getCount(CircBuffer *circ)
{
    return count(circ);
}


/******************************************************************************
    [docimport CircBuffer_read]
*//**
    @brief Reads contents of the buffer.
    @param[in] circ  Pointer to CircBuffer instance.
    @param[in] buf  Pointer to buffer to write to.
    @param[in] req_size  Requested size to read from buffer.
    @return Returns the number read or negative error code.
******************************************************************************/
int
CircBuffer_read(CircBuffer *circ, uint8_t *buf, uint32_t req_size)
{
    SwFifo *fifo = &circ->hist_fifo;
    uint32_t num_read = 0;
    uint32_t num_avail = 0;
    uint32_t count;
    int ret = 0;

    lock(circ);

    num_avail = avail(circ);
    count = count(circ);

    LOG_DBG("Read  %4u bytes: wr_idx: %4u; rd_idx: %4u; count: %4u; avail: %4u; items: %4u",
            req_size, circ->wr_idx, circ->rd_idx, count(circ), avail(circ),
            SwFifo_getCount(fifo));

    if (count == 0)
    {
        LOG_WRN("Read of empty fifo");
        ret = 0;
        goto exit;
    }

    while (1)
    {
        uint16_t block_size, num;
        num = SwFifo_peek(fifo, &block_size, 1);
        if (num == 0)
        {
            ret = num_read;
            break;
        }

        /* Break if reading the next block puts us over the requested size. */
        if (num_read + block_size > req_size)
        {
            if (num_read == 0)
            {
                LOG_WRN("Undersized request size (%u) for block size of %u",
                    req_size, block_size);
            }
            ret = num_read;
            break;
        }

        circ_read(circ, buf + num_read, block_size);
        inc_rd_idx(circ, block_size);

        SwFifo_ack(fifo, 1);
        num_read += block_size;
        LOG_DBG("Reading block: %u bytes (total=%u) %u.",
            block_size, num_read, SwFifo_getCount(fifo));
    }

exit:
    unlock(circ);
    return ret;
}

/******************************************************************************
    [docimport CircBuffer_init]
*//**
    @brief Initializer for CircBuffer.
    @param[in] circ  Pointer to CircBuffer instance.
    @param[in] depth  Depth of the circular buffer.
    @param[in] buf  Pointer to pre-allocated buffer (NULL to malloc).
    If pre-allocating, use CircBuffer_getMemAllocSize(desired_depth) to properly
    size the buffer for the desired depth.
    @param[in] buf_size  Size of the buffer (only applies if buf != NULL).
    @param[in] max_items  Max number of items to track in the circular buffer.
******************************************************************************/
int
CircBuffer_init(
    CircBuffer *circ,
    uint32_t depth,
    uint8_t *buf,
    uint32_t buf_size,
    uint32_t max_items)
{
    int ret;

    /* Initialize the history fifo. */
    ret = SwFifo_init(
        &circ->hist_fifo,
        "hist_fifo",
        max_items,
        sizeof(uint16_t),
        NULL,
        0,
        false);
    if (ret < 0)
    {
        LOG_ERR("Error initializing history fifo: %d", ret);
        return ret;
    }

    k_mutex_init(&circ->mtx);

    if (!buf)
    {
        /*  Note we allocate 1 more than is requested to simplify determining
            buffer wrap conditions. */
        circ->buf = (uint8_t *)k_malloc(depth + 1);
        if (!circ->buf)
        {
            LOG_ERR("Out of memory.");
            return -ENOMEM;
        }
    }
    else 
    {
        if (buf_size != CircBuffer_getMemAllocSize(depth))
        {
            LOG_ERR("Invalid buffer size for depth=%u, should be %u.",
                depth, CircBuffer_getMemAllocSize(depth));
            return -EINVAL;
        }
        circ->buf = buf;
    }

    circ->buf_size = depth;
    CircBuffer_flush(circ);

    return 0;
}

