/*******************************************************************************
 *  @file: Cobs_frame.c
 *  
 *  @brief: Library for performing COBS framing and de-framing.
*******************************************************************************/
#include <stdbool.h>
#include "Cobs_frame.h"
#include "Cobs.h"
#include "CheckCond.h"
#include <zephyr/logging/log.h>

/** @brief Initialize the logging module. */
LOG_MODULE_REGISTER(Cobs_frame, CONFIG_COBS_LOG_LEVEL);

#define FRAMING_BYTE        0x00

enum {
    INIT = 0,
    FIND_SOF,
    FIND_EOF,
    DECODE,
    ERROR
};

/******************************************************************************
    [docimport Cobs_framer]
*//**
    @brief Applies COBS framing to the supplied buffer.
    @param[in] buf_in  Pointer to input data buffer.
    @param[in] buf_in_len  Number of bytes in buf_in.
    @param[in] enc_out  Pointer to encoded output buffer.
    @param[in] max_enc_len  Max size of the output buffer.
    @return Returns the framed output size on success, -1 on failure.
******************************************************************************/
int
Cobs_framer(
    uint8_t *buf_in,
    uint32_t buf_in_len,
    uint8_t *enc_out,
    uint32_t max_enc_len)
{
    int num = Cobs_encode(buf_in, buf_in_len, enc_out + 1, max_enc_len);
    CHECK_COND_RETURN_MSG(num <= 0, -1, "COBS encode failed.");
    *enc_out = FRAMING_BYTE;
    if (num + 2 >= max_enc_len)
    {
        LOG_ERR("Overflow during framing.");
        return -1;
    }

    *(enc_out + 1 + num) = FRAMING_BYTE;
    return num + 2;
}



/******************************************************************************
    [docimport Cobs_deframer]
*//**
    @brief Performs COBS deframing on the incoming bytestream.
    @param[in] self  Pointer to Cobs_Deframer object.
    @param[in] buf_in  Pointer to input data buffer (new data).
    @param[in] buf_in_len  Length of new bytes in buf_in.
    @param[in] buf_out  Pointer to output buffer where deframed data will be
    written.
    @param[in] max_buf_out  Max size of buf_out.
    @return Returns the size of the deframed buffer on success, -1 on error.
******************************************************************************/
int
Cobs_deframer(
    void *self,
    uint8_t *buf_in,
    uint32_t buf_in_len,
    uint8_t *buf_out,
    uint32_t max_buf_out)
{
    Cobs_Deframer *deframer = (Cobs_Deframer *)self;
    SwFifo *fifo = &deframer->fifo;
    uint8_t *work = deframer->work;
    uint32_t work_size = deframer->work_size;
    int ret;

    ret = SwFifo_write(fifo, (void *)buf_in, buf_in_len);
    if (ret < 0)
    {
        LOG_ERR("Not enough space in fifo for writing %u bytes.",
            (unsigned int)buf_in_len);
        SwFifo_flush(fifo);
        deframer->state = INIT;
        return 0;
    }

    LOG_DBG("Wrote %u bytes into deframer fifo.",
        (unsigned int)buf_in_len);
    
    while (1)
    {
        uint32_t avail;
        uint8_t byte;
        bool found_eof;
        int i;
        int num;

        switch (deframer->state)
        {
        case INIT:
            LOG_DBG("INIT: avail=%u", (unsigned int)SwFifo_getCount(fifo));
            deframer->count = 0;
            deframer->state = FIND_SOF;
            break;

        case FIND_SOF:
            avail = SwFifo_getCount(fifo);
            if (avail == 0)
            {
                LOG_ERR("FIND_SOF: Fifo is empty (buf_in_len was %u).", 
                    buf_in_len);
                deframer->state = INIT;
                return 0;
            }

            LOG_DBG("FIND_SOF: avail=%u", (unsigned int)avail);

            /* Search for FRAMING_BYTE in fifo, remove and discard non-framing
                bytes. */
            for (i = 0; i < avail; i++)
            {
                SwFifo_read(fifo, (void *)&byte, 1);

                if (byte == FRAMING_BYTE)
                {
                    LOG_DBG("FIND_SOF: Found Start of frame i=%u.", i);
                    /* Now, look for end of frame. */
                    deframer->state = FIND_EOF;
                    i++;
                    /* Break out of for loop. */
                    break;
                }
            }

            LOG_DBG("FIND_SOF: Read %u bytes", i);
            break;

        case FIND_EOF:
            found_eof = false;
            avail = SwFifo_getCount(fifo);
            if (avail == 0)
            {
                LOG_DBG("FIND_EOF: Fifo is empty.");
                return 0;
            }

            LOG_DBG("FIND_EOF: avail=%u", (unsigned int)avail);

            /* First encoded byte is sitting at top of fifo. Read until EOF or
                all bytes exhausted, copying into output buffer. */
            for (i = 0; i < avail; i++)
            {
                /* Check for work buf overflow */
                if (deframer->count == work_size)
                {
                    LOG_ERR("FIND_EOF: work buffer overflow (size=%u; buf_in_len=%u).",
                        deframer->count, buf_in_len);
                    deframer->state = ERROR;
                    break;
                }

                SwFifo_read(fifo, (void *)(work + deframer->count), 1);

                LOG_DBG("work[%u]=0x%02x", (unsigned int)deframer->count,
                    (unsigned int)work[deframer->count]);

                /* Was the byte just read the FRAMING_BYTE? */
                if (work[deframer->count] == FRAMING_BYTE)
                {
                    deframer->state = DECODE;
                    found_eof = true;
                    LOG_DBG("FIND_EOF: Found End of frame count=%u.",
                        (unsigned int)deframer->count);
                    break;
                }
                deframer->count++;
            }

            /* If we're here and we haven't found EOF, we need to return and let
                more data be received. We'll continue searching for EOF on next
                entry. */
            if (!found_eof)
            {
                return 0;
            }
            break;

        case DECODE:
            num = Cobs_decode(work, deframer->count, buf_out, max_buf_out);
            if (num < 0)
            {
                LOG_ERR("DECODE: Error during COBS decode :%d", num);
                deframer->state = ERROR;
                break;
            }
            LOG_DBG("DECODE: Decoded size is %u bytes (avail=%u).",
                num, (unsigned int)SwFifo_getCount(fifo));
            deframer->state = INIT;
            return num;

        case ERROR:
            SwFifo_flush(fifo);
            deframer->state = INIT;
            return 0;

        default:
            break;
        }
    }

    /* We should never get here. */
    return 0;
}

/******************************************************************************
    [docimport Cobs_frame_init]
*//**
    @brief Initializes a COBS framer/deframer.
    @param[in] self  Pointer to uninitialized Cobs_Deframer object.
    @param[in] buf_depth  Buffer depth for the internal deframer fifo.
    A reasonable value is 1024.
    @return Returns 0 on success, -1 on error.
******************************************************************************/
int
Cobs_deframer_init(void *self, uint16_t buf_depth)
{
    int status;

    Cobs_Deframer *deframer = (Cobs_Deframer *)self;
    deframer->state = INIT;
    deframer->work = (uint8_t *)malloc(buf_depth);
    deframer->work_size = buf_depth;
    CHECK_COND_RETURN_MSG(!deframer->work, -ENOMEM, "Out of memory.");

    /** @brief Initialize the fifo. */
    status = SwFifo_init(
        &deframer->fifo,
        "cobs_deframer",
        buf_depth,
        sizeof(uint8_t),
        NULL,
        0,
        false);
    CHECK_COND_RETURN_MSG(status < 0, -1, "Error initializing fifo.");

    return 0;
}
