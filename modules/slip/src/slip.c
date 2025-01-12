/*******************************************************************************
 *  @file: slip.c
 *  
 *  @brief: Library for performing SLIP framing and de-framing.
*******************************************************************************/
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "slip.h"
#include "CheckCond.h"

/** @brief Initialize the logging module. */
LOG_MODULE_REGISTER(slip_frame, CONFIG_SLIP_FRAME_LOG_LEVEL);

#define END     0xC0
#define ESC     0xDB
#define ESC_END 0xDC
#define ESC_ESC 0xDD

enum {
    INIT = 0,
    FIND_SOF,
    FIND_EOF
};

/******************************************************************************
    [docimport slip_framer]
*//**
    @brief Applies SLIP framing to the supplied buffer.
    @param[in] buf_in  Pointer to input data buffer.
    @param[in] buf_in_len  Number of bytes in buf_in.
    @param[in] buf_out  Pointer to encoded output buffer.
    @param[in] buf_out_max  Max size of the output buffer.
    @return Returns the framed output size on success, -1 on failure.
******************************************************************************/
int
slip_framer(
    uint8_t *buf_in,
    uint32_t buf_in_len,
    uint8_t *buf_out,
    uint32_t buf_out_max)
{
    uint32_t idx = 1;
    uint32_t k;

    buf_out[0] = END;
    for (k = 0; k < buf_in_len; k++)
    {
        uint8_t c = buf_in[k];

        if (idx == buf_out_max)
        {
            LOG_ERR("Overflow buf_out.");
            return -EOVERFLOW;
        }

        switch (c)
        {
        case END:
            buf_out[idx++] = ESC;
            buf_out[idx++] = ESC_END;
            break;
        case ESC:
            buf_out[idx++] = ESC;
            buf_out[idx++] = ESC_ESC;
            break;
        default:
            buf_out[idx++] = c;
        }
    }

    buf_out[idx++] = END;
    return (int)idx;
}


/******************************************************************************
    [docimport slip_deframer]
*//**
    @brief Performs SLIP deframing on the incoming bytestream.
    @param[in] self  Pointer to slip_deframer object.
    @param[in] buf_in  Pointer to input data buffer (new data).
    @param[in] buf_in_len  Length of new bytes in buf_in.
    @param[in] buf_out  Pointer to output buffer where deframed data will be
    written.
    @param[in] buf_out_max  Max size of buf_out.
    @return Returns the size of the deframed buffer on success, Negative on
    error.
******************************************************************************/
int
slip_deframer(
    void *self,
    uint8_t *buf_in,
    uint32_t buf_in_len,
    uint8_t *buf_out,
    uint32_t buf_out_max)
{
    slip_deframer_ctx *deframer = (slip_deframer_ctx *)self;
    SwFifo *fifo = &deframer->fifo;
    int ret;

    ret = SwFifo_write(fifo, (void *)buf_in, buf_in_len);
    if (ret < 0)
    {
        LOG_ERR("Not enough space in fifo for writing %u bytes.", buf_in_len);
        SwFifo_flush(fifo);
        deframer->state = INIT;
        return 0;
    }

    while (1)
    {
        uint32_t avail;
        uint8_t byte;
        int i;

        switch (deframer->state)
        {
        case INIT:
            LOG_DBG("INIT: avail=%u", SwFifo_getCount(fifo));
            deframer->idx = 0;
            deframer->get_escaped = false;
            deframer->state = FIND_SOF;
            break;

        case FIND_SOF:
            avail = SwFifo_getCount(fifo);
            if (avail == 0)
            {
                LOG_DBG("FIND_SOF: Fifo is empty (buf_in_len=%u).", 
                    buf_in_len);
                return 0;
            }

            LOG_DBG("FIND_SOF: avail=%u", avail);

            /* Search for END in fifo, remove and discard non-framing bytes. */
            for (i = 0; i < avail; i++)
            {
                SwFifo_read(fifo, (void *)&byte, 1);

                if (byte == END)
                {
                    LOG_DBG("FIND_SOF: Found Start of frame i=%u.", i);
                    /* Now, look for end of frame. */
                    deframer->state = FIND_EOF;
                    i++;
                    /* Break out of for loop. */
                    break;
                }
            }

            LOG_DBG("FIND_SOF: Read %u bytes.", i);
            break;

        case FIND_EOF:
            avail = SwFifo_getCount(fifo);
            if (avail == 0)
            {
                LOG_DBG("FIND_EOF: Fifo is empty.");
                return 0;
            }

            LOG_DBG("FIND_EOF: avail=%u, idx=%u", avail, deframer->idx);

            /*  First encoded byte is sitting at top of fifo. Read until EOF or
                all bytes exhausted, copying into output buffer. */
            for (i = 0; i < avail; i++)
            {
                /* Check for work buf overflow */
                if (deframer->idx == buf_out_max)
                {
                    LOG_ERR("FIND_EOF: buf_out overflow "
                            "(idx=%u; buf_in_len=%u).",
                             deframer->idx, buf_in_len);
                    deframer->state = INIT;
                    SwFifo_flush(fifo);
                    return -EOVERFLOW;
                }

                /* Read the next byte from the fifo. */
                SwFifo_read(fifo, (void *)&byte, 1);

                if (byte == END)
                {
                    LOG_DBG("--> Found EOF. len=%u", deframer->idx);
                    deframer->state = INIT;
                    return deframer->idx;
                }
                else if (byte == ESC)
                {
                    /* Escaped byte, get next byte. */
                    deframer->get_escaped = true;
                    LOG_DBG("Escape char: idx=%u", deframer->idx);
                    continue;
                }
                else if (byte == ESC_ESC && deframer->get_escaped)
                {
                    deframer->get_escaped = false;
                    LOG_DBG("ESC_ESC %u", deframer->idx);
                    buf_out[deframer->idx++] = ESC;
                }
                else if (byte == ESC_END && deframer->get_escaped)
                {
                    deframer->get_escaped = false;
                    LOG_DBG("ESC_END %u", deframer->idx);
                    buf_out[deframer->idx++] = END;
                }
                else if (deframer->get_escaped)
                {
                    LOG_ERR("Incorrect ESC char sequence received.");
                    deframer->state = INIT;
                    SwFifo_flush(fifo);
                    return -EINVAL;
                }
                else
                {
                    buf_out[deframer->idx++] = byte;
                }
            }

            /*  If we're here and we haven't found EOF, we need to return and
                let more data be received. We'll pick up where we left off on
                the next entry.
            */
            LOG_DBG("FIND_EOF: Partial packet at idx=%u", deframer->idx);
            return 0;

        default:
            LOG_ERR("Bad state (%u).", deframer->state);
            deframer->state = INIT;
            SwFifo_flush(fifo);
            return -EINVAL;
        }
    }

    /* We should never get here. */
    return 0;
}

/******************************************************************************
    [docimport slip_frame_init]
*//**
    @brief Initializes a SLIP deframer.
    @param[in] self  Pointer to uninitialized slip_deframer context object.
    @param[in] mtu  Frame MTU.
    @return Returns 0 on success, -1 on error.
******************************************************************************/
int
slip_deframer_init(void *self, uint16_t mtu)
{
    int status;

    slip_deframer_ctx *deframer = (slip_deframer_ctx *)self;
    deframer->state = INIT;

    /** @brief Initialize the fifo. */
    status = SwFifo_init(
        &deframer->fifo,
        "slip_deframer",
        2*mtu,
        sizeof(uint8_t),
        NULL,
        0,
        false);
    CHECK_COND_RETURN_MSG(status < 0, -1, "Error initializing fifo.");

    return 0;
}
