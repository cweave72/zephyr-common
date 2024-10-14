/*******************************************************************************
 *  @file: TcpEcho.c
 *  
 *  @brief: Simple TCP echo server.
*******************************************************************************/
#include "TcpSocket.h"
#include "TcpEcho.h"
#include <zephyr/logging/log.h>

/** @brief Initialize the logging module. */
LOG_MODULE_REGISTER(TcpEcho, CONFIG_TCPECHO_LOG_LEVEL);

/******************************************************************************
    echo
*//**
    @brief TcpServer callback. Echos data received back to the socket.

    @param[in] server  Reference to the underlying TcpServer object.
    @param[in] sock  The accepted socket.
    @param[in] data  Pointer to data buffer to send.
    @param[in] len  Length of data to send.
    @param[out] finished  0 = not finished, keep connection; 1 = finished.
******************************************************************************/
static void
echo_callback(void *server, int sock, uint8_t *data, uint16_t len, int *finished)
{
    /** @brief TcpEcho type masquerades as a TcpServer. */
    TcpEcho *echo_server = (TcpEcho *)server;
    int num = TcpSocket_write(sock, data, len);
    echo_server->byte_count += num;
    *finished = 1;
    LOG_DBG("Echo'd %d bytes (total: %u).", num,
        (unsigned int)echo_server->byte_count);
}

/******************************************************************************
    [docimport TcpEcho_init]
*//**
    @brief Initializes the echo server.

    @param[in] echo  Pointer to TcpEcho instance.
    @param[in] port  Port number to use.
    @param[in] buf  Pointer to user-allocated buffer used for Rx. If NULL,
    buffer will be dynamically allocated.
    @param[in] buf_len  Length of the buffer.
    @param[in] stack_size  Size of the server task stack.
    @param[in] name  Name for the task.
    @param[in] prio  Task priority.
    @return Returns 0 on success, negative on error.
******************************************************************************/
int
TcpEcho_init(
    TcpEcho *echo,
    uint16_t port,
    uint8_t *buf,
    uint32_t buf_len,
    uint16_t stack_size,
    char *name,
    uint8_t prio)
{
    echo->byte_count = 0;

    /** @brief Initialize the TcpServer. */
    return TcpServer_init(
        &echo->tcp_svr,
        port,
        buf,
        buf_len,
        stack_size,
        name,
        prio,
        echo_callback);
}
