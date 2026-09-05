/*******************************************************************************
 *  @file: rpc.c
 *
 *  @brief: ProtoRpc server setup over TCP with COBS framing.
 *
 *  Callsets are discovered by the host at runtime (callset_query in
 *  ProtoRpcHeader), so there is no callsets.yaml to keep in sync. Register
 *  additional callsets by adding to the table below.
*******************************************************************************/
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "TcpRpcServer.h"
#include "ProtoRpc.h"
#include "SystemRpc.h"
#include "SystemRpc.pb.h"
#include "RtosUtilsRpc.h"
#include "RtosUtilsRpc.pb.h"

#include "app.h"

LOG_MODULE_DECLARE(app, CONFIG_APP_LOG_LEVEL);

static ProtoRpc_Callset_Entry callsets[] = {
    PROTORPC_ADD_CALLSET(0, SystemRpc_resolver, system_Callset),
    PROTORPC_ADD_CALLSET(1, RtosUtilsRpc_resolver, rtosutils_Callset),
};

static TcpRpcServer tcp_rpc;
static ProtoRpc rpc;

#define RPCSERVER_STACK_SIZE    (4*1024)

static uint8_t *rpc_call_frame;
static uint8_t *rpc_reply_frame;
static uint8_t *rpc_callset_call_buf;
static uint8_t *rpc_callset_reply_buf;

int
app_rpc_init(void)
{
    uint32_t k;
    uint32_t max_callset_size = 0;
    int ret;

    for (k = 0; k < PROTORPC_ARRAY_LENGTH(callsets); k++)
    {
        if (callsets[k].size > max_callset_size)
        {
            max_callset_size = callsets[k].size;
        }
    }

    LOG_DBG("Max rpc callset size: %u", max_callset_size);

    rpc_call_frame = k_malloc(ProtoRpcHeader_size + max_callset_size);
    rpc_reply_frame = k_malloc(ProtoRpcHeader_size + max_callset_size);
    rpc_callset_call_buf = k_malloc(max_callset_size);
    rpc_callset_reply_buf = k_malloc(max_callset_size);

    if (!rpc_call_frame || !rpc_reply_frame ||
        !rpc_callset_call_buf || !rpc_callset_reply_buf)
    {
        LOG_ERR("Error allocating rpc buffers.");
        return -ENOMEM;
    }

    rpc.call_frame             = rpc_call_frame;
    rpc.reply_frame            = rpc_reply_frame;
    rpc.callsets               = callsets;
    rpc.callset_call_buf       = rpc_callset_call_buf;
    /* Note: max_callset_size, not sizeof() -- these are pointers. */
    rpc.callset_call_buf_size  = max_callset_size;
    rpc.callset_reply_buf      = rpc_callset_reply_buf;
    rpc.callset_reply_buf_size = max_callset_size;
    rpc.num_callsets           = PROTORPC_ARRAY_LENGTH(callsets);

    LOG_INF("Starting tcp rpc server on port %u.", CONFIG_TCPRPCSERVER_PORT);
    ret = TcpRpcServer_init(
        &tcp_rpc,
        &rpc,
        CONFIG_TCPRPCSERVER_PORT,
        RPCSERVER_STACK_SIZE,
        20);
    if (ret < 0)
    {
        LOG_ERR("Error initializing TcpRpcServer: %d", ret);
    }

    return ret;
}
