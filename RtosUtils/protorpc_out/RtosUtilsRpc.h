/*******************************************************************************
 *  @file: RtosUtilsRpc.h
 *
 *  @brief: Header for RtosUtilsRpc.
*******************************************************************************/
#ifndef RTOSUTILSRPC_H
#define RTOSUTILSRPC_H

#include <stdint.h>

/******************************************************************************
    [docexport RtosUtilsRpc_resolver]
*//**
    @brief Resolver function for RtosUtilsRpc.
******************************************************************************/
ProtoRpc_handler *
RtosUtilsRpc_resolver(void *call_frame, uint32_t offset);
#endif