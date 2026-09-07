/*******************************************************************************
 *  @file: SerialPipe.h
 *   
 *  @brief: Byte pipe over the chosen zephyr,uart-pipe UART.
*******************************************************************************/
#ifndef SERIALPIPE_H
#define SERIALPIPE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <zephyr/kernel.h>

/******************************************************************************
    [docexport SerialPipe_send]
*//**
    @brief Queues bytes for transmission.

    The bytes are copied into a transmit item and queued. An internal thread
    drains the queue and performs the (blocking) uart-pipe write, so this
    function does not stall the caller for the duration of the transfer. Safe to
    call from an ISR.

    @param[in] data  Pointer to the bytes to send.
    @param[in] len  Number of bytes to send.
    @return Returns 0 on success, -EINVAL on a bad argument, -EMSGSIZE if len
    exceeds CONFIG_SERIALPIPE_TX_ITEM_SIZE, -ENOMEM if the transmit pool is
    exhausted.
******************************************************************************/
int
SerialPipe_send(const uint8_t *data, uint16_t len);

/******************************************************************************
    [docexport SerialPipe_sendSize]
*//**
    @brief Gets the largest transfer SerialPipe_send() will accept.

    Provided so callers can size their buffers up front rather than discovering
    the limit from a failed send.

    @return Returns the maximum number of bytes accepted by a single send.
******************************************************************************/
size_t
SerialPipe_sendSize(void);

/******************************************************************************
    [docexport SerialPipe_rxGet]
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
SerialPipe_rxGet(uint8_t *data, size_t len);

/******************************************************************************
    [docexport SerialPipe_rxAvail]
*//**
    @brief Gets the number of received bytes waiting to be read.
    @return Returns the number of bytes available.
******************************************************************************/
uint32_t
SerialPipe_rxAvail(void);

/******************************************************************************
    [docexport SerialPipe_flush]
*//**
    @brief Discards all pending received items.
******************************************************************************/
void
SerialPipe_flush(void);

/******************************************************************************
    [docexport SerialPipe_init]
*//**
    @brief Initializes the serial pipe.

    Registers the receive callback with the uart-pipe driver and releases the
    internal transmit thread. Must be called before any other SerialPipe
    function.

    @return Returns 0 on success, -EALREADY if already initialized.
******************************************************************************/
int
SerialPipe_init(void);
#endif
