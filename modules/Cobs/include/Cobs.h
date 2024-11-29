/*******************************************************************************
 *  @file: Cobs.h
 *   
 *  @brief: Header for COBS encoder.
*******************************************************************************/
#ifndef COBS_H
#define COBS_H

#include <stdint.h>

/******************************************************************************
    [docexport Cobs_encode]
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
    uint32_t max_enc_len);

/******************************************************************************
    [docexport Cobs_decode]
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
    uint32_t max_buf_out);
#endif
