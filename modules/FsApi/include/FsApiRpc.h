/*******************************************************************************
 *  @file: FsApiRpc.h
 *
 *  @brief: Header for FsApiRpc, the remote access callset for FsApi.
*******************************************************************************/
#ifndef FSAPIRPC_H
#define FSAPIRPC_H

#include <stdint.h>
#include "ProtoRpc.h"
#include "ProtoRpcHeader.pb.h"
#include "FsApiRpc.pb.h"

extern CallsetInfo fsapi_Callset_info;

/******************************************************************************
    [docexport FsApiRpc_resolver]
*//**
    @brief Resolver function for FsApiRpc.
    @param[in] call_frame  Pointer to the unpacked call frame object.
    @param[out] which_msg  Output which_msg was requested.
******************************************************************************/
ProtoRpc_handler *
FsApiRpc_resolver(void *call_frame, uint32_t *which_msg);

#endif /* FSAPIRPC_H */
