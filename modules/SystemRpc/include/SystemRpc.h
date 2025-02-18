/*******************************************************************************
 *  @file: SystemRpc.h
 *
 *  @brief: Header for SystemRpc.
*******************************************************************************/
#ifndef SYSTEMRPC_H
#define SYSTEMRPC_H

#include <stdint.h>
#include "ProtoRpc.h"

/******************************************************************************
    [docexport SystemRpc_resolver]
*//**
    @brief Resolver function for SystemRpc.
    @param[in] call_frame  Pointer to the unpacked call frame object.
    @param[out] which_msg  Output which_msg was requested.
******************************************************************************/
ProtoRpc_handler *
SystemRpc_resolver(void *call_frame, uint32_t *which_msg);
#endif
