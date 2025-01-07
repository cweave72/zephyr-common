/*******************************************************************************
 *  @file: Cobs_frame.h
 *   
 *  @brief: Header for Cobs framer/deframer.
*******************************************************************************/
#ifndef COBS_FRAME_H
#define COBS_FRAME_H

#include <stdint.h>
#include "SwFifo.h"

/** @brief COBS Framer/Deframer object.
*/
typedef struct Cobs_Deframer
{
    /** @brief Deframer State. */
    uint8_t state;
    /** @brief Working buffer. */
    uint8_t *work;
    uint32_t work_size;
    /** @brief Fifo used for deframer stream storage. */
    SwFifo fifo;
    /** @brief Current byte count. */
    uint16_t count;

} Cobs_Deframer;


/******************************************************************************
    [docexport Cobs_framer]
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
    uint32_t max_enc_len);

/******************************************************************************
    [docexport Cobs_deframer]
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
    uint32_t max_buf_out);

/******************************************************************************
    [docexport Cobs_frame_init]
*//**
    @brief Initializes a COBS framer/deframer.
    @param[in] self  Pointer to uninitialized Cobs_Deframer object.
    @param[in] buf_depth  Buffer depth for the internal deframer fifo.
    A reasonable value is 1024.
    @return Returns 0 on success, -1 on error.
******************************************************************************/
int
Cobs_deframer_init(void *self, uint16_t buf_depth);
#endif
