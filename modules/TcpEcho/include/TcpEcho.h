/*******************************************************************************
 *  @file: TcpEcho.h
 *   
 *  @brief: Header for TcpEcho.
*******************************************************************************/
#ifndef TCPECHO_H
#define TCPECHO_H

#include <stdint.h>
#include "TcpServer.h"

/** @brief TcpEcho object.
*/
typedef struct TcpEcho
{
    /** @brief Server instance. */
    TcpServer tcp_svr;
    /** @brief Total byte count. */
    uint32_t byte_count;
    
} TcpEcho;

/******************************************************************************
    [docexport TcpEcho_init]
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
    uint8_t prio);
#endif
