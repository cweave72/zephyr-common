/*******************************************************************************
 *  @file: TcpSocket.h
 *   
 *  @brief: Header for TcpSocket.c
*******************************************************************************/
#ifndef TCPSOCKET_H
#define TCPSOCKET_H

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/posix/arpa/inet.h>

typedef struct TcpSocket
{
    uint16_t port;
    /** @brief Pointer to data buffer. */
    char *data;
    /** @brief Length of data buffer. */
    uint32_t data_len;
    /** @brief Internal reference. */
    struct sockaddr_in dest_addr;
    struct sockaddr_storage source_addr;
    int sock;

} TcpSocket;

/** @brief Convert sender's IP to string. */
#define TCPSOCKET_GET_ADDR(sa) \
    inet_ntoa(((struct sockaddr_in *)(&(sa)))->sin_addr)


/******************************************************************************
    [docexport TcpSocket_read]
*//**
    @brief Reads data from socket.

    @param[in] sock  The active socket descriptor to read from.
    @param[in] buf  Pointer to read buffer.
    @param[in] buf_size  Max size of the buffer.
    @return Returns the number of bytes read or -1 on error.
******************************************************************************/
int
TcpSocket_read(int sock, uint8_t *buf, uint16_t buf_size);

/******************************************************************************
    [docexport TcpSocket_write]
*//**
    @brief Sends data to socket.

    @param[in] sock  The active socket descriptor to write to.
    @param[in] data  Pointer to data buffer.
    @param[in] data_size  Size of the buffer to write.
    @return Returns the number of bytes written or -1 on error.
******************************************************************************/
int
TcpSocket_write(int sock, uint8_t *data, uint16_t data_size);

/******************************************************************************
    [docexport TcpSocket_close]
*//**
    @brief Close a socket.

    @param[in] sock  The active socket descriptor to write to.
    @return Returns 0 on success, -1 on error.
******************************************************************************/
int
TcpSocket_close(int sock);

/******************************************************************************
    [docexport TcpSocket_shutdown]
*//**
    @brief Shuts down connection on socket.

    @param[in] sock  The active socket descriptor to write to.
    @param[in] type  Shutdown type: 0=recv; 1=transmit; 2=both
    @return Returns 0 on success, -1 on error.
******************************************************************************/
int
TcpSocket_shutdown(int sock, int type);

/******************************************************************************
    [docexport TcpSocket_listen]
*//**
    @brief Start listening on socket port.

    @param[in] tcp  Pointer to TcpSocket object.
    @param[in] queue_num  Max number of connections to queue up. Set to 1 if
    unsure.
    @return Returns 0 on success, -1 on error.
******************************************************************************/
int
TcpSocket_listen(TcpSocket *tcp, int queue_num);

/******************************************************************************
    [docexport TcpSocket_accept]
*//**
    @brief Accepts incoming connections on the socket.

    @param[in] tcp  Pointer to TcpSocket object.
    @param[in] keepIdle  Keep alive idle time, sec.
    @param[in] keepInterval  Keep alive interval time, sec.
    @param[in] keepCount  Keep alive count.
    @return Returns the accepted socket descriptor on success, -1 on error.
******************************************************************************/
int
TcpSocket_accept(TcpSocket *tcp, int keepIdle, int keepInterval, int keepCount);

/******************************************************************************
    [docexport TcpSocket_bind]
*//**
    @brief Binds a socket to any local address. (server use).
    Called after TcpSocket_init.
    @param[in] tcp  Pointer to TcpSocket object (initialized).
    @param[in] port  Port number to use.
******************************************************************************/
int
TcpSocket_bind(TcpSocket *tcp, uint16_t port);

/******************************************************************************
    [docexport TcpSocket_connect]
*//**
    @brief Connect to a peer. (client use).
    Called after TcpSocket_init.
    @param[in] tcp  Pointer to TcpSocket object (initialized).
    @param[in] ip  IP address of peer.
    @param[in] port  Port number to use.
******************************************************************************/
int
TcpSocket_connect(TcpSocket *tcp, const char *ip, uint16_t port);

/******************************************************************************
    [docexport TcpSocket_init]
*//**
    @brief Initializes a TCP socket.
    Follow this with calls to:
    TcpSocket_connect (client)
    TcpSocket_bind    (server)

    @param[in] tcp  Pointer to TcpSocket object (uninitialized).
    @return Returns 0 on success, negative on error.
******************************************************************************/
int
TcpSocket_init(TcpSocket *tcp);
#endif
