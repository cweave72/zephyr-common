/*******************************************************************************
 *  @file: TcpRpcServer.h
 *   
 *  @brief: Header for TcpRpcServer.c
*******************************************************************************/
#ifndef TCPRPCSERVER_H
#define TCPRPCSERVER_H

#include <stdint.h>
#include "TcpServer.h"
#include "ProtoRpc.h"
#include "Cobs_frame.h"

/** @brief TcpRpcServer object.
*/
typedef struct TcpRpcServer
{
    /** @brief Server instance. */
    TcpServer tcp;
    /** @brief Pointer to the ProtoRpc instance. */
    ProtoRpc *rpc;
    /** @brief Stream de-framer. */
    Cobs_Deframer deframer;
    
} TcpRpcServer;


/******************************************************************************
    [docexport TcpRpcServer_init]
*//**
    @brief Initializes the TCP-based RPC server.
    @param[in] server  Pointer to uninitialized TcpRpcServer instance.
    @param[in] rpc  Pointer to *initialized* ProtoRpc instance.
    @param[in] port  Port number to use.
    @param[in] stack_size  Size of the server task stack.
    @param[in] prio  Server task priority.
    @return Returns 0 on success, negative on error.
******************************************************************************/
int
TcpRpcServer_init(
    TcpRpcServer *server,
    ProtoRpc *rpc,
    uint16_t port,
    uint16_t stack_size,
    uint8_t prio);
#endif
