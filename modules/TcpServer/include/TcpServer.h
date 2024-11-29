/*******************************************************************************
 *  @file: TcpServer.h
 *   
 *  @brief: Header for TcpServer library.
*******************************************************************************/
#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <stdint.h>
#include "RtosUtils.h"
#include "TcpSocket.h"

/******************************************************************************
    TcpServer_cb
*//**
    @brief Server user callback.

    @param[in] server  Pointer to the server object.
    @param[in] sock  The active connected socket.
    @param[in] data  Pointer to buffer holding received data.
    @param[in] len  Length of received data.
    @param[out] finished  Status:
    0 --> not finished, keep connection active
    1 --> finished, close connection.
******************************************************************************/
typedef void
TcpServer_cb(
    void *server,
    int sock,
    uint8_t *data,
    uint16_t len,
    int *finished);

/** @brief Parameters for the Tcp server task.
*/
typedef struct TcpTask
{
    /** @brief Task stack size. */
    uint16_t stackSize;
    /** @brief Task name. */
    char name[16];
    /** @brief Task prio. */
    uint8_t prio;
    /** @brief Internal */
    /** @brief Task Stack pointer. */
    RTOS_TASK_STACK *stack;
    /** @brief Loop task handle. */
    RTOS_TASK handle;

} TcpTask;

typedef struct TcpServer
{
    /** @brief TcpSocket object. */
    TcpSocket tcpsock;

    /** @brief Pointer to data buffer. */
    uint8_t *data;
    /** @brief Length of data buffer. */
    uint16_t data_len;
    /** @brief User callback. */
    TcpServer_cb *cb;

    /** @brief Tcp task object. */
    TcpTask task;

} TcpServer;


/******************************************************************************
    [docexport TcpServer_init]
*//**
    @brief Initializes a TCP server.
    @param[in] server  Pointer to uninitialized TcpServer object.
    @param[in] port  Port number to use.
    @param[in] buf  Pointer to user-allocated buffer used for Rx. If NULL,
    buffer will be dynamically allocated.
    @param[in] buf_len  Length of the buffer.
    @param[in] task_stackSize  Size of the server task stack.
    @param[in] task_name  Name for the task.
    @param[in] task_prio  Task priority.
    @param[in] cb  User callback.
    @return Returns 0 on success, negative on error.
******************************************************************************/
int
TcpServer_init(
    TcpServer *server,
    uint16_t port,
    uint8_t *buf,
    uint32_t buf_len,
    uint16_t task_stackSize,
    char *task_name,
    uint8_t task_prio,
    TcpServer_cb *cb);
#endif
