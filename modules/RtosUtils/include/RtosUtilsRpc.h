/*******************************************************************************
 *  @file: RtosUtilsRpc.h
 *
 *  @brief: Header for RtosUtilsRpc.
*******************************************************************************/
#ifndef RTOSUTILSRPC_H
#define RTOSUTILSRPC_H

#include <stdint.h>
#include "ProtoRpc.h"
#include "ProtoRpcHeader.pb.h"
#include "RtosUtilsRpc.pb.h"

extern CallsetInfo rtosutils_Callset_info;

/******************************************************************************
    [docexport RtosUtilsRpc_resolver]
*//**
    @brief Resolver function for RtosUtilsRpc.
    @param[in] call_frame  Pointer to the unpacked call frame object.
    @param[out] which_msg  Output which_msg was requested.
******************************************************************************/
ProtoRpc_handler *
RtosUtilsRpc_resolver(void *call_frame, uint32_t *which_msg);
#endif
