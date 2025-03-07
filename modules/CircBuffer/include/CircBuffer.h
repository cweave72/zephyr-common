/*******************************************************************************
 *  @file: CircBuffer.h
 *   
 *  @brief: Header for circular buffer
*******************************************************************************/
#ifndef CIRCBUFFER_H
#define CIRCBUFFER_H

#include <stdint.h>
#include <zephyr/kernel.h>
#include "SwFifo.h"

typedef struct CircBuffer
{
    SwFifo hist_fifo;
    uint8_t *hist_fifo_mem;
    uint8_t *buf;
    uint32_t buf_size;
    int wr_idx;
    int rd_idx;
    struct k_mutex mtx;

} CircBuffer;

/** @brief Macro for obtaining the proper memory allocation size for a
      CircBuffer when statically allocating memory.
    Ex: static uint8_t buf[CircBuffer_getMemAllocSize(15)];
  */
#define CircBuffer_getMemAllocSize(depth)   ((depth)+1)

/******************************************************************************
    [docexport CircBuffer_flush]
*//**
    @brief Flushes the buffer.
    @param[in] circ  Pointer to CircBuffer instance.
******************************************************************************/
void
CircBuffer_flush(CircBuffer *circ);

/******************************************************************************
    [docexport CircBuffer_write]
*//**
    @brief Writes to the circular buffer.  As the buffer wraps, old items are
    purged to make room available for new data.
    @param[in] circ  Pointer to CircBuffer instance.
    @param[in] data  Pointer to data to write.
    @param[in] size  Size of data to write.
******************************************************************************/
int
CircBuffer_write(CircBuffer *circ, uint8_t *data, uint16_t size);

/******************************************************************************
    [docexport CircBuffer_getCount]
*//**
    @brief Returns the number of bytes in the buffer.
    @param[in] circ  Pointer to CircBuffer instance.
******************************************************************************/
uint32_t
CircBuffer_getCount(CircBuffer *circ);

/******************************************************************************
    [docexport CircBuffer_read]
*//**
    @brief Reads contents of the buffer.
    @param[in] circ  Pointer to CircBuffer instance.
    @param[in] buf  Pointer to buffer to write to.
    @param[in] req_size  Requested size to read from buffer.
    @return Returns the number read or negative error code.
******************************************************************************/
int
CircBuffer_read(CircBuffer *circ, uint8_t *buf, uint32_t req_size);

/******************************************************************************
    [docexport CircBuffer_init]
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
    uint32_t max_items);
#endif
