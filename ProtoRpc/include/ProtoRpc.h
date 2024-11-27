/*******************************************************************************
 *  @file: ProtoRpc.h
 *   
 *  @brief: Header for ProtoRpc.
*******************************************************************************/
#ifndef PROTORPC_H
#define PROTORPC_H
#include <stddef.h>
#include <stdint.h>
#include "ProtoRpc.pb.h"

/** @brief Max size of a ProtoRpc message */
#define PROTORPC_MSG_MAX_SIZE    4096

typedef void ProtoRpc_handler(void *call_frame, void *reply_frame, StatusEnum *status);
typedef ProtoRpc_handler * ProtoRpc_resolver(void *call_frame, uint32_t offset);

typedef struct ProtoRpc_Resolver_Entry
{
    /** @brief The callset tag in the RpcFrame. */
    uint32_t tag;
    /** @brief Pointer to the resolver function. */
    ProtoRpc_resolver *resolver;

} ProtoRpc_Resolver_Entry;

typedef ProtoRpc_Resolver_Entry * ProtoRpc_resolvers;

#define PROTORPC_ADD_CALLSET(callset_tag, callset_resolver) \
{ .tag = (callset_tag), .resolver = (callset_resolver) }

typedef struct ProtoRpc
{
    uint8_t *call_frame;
    uint8_t *reply_frame;
    size_t header_offset;
    size_t which_callset_offset;
    size_t callset_offset;
    const void *frame_fields;
    ProtoRpc_resolvers resolvers;
    int num_resolvers;
} ProtoRpc;

typedef struct ProtoRpc_Handler_Entry
{
    /** @brief The handler tag in the callset. */
    uint32_t tag;
    /** @brief Pointer to the handler function. */
    ProtoRpc_handler *handler;

} ProtoRpc_Handler_Entry;

/** @brief Helper macro for initializing the ProtoRpc object. */
#define ProtoRpc_init(frame, call_frame_buf, reply_frame_buf, resolvers)  \
{   .header_offset = offsetof(frame, header),                     \
    .which_callset_offset = offsetof(frame, which_callset),       \
    .callset_offset = offsetof(frame, callset),                   \
    .frame_fields = frame ## _fields,                             \
    .resolvers = resolvers,                                       \
    .call_frame = (call_frame_buf),                               \
    .reply_frame = (reply_frame_buf),                             \
    .num_resolvers = PROTORPC_ARRAY_LENGTH(resolvers)             \
}

typedef ProtoRpc_Handler_Entry *ProtoRpc_handlers;

#define PROTORPC_ADD_HANDLER(handler_tag, handler_func)\
{ .tag = (handler_tag), .handler = (handler_func) }

#define PROTORPC_ARRAY_LENGTH(array)\
    (sizeof((array)) / sizeof((array)[0]))

/******************************************************************************
    [docexport ProtoRpc_server]
*//**
    @brief Decoded received ProtoRpc frame, executes the RPC, provides the reply.
    @param[in] rpc  Pointer to initialized ProtoRpc instance.
    @param[in] rcvd_buf  Pointer to the received buffer.
    @param[in] rcvd_buf_size  Number of bytes in the recieved message.
    @param[in] reply_buf  Pointer to the message reply buffer.
    @param[in] reply_buf_max_size  Max size of the reply buffer.
    @param[out] reply_encoded_size  Returned size of the packed reply message.
******************************************************************************/
void
ProtoRpc_server(
    ProtoRpc *rpc,
    uint8_t *rcvd_buf,
    uint32_t rcvd_buf_size,
    uint8_t *reply_buf,
    uint32_t reply_buf_max_size,
    uint32_t *reply_encoded_size);
#endif
