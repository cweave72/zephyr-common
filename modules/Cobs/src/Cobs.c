/*******************************************************************************
 *  @file: Cobs.c
 *  
 *  @brief: Library for performing COBS (Consistent Overhead Byte Encoding)
    encoding.
*******************************************************************************/
#include <stdbool.h>
#include "Cobs.h"
#include <zephyr/logging/log.h>

/** @brief Initialize the logging module. */
LOG_MODULE_REGISTER(Cobs, LOG_LEVEL_DBG);

#define ESCAPED_BYTE    0x00

#define CHECK_OVERFLOW(cond)                               \
if ((cond)) {                                              \
    LOG_ERR("Overflow writing output.");                   \
    return -1;                                             \
}

/******************************************************************************
    [docimport Cobs_encode]
*//**
    @brief Performs COBS encoding on the input buffer.
    Note, this encoder does not apply the tail framing byte.

    @param[in] buf_in  Pointer to input data buffer.
    @param[in] buf_in_len  Number of bytes in buf_in.
    @param[in] enc_out  Pointer to encoded output buffer.
    @param[in] max_enc_len  Max size of the output buffer.
    @return Returns the encoded output size on success, -1 on failure.
******************************************************************************/
int
Cobs_encode(
    uint8_t *buf_in,
    uint32_t buf_in_len,
    uint8_t *enc_out,
    uint32_t max_enc_len)
{
    int count = 0;
    int code_word_idx = 0;
    int rd_idx = 0;
    int wr_idx;

    while (rd_idx != buf_in_len)
    {
        uint8_t byte = buf_in[rd_idx++];
        count++;

        if (byte == ESCAPED_BYTE || count == 255)
        {
            enc_out[code_word_idx] = count;
            /* Advance code_word_idx by the curent count */
            code_word_idx += count;

            if (count == 255)
            {
                /*  Since we're stuffing an 0xff byte, we need to still write
                    the current non-null byte. Advance to the next non-codeword
                    position and write data. */
                wr_idx = code_word_idx + 1;
                CHECK_OVERFLOW(wr_idx == max_enc_len);
                enc_out[wr_idx] = byte;
                count = 1;
            }
            else
            {
                count = 0;
            }
        }
        else
        {
            /*  Non-null byte, store in the output buffer. */
            wr_idx = code_word_idx + count;
            CHECK_OVERFLOW(wr_idx == max_enc_len);
            enc_out[wr_idx] = byte;
        }
    }

    enc_out[code_word_idx] = count + 1;
    code_word_idx += count + 1;

    return code_word_idx;
}

/******************************************************************************
    [docimport Cobs_decode]
*//**
    @brief Decode a COBS encoded stream.
    @param[in] enc_in Pointer to encoded input byte stream.
    @param[in] enc_in_len  Length of the input stream.
    @param[in] buf_out  Pointer to decoded output buffer.
    @param[in] max_buf_out  Maximum decoded output buffer length.
    @return Returns the length of the encoded output.
******************************************************************************/
int
Cobs_decode(
    uint8_t *enc_in,
    uint32_t enc_in_len,
    uint8_t *buf_out,
    uint32_t max_buf_out)
{
    uint8_t *pCode = enc_in;
    uint8_t *pData = enc_in + 1;
    uint8_t *pBuf = buf_out;
    uint8_t count;
    int num_out = 0;
    
    while (1)
    {
        for (count = 1; count < *pCode; count++)
        {
            CHECK_OVERFLOW(num_out == max_buf_out);
            *pBuf++ = *pData++;
            num_out++;
        }

        pCode += count;
        pData = pCode + 1;

        if (pCode == enc_in + enc_in_len)
        {
            break;
        }

        if (count == 255)
        {
            continue;
        }

        CHECK_OVERFLOW(num_out == max_buf_out);
        *pBuf++ = 0;
        num_out++;
    }

    return num_out;
}
