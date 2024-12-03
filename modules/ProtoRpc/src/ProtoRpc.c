/*******************************************************************************
 *  @file: ProtoRpc.c
 *  
 *  @brief: Implements a Protobuf-based RPC server.
*******************************************************************************/
#include <string.h>
#include <zephyr/logging/log.h>
#include "PbGeneric.h"
#include "ProtoRpc.pb.h"
#include "ProtoRpc.h"

LOG_MODULE_REGISTER(ProtoRpc, CONFIG_PROTORPC_LOG_LEVEL);

/******************************************************************************
    callset_lookup
*//**
    @brief Performs a callset lookup based on the which_callset tag.
******************************************************************************/
static ProtoRpc_resolver *
callset_lookup(
    uint32_t which_callset,
    ProtoRpc_Callset_Entry *callsets,
    uint32_t num_callsets,
    const void **fields)
{
    ProtoRpc_Callset_Entry *entry;
    uint32_t i;

    for (i = 0; i < num_callsets; i++)
    {
        entry = callsets + i;
        if (entry->id == which_callset)
        {
            *fields = entry->fields;
            return entry->resolver;
        }
    }

    return NULL;
}

/******************************************************************************
    [docimport ProtoRpc_exec]
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
    uint32_t *reply_encoded_size)
{
    bool status;
    ProtoRpc_resolver *resolver;
    ProtoRpcHeader header;
    ProtoRpcHeader reply_header;
    ProtoRpc_handler *handler;
    const void *callset_fields;
    uint32_t which_msg;
    uint8_t *callset_call_buf = rpc->callset_call_buf;
    uint8_t *callset_reply_buf = rpc->callset_reply_buf;

    *reply_encoded_size = 0;
    memset(&reply_header, 0, sizeof(ProtoRpcHeader));

    pb_istream_t istream = pb_istream_from_buffer(rcvd_buf, rcvd_buf_size);
    pb_ostream_t ostream = pb_ostream_from_buffer(reply_buf, reply_buf_max_size);

    //LOG_HEXDUMP_DBG(rcvd_buf, rcvd_buf_size, "Received frame.");

    /* Unpack the RPC header received buffer into the header struct. */
    status = Pb_unpack_delimited(&istream, &header, ProtoRpcHeader_fields);
    if (!status)
    {
        LOG_HEXDUMP_ERR(rcvd_buf, rcvd_buf_size, "Pb_unpack failed.");
        return;
    }

    LOG_DBG("header: seqn = %u; no_reply = %u; which_callset = %u",
        (unsigned int)header.seqn,
        (unsigned int)header.no_reply,
        (unsigned int)header.which_callset);

    /** @brief Get the callset resolver function. */
    resolver = callset_lookup(header.which_callset,
                              rpc->callsets,
                              rpc->num_callsets,
                              &callset_fields);
    if (!resolver)
    {
        LOG_ERR("Bad resolver lookup (which_callset=%u).",
            (unsigned int)header.which_callset);
        reply_header.seqn = header.seqn;
        reply_header.which_callset = header.which_callset;
        reply_header.status = StatusEnum_RPC_BAD_RESOLVER_LOOKUP;
        *reply_encoded_size = Pb_pack_delimited(&ostream,
                                                &reply_header,
                                                ProtoRpcHeader_fields);
        return;
    }

    /** @brief Unpack the callset. */
    status = Pb_unpack_delimited(&istream, callset_call_buf, callset_fields);
    if (!status)
    {
        LOG_ERR("Bad callset unpack (which_callset=%u).",
            (unsigned int)header.which_callset);
        reply_header.seqn = header.seqn;
        reply_header.which_callset = header.which_callset;
        reply_header.status = StatusEnum_RPC_BAD_CALLSET_UNPACK;
        *reply_encoded_size = Pb_pack_delimited(&ostream,
                                                &reply_header,
                                                ProtoRpcHeader_fields);
        return;
    }

    /** @brief Get the callset handler function. */
    handler = resolver(callset_call_buf, &which_msg);
    if (!handler)
    {
        LOG_ERR("Bad handler lookup (which_callset=%u).",
            (unsigned int)header.which_callset);
        reply_header.seqn = header.seqn;
        reply_header.which_callset = header.which_callset;
        reply_header.status = StatusEnum_RPC_BAD_HANDLER_LOOKUP;
        *reply_encoded_size = Pb_pack_delimited(&ostream,
                                                &reply_header,
                                                ProtoRpcHeader_fields);
        return;
    }

    /** @brief Call the handler. */
    LOG_DBG("Calling handler for which_msg=%u", which_msg);
    handler(callset_call_buf, callset_reply_buf, &reply_header.status);

    if (header.no_reply)
    {
        return;
    }

    /** @brief Pack Reply frame header, then callset reply */
    reply_header.seqn = header.seqn;
    reply_header.which_callset = header.which_callset;
    Pb_pack_delimited(&ostream, &reply_header, ProtoRpcHeader_fields);
    Pb_pack_delimited(&ostream, callset_reply_buf, callset_fields);

    //LOG_DBG("ostream bytes (after callset): %u", ostream.bytes_written);
    //LOG_HEXDUMP_DBG(reply_buf, ostream.bytes_written, "Reply frame");

    *reply_encoded_size = ostream.bytes_written;
}
