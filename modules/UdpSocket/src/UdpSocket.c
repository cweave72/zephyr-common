#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include "CheckCond.h"
#include "UdpSocket.h"

/** @brief Initialize the logging module. */
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(UdpSocket, CONFIG_UDPSOCKET_LOG_LEVEL);

#define min(a, b)   (((a)<(b)) ? (a) : (b))

/******************************************************************************
    wait_recv
*//**
    @brief Recv with timeout.
    @param[in] sock  The active socket descriptor to read from.
    @param[in] timeout_ms  Poll timeout in ms. Set to 0 for no timeout.
    @return Returns 0 when data is ready.
    @return -ETIMEDOUT on timeout.
    @return other negative code.
******************************************************************************/
static int
poll_recv(int sock, uint32_t timeout_ms)
{
    struct zsock_pollfd fds[1];
    int ready;

    fds[0].fd = sock;
    fds[0].events = ZSOCK_POLLIN;
    
    /* poll():
         0 : timeout waiting for fd ready.
        <0 : errno
        >0 : number of elements in pollfds whose revents are nonzero
    */
    if ((ready = zsock_poll(fds, 1, (int)timeout_ms)) < 0)
    {
        LOG_ERR("wait_poll_input error: %d", errno);
        return ready;
    }

    if (ready == 0)
    {
        return -ETIMEDOUT;
    }

    /* There is only one fd to check. */
    if (fds[0].revents & POLLIN)
    {
        /* Data received, let mqtt lib handle it. */
        LOG_DBG("UDP data ready.");
        return 0;
    }
    else if (fds[0].revents & POLLHUP)
    {
        /* Hangup detected. */
        LOG_WRN("UDP POLLHUP.");
        return -ECONNRESET;
    }
    else if (fds[0].revents & POLLERR)
    {
        /* Error . */
        LOG_WRN("UDP POLLERR.");
        return -EIO;
    }

    /* Should not get here. */
    return 0;
}

/******************************************************************************
    [docimport UdpSocket_read]
*//**
    @brief Reads data from socket.  Reads from peer address specified during
    connect. Typcially used when operating as a client.

    @param[in] sock  The active socket descriptor to read from.
    @param[in] buf  Pointer to read buffer.
    @param[in] buf_size  Max size of the buffer.
    @param[in] timeout_ms  Poll timeout in ms. Set to 0 for no timeout.
    @return Returns the number of bytes read or negative error code.
    @return Returns -ETIMEDOUT on timeout.
******************************************************************************/
int
UdpSocket_read(int sock, uint8_t *buf, uint16_t buf_size, uint32_t timeout_ms)
{
    int ret;

    if (timeout_ms > 0)
    {
        if ((ret = poll_recv(sock, timeout_ms)) < 0)
        {
            return ret;
        }
    }

    /** @brief Read bytes at the port. */
    int len = zsock_recv(sock, buf, buf_size, 0);

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
        LOG_HEXDUMP_DBG(buf, min(len, 64), "Bytes recv'd");
    }

    return len;
}

/******************************************************************************
    [docimport UdpSocket_readfrom]
*//**
    @brief Reads data from socket, provides sender's address.
    Typcially used when operating as a server.

    @param[in] sock  The active socket descriptor to read from.
    @param[in] buf  Pointer to read buffer.
    @param[in] buf_size  Max size of the buffer.
    @param[out] saddr  Pointer to sockaddr struct for source to be filled in.
    @param[out] saddr_len  Length of the sockaddr struct.
    @param[in] timeout_ms  Poll timeout in ms. Set to 0 for no timeout.
    @return Returns the number of bytes read or negative error code.
    @return Returns -ETIMEDOUT on timeout.
******************************************************************************/
int
UdpSocket_readfrom(
    int sock,
    uint8_t *buf,
    uint16_t buf_size,
    struct sockaddr_in *saddr,
    socklen_t *saddr_len,
    uint32_t timeout_ms)
{
    int ret;

    if (timeout_ms > 0)
    {
        if ((ret = poll_recv(sock, timeout_ms)) < 0)
        {
            return ret;
        }
    }

    /** @brief Read bytes at the port. */
    int len = zsock_recvfrom(
        sock, buf, buf_size, 0, (struct sockaddr *)saddr, saddr_len);

    if (len < 0)
    {
        LOG_ERR("Error occured during socket recvfrom: errno %d", errno);
    }
    else if (len == 0)
    {
        LOG_DBG("Peer connection closed.");
    }
    else
    {
        LOG_DBG("Received %u bytes from %s.", len, UDPSOCKET_GET_ADDR(saddr));
        LOG_HEXDUMP_DBG(buf, min(len, 64), "Bytes recv'd");
    }

    return len;
}

/******************************************************************************
    [docimport UdpSocket_writeto]
*//**
    @brief Sends data to socket.

    @param[in] sock  The active socket descriptor to write to.
    @param[in] data  Pointer to data buffer.
    @param[in] data_size  Size of the buffer to write.
    @param[in] daddr  Pointer to destination addr.
    @param[in] daddr_len  Length of dest addr struct.
    @return Returns the number of bytes written or -1 on error.
******************************************************************************/
int
UdpSocket_writeto(
    int sock,
    uint8_t *data,
    uint16_t data_size,
    const struct sockaddr_in *daddr,
    socklen_t daddr_len)
{
    int num = zsock_sendto(
        sock, data, data_size, 0, (struct sockaddr *)daddr, daddr_len);
    if (num < 0)
    {
        LOG_ERR("Error socket sendto: errno %d", errno);
        return num;
    }

    return num;
}

/******************************************************************************
    [docimport UdpSocket_close]
*//**
    @brief Close a socket.

    @param[in] sock  The active socket descriptor to write to.
    @return Returns 0 on success, -1 on error.
******************************************************************************/
int
UdpSocket_close(int sock)
{
    int ret = zsock_close(sock);
    if (ret < 0)
    {
        LOG_ERR("Error on socket close: errno %d", errno);
    }
    return ret;
}

/******************************************************************************
    [docimport UdpSocket_bind]
*//**
    @brief Binds a socket to any local address. (server use).
    Called after UdpSocket_init.
    @param[in] udp  Pointer to UdpSocket object (initialized).
    @param[in] port  Port number to use.
******************************************************************************/
int
UdpSocket_bind(UdpSocket *udp, uint16_t port)
{
    struct sockaddr_in *server_addr = &udp->addr;

    udp->port = port;

    server_addr->sin_addr.s_addr = (uint32_t)INADDR_ANY;  /* 0.0.0.0 */
    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(port);   /* Note an endian swap occurs here */
    
    int err = zsock_bind(
        udp->sock,
        (struct sockaddr *)server_addr,
        sizeof(struct sockaddr_in));
    if (err < 0)
    {
        LOG_ERR("Error binding to port %u (%d)", port, err);
        return err;
    }
    LOG_INF("Socket bound to port %u", port);
    return 0;
}

/******************************************************************************
    [docimport UdpSocket_connect]
*//**
    @brief Connect to a peer. (client use).
    Called after UdpSocket_init.
    @param[in] udp  Pointer to UdpSocket object (initialized).
    @param[in] ip  IP address of peer.
    @param[in] port  Port number to use.
******************************************************************************/
int
UdpSocket_connect(UdpSocket *udp, const char *ip, uint16_t port)
{
    struct sockaddr_in *peer_addr = &udp->addr;

    udp->port = port;

    inet_pton(AF_INET, ip, &peer_addr->sin_addr);
    peer_addr->sin_family = AF_INET;
    peer_addr->sin_port = htons(port);   /* Note an endian swap occurs here */
    
    int err = zsock_connect(
        udp->sock,
        (struct sockaddr *)peer_addr,
        sizeof(struct sockaddr_in));
    if (err < 0)
    {
        LOG_ERR("Error connecting to %s:%u (%d)", ip, port, err);
        return err;
    }
    LOG_INF("Socket connected to %s:%u", ip, port);
    return 0;
}

/******************************************************************************
    [docimport UdpSocket_init]
*//**
    @brief Initializes a udp socket.
    Follow this with calls to:
    UdpSocket_connect (client)
    UdpSocket_bind    (server)

    @param[in] udp  Pointer to UdpSocket object (uninitialized).
    @return Returns 0 on success, negative on error.
******************************************************************************/
int
UdpSocket_init(UdpSocket *udp)
{
    int sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0)
    {
        LOG_ERR("Error creating UDP socket: %d", sock);
        return sock;
    }
    LOG_INF("UDP Socket created successfully.");
    udp->sock = sock;
    
    return 0;
}
