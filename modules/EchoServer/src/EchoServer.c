/*******************************************************************************
 *  @file: EchoServer.c
 *  
 *  @brief: Simple TCP echo server.
*******************************************************************************/
#include "EchoServer.h"
#include <zephyr/logging/log.h>

#if CONFIG_ECHOSERVER_TRANSPORT_TCP
#include "TcpSocket.h"
#elif CONFIG_ECHOSERVER_TRANSPORT_UDP
#include "UdpSocket.h"
#else
#error "Unsupported echo server transport."
#endif


/** @brief Initialize the logging module. */
LOG_MODULE_REGISTER(EchoServer, CONFIG_ECHOSERVER_LOG_LEVEL);

/******************************************************************************
    echo_callback
*//**
    @brief Server callback. Echos data received back to the socket.

    @param[in] server  Reference to the underlying server object.
    @param[in] sock  The active socket.
    @param[in] data  Pointer to data buffer to send.
    @param[in] len  Length of data to send.
    @param[out] finished  0 = not finished, keep connection; 1 = finished.
******************************************************************************/
static void
echo_callback(void *server, int sock, uint8_t *data, uint16_t len, int *finished)
{
    /** @brief EchoServer type masquerades as a TcpServer. */
    EchoServer *echo = (EchoServer *)server;
    int num;
#if CONFIG_ECHOSERVER_TRANSPORT_TCP
    num = TcpSocket_write(sock, data, len);
#else
    UdpServer *udp_svr = &echo->svr.udp_svr;
    socklen_t addrlen = sizeof(udp_svr->src_addr);
    num = UdpSocket_writeto(sock, data, len,
        &udp_svr->src_addr, addrlen);
#endif
    if (num > 0)
    {
        echo->byte_count += num;
        LOG_DBG("Echo'd %d bytes (total: %u).", num, echo->byte_count);
    }
    else if (num < 0)
    {
        LOG_ERR("Error writing to socket: %d", num);
    }
    *finished = 1;
}

/******************************************************************************
    [docimport EchoServer_init]
*//**
    @brief Initializes the echo server.

    @param[in] echo  Pointer to EchoServer instance.
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
EchoServer_init(
    EchoServer *echo,
    uint16_t port,
    uint8_t *buf,
    uint32_t buf_len,
    uint16_t stack_size,
    char *name,
    uint8_t prio)
{
    int ret;
    echo->byte_count = 0;

#if CONFIG_ECHOSERVER_TRANSPORT_TCP
    LOG_INF("Echo server using TCP.");
    ret = TcpServer_init(
        &echo->svr.tcp_svr,
        port,
        buf,
        buf_len,
        stack_size,
        name,
        prio,
        echo_callback);
#else
    LOG_INF("Echo server using UDP.");
    ret = UdpServer_init(
        &echo->svr.udp_svr,
        port,
        buf,
        buf_len,
        stack_size,
        name,
        prio,
        echo_callback);
#endif

    return ret;
}

