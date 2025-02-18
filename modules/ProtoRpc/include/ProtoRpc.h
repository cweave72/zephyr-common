/*******************************************************************************
 *  @file: ProtoRpc.h
 *   
 *  @brief: Header for ProtoRpc.
*******************************************************************************/
#ifndef PROTORPC_H
#define PROTORPC_H
#include <stddef.h>
#include <stdint.h>
#include "ProtoRpcHeader.pb.h"

/** @brief Max size of a ProtoRpc message */
#define PROTORPC_MSG_MAX_SIZE    4096

typedef void ProtoRpc_handler(void *call_frame, void *reply_frame, StatusEnum *status);
typedef ProtoRpc_handler * ProtoRpc_resolver(void *call_frame, uint32_t *which_msg);

typedef struct ProtoRpc_Callset_Entry
{
    /** @brief The callset id. */
    uint32_t id;
    /** @brief Pointer to the resolver function. */
    ProtoRpc_resolver *resolver;
    /** @brief Callset fields */
    const void *fields;
    /** @brief Callset size */
    uint32_t size;

} ProtoRpc_Callset_Entry;

typedef ProtoRpc_Callset_Entry * ProtoRpc_callsets;

#define PROTORPC_ADD_CALLSET(callset_id, callset_resolver, callset_fields, callset_size) \
{ .id       = (callset_id),                  \
  .resolver = (callset_resolver),            \
  .fields   = (callset_fields),              \
  .size     = (callset_size)                 \
}

typedef struct ProtoRpc
{
    uint8_t *call_frame;
    uint8_t *reply_frame;
    uint8_t *callset_call_buf;
    uint32_t callset_call_buf_size;
    uint8_t *callset_reply_buf;
    uint32_t callset_reply_buf_size;
    ProtoRpc_callsets callsets;
    int num_callsets;
} ProtoRpc;

typedef struct ProtoRpc_Handler_Entry
{
    /** @brief The handler tag in the callset. */
    uint32_t tag;
    /** @brief Pointer to the handler function. */
    ProtoRpc_handler *handler;

} ProtoRpc_Handler_Entry;

typedef ProtoRpc_Handler_Entry *ProtoRpc_handlers;

#define PROTORPC_ADD_HANDLER(handler_tag, handler_func)\
{ .tag = (handler_tag), .handler = (handler_func) }

#define PROTORPC_ARRAY_LENGTH(array)\
    (sizeof((array)) / sizeof((array)[0]))

/******************************************************************************
    [docexport ProtoRpc_exec]
*//**
    @brief Decodes received ProtoRpc frame, executes the RPC, provides the reply.
    @param[in] rpc  Pointer to initialized ProtoRpc instance.
    @param[in] rcvd_buf  Pointer to the received buffer.
    @param[in] rcvd_buf_size  Number of bytes in the recieved message.
    @param[in] reply_buf  Pointer to the message reply buffer.
    @param[in] reply_buf_max_size  Max size of the reply buffer.
    @param[out] reply_encoded_size  Returned size of the packed reply message.
******************************************************************************/
void
ProtoRpc_exec(
    ProtoRpc *rpc,
    uint8_t *rcvd_buf,
    uint32_t rcvd_buf_size,
    uint8_t *reply_buf,
    uint32_t reply_buf_max_size,
    uint32_t *reply_encoded_size);
#endif
