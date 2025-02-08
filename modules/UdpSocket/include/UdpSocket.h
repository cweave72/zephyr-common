/*******************************************************************************
 *  @file: UdpSocket.h
 *   
 *  @brief: Header for UdpSocket.c
*******************************************************************************/
#ifndef UDPSOCKET_H
#define UDPSOCKET_H

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/posix/arpa/inet.h>

typedef struct UdpSocket
{
    uint16_t port;
    /** @brief Pointer to data buffer. */
    char *data;
    /** @brief Length of data buffer. */
    uint32_t data_len;
    /** @brief Internal reference. */
    struct sockaddr_in addr;
    int sock;

} UdpSocket;

/** @brief Convert sender's IP to string. */
#define UDPSOCKET_GET_ADDR(psa) \
    inet_ntoa(((struct sockaddr_in *)(psa))->sin_addr)

/******************************************************************************
    [docexport UdpSocket_read]
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
UdpSocket_read(int sock, uint8_t *buf, uint16_t buf_size, uint32_t timeout_ms);

/******************************************************************************
    [docexport UdpSocket_readfrom]
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
    uint32_t timeout_ms);

/******************************************************************************
    [docexport UdpSocket_writeto]
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
    socklen_t daddr_len);

/******************************************************************************
    [docexport UdpSocket_close]
*//**
    @brief Close a socket.

    @param[in] sock  The active socket descriptor to write to.
    @return Returns 0 on success, -1 on error.
******************************************************************************/
int
UdpSocket_close(int sock);

/******************************************************************************
    [docexport UdpSocket_bind]
*//**
    @brief Binds a socket to any local address. (server use).
    Called after UdpSocket_init.
    @param[in] udp  Pointer to UdpSocket object (initialized).
    @param[in] port  Port number to use.
******************************************************************************/
int
UdpSocket_bind(UdpSocket *udp, uint16_t port);

/******************************************************************************
    [docexport UdpSocket_connect]
*//**
    @brief Connect to a peer. (client use).
    Called after UdpSocket_init.
    @param[in] udp  Pointer to UdpSocket object (initialized).
    @param[in] ip  IP address of peer.
    @param[in] port  Port number to use.
******************************************************************************/
int
UdpSocket_connect(UdpSocket *udp, const char *ip, uint16_t port);

/******************************************************************************
    [docexport UdpSocket_init]
*//**
    @brief Initializes a udp socket.
    Follow this with calls to:
    UdpSocket_connect (client)
    UdpSocket_bind    (server)

    @param[in] udp  Pointer to UdpSocket object (uninitialized).
    @return Returns 0 on success, negative on error.
******************************************************************************/
int
UdpSocket_init(UdpSocket *udp);
#endif
