/*******************************************************************************
 *  @file: slip.h
 *   
 *  @brief: Header for SLIP framer/deframer.
*******************************************************************************/
#ifndef SLIP_FRAME_H
#define SLIP_FRAME_H

#include <stdint.h>
#include "SwFifo.h"

/** @brief SLIP Deframer object.
*/
typedef struct slip_deframer_ctx
{
    /** @brief Deframer State. */
    uint8_t state;
    /** @brief Fifo used for deframer stream storage. */
    SwFifo fifo;
    /** @brief Current byte index. */
    uint16_t idx;
    /** @brief Flag indicating we're processing an escaped char. */
    bool get_escaped;

} slip_deframer_ctx;

/******************************************************************************
    [docexport slip_framer]
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
    uint32_t buf_out_max);

/******************************************************************************
    [docexport slip_deframer]
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
    uint32_t buf_out_max);

/******************************************************************************
    [docexport slip_frame_init]
*//**
    @brief Initializes a SLIP deframer.
    @param[in] self  Pointer to uninitialized slip_deframer context object.
    @param[in] mtu  Frame MTU.
    @return Returns 0 on success, -1 on error.
******************************************************************************/
int
slip_deframer_init(void *self, uint16_t mtu);
#endif
