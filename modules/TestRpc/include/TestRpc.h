/*******************************************************************************
 *  @file: TestRpc.h
 *   
 *  @brief: Header for TestRpc.
*******************************************************************************/
#ifndef TESTRPC_H
#define TESTRPC_H

#include <stdint.h>
#include "ProtoRpc.h"

/******************************************************************************
    [docexport TestRpc_resolver]
*//**
    @brief Resolver function for TestRpc.
    @param[in] call_frame  Pointer to the unpacked call frame object.
    @param[out] which_msg  Output which_msg was requested.
******************************************************************************/
ProtoRpc_handler *
TestRpc_resolver(void *call_frame, uint32_t *which_msg);
#endif
