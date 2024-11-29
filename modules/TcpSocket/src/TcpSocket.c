#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include "CheckCond.h"
#include "TcpSocket.h"

/** @brief Initialize the logging module. */
LOG_MODULE_REGISTER(TcpSocket, CONFIG_TCPSOCKET_LOG_LEVEL);

/******************************************************************************
    [docimport TcpSocket_read]
*//**
    @brief Reads data from socket.

    @param[in] sock  The active socket descriptor to read from.
    @param[in] buf  Pointer to read buffer.
    @param[in] buf_size  Max size of the buffer.
    @return Returns the number of bytes read or -1 on error.
******************************************************************************/
int
TcpSocket_read(int sock, uint8_t *buf, uint16_t buf_size)
{
    int len;

    /** @brief Read bytes at the port. */
    len = zsock_recv(sock, buf, buf_size, 0);

    if (len < 0)
    {
        LOG_ERR("Error occured during socket recv: errno %d", errno);
    }
    else if (len == 0)
    {
        LOG_DBG("Peer connection closed.");
    }
    else
    {
        LOG_DBG("Received %u bytes.", len);
        LOG_HEXDUMP_DBG(buf, len, "Bytes recv'd");
    }
    return len;
}

/******************************************************************************
    [docimport TcpSocket_write]
*//**
    @brief Sends data to socket.

    @param[in] sock  The active socket descriptor to write to.
    @param[in] data  Pointer to data buffer.
    @param[in] data_size  Size of the buffer to write.
    @return Returns the number of bytes written or -1 on error.
******************************************************************************/
int
TcpSocket_write(int sock, uint8_t *data, uint16_t data_size)
{
    int num_written = 0;
    uint16_t to_write = data_size;

    while (num_written < data_size)
    {
        int num = zsock_send(sock, data + num_written, to_write, 0);
        if (num < 0)
        {
            LOG_ERR("Error writing to socket after writing %u bytes: "
                "errno %d", num_written, errno);
            return num;
        }
        to_write -= num;
        num_written += num;
    }
    return num_written;
}

/******************************************************************************
    [docimport TcpSocket_close]
*//**
    @brief Close a socket.

    @param[in] sock  The active socket descriptor to write to.
    @return Returns 0 on success, -1 on error.
******************************************************************************/
int
TcpSocket_close(int sock)
{
    int ret = zsock_close(sock);
    if (ret < 0)
    {
        LOG_ERR("Error on socket close: errno %d", errno);
    }
    return ret;
}

/******************************************************************************
    [docimport TcpSocket_shutdown]
*//**
    @brief Shuts down connection on socket.

    @param[in] sock  The active socket descriptor to write to.
    @param[in] type  Shutdown type: 0=recv; 1=transmit; 2=both
    @return Returns 0 on success, -1 on error.
******************************************************************************/
int
TcpSocket_shutdown(int sock, int type)
{
    /* shutdown() has no effect in zephyr */
    (void)sock;
    (void)type;
    return 0;
}

/******************************************************************************
    [docimport TcpSocket_listen]
*//**
    @brief Start listening on socket port.

    @param[in] tcp  Pointer to TcpSocket object.
    @param[in] queue_num  Max number of connections to queue up. Set to 1 if
    unsure.
    @return Returns 0 on success, -1 on error.
******************************************************************************/
int
TcpSocket_listen(TcpSocket *tcp, int queue_num)
{
    int ret = zsock_listen(tcp->sock, queue_num);
    if (ret != 0)
    {
        LOG_ERR("Error on socket listen: errno %d", errno);
    }
    return ret;
}

/******************************************************************************
    [docimport TcpSocket_accept]
*//**
    @brief Accepts incoming connections on the socket.

    @param[in] tcp  Pointer to TcpSocket object.
    @param[in] keepIdle  Keep alive idle time, sec.
    @param[in] keepInterval  Keep alive interval time, sec.
    @param[in] keepCount  Keep alive count.
    @return Returns the accepted socket descriptor on success, -1 on error.
******************************************************************************/
int
TcpSocket_accept(TcpSocket *tcp, int keepIdle, int keepInterval, int keepCount)
{
    int sock;
    int keepAlive = 1;
    char *addr_str;
    struct sockaddr_storage source_addr;
    socklen_t addr_len = sizeof(source_addr);

    /** @brief Accept incoming connections. */
    sock = zsock_accept(tcp->sock, (struct sockaddr *)&source_addr, &addr_len);
    if (sock < 0)
    {
        LOG_ERR("Error on socket accept: errno %d", errno);
        return -1;
    }

    /** @brief Set incoming socket keep-alive options. */
    zsock_setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
    zsock_setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
    zsock_setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
    zsock_setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));

    addr_str = TCPSOCKET_GET_ADDR(source_addr);
    LOG_DBG("TCP connection accepted from %s", addr_str);

    return sock;
}

/******************************************************************************
    [docimport TcpSocket_init]
*//**
    @brief Initializes a TCP socket.

    @param[in] tcp  Pointer to TcpSocket object (uninitialized).
    @param[in] port  Port number to use.
    @return Returns 0 on success, negative on error.
******************************************************************************/
int
TcpSocket_init(TcpSocket *tcp, uint16_t port)
{
    struct sockaddr_in *dest_ip4 = &tcp->dest_addr;
    int sock, err;

    tcp->port = port;

    dest_ip4->sin_addr.s_addr = (uint32_t)INADDR_ANY;  /* 0.0.0.0 */
    dest_ip4->sin_family = AF_INET;
    dest_ip4->sin_port = htons(port);   /* Note an endian swap occurs here */

    sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    CHECK_COND_RETURN_MSG(sock < 0, sock, "Error creating socket.");
    LOG_INF("Socket created successfully.");
    tcp->sock = sock;
    
    err = zsock_bind(
        sock,
        (struct sockaddr *)dest_ip4,
        sizeof(struct sockaddr_in));
    CHECK_COND_RETURN_MSG(err < 0, err, "Error binding socket.");
    LOG_INF("Socket bound to port %d", port);

    return 0;
}
