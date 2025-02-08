/*******************************************************************************
 *  @file: UdpServer.c
 *  
 *  @brief: Library implementing a udp server.
*******************************************************************************/
#include <string.h>
#include <zephyr/logging/log.h>

#include "CheckCond.h"
#include "UdpServer.h"

/** @brief Initialize the logging module. */
LOG_MODULE_REGISTER(UdpServer, CONFIG_UDPSERVER_LOG_LEVEL);

/******************************************************************************
    udp_server_task
*//**
    @brief Main task loop for Udp server.
******************************************************************************/
static void
udp_server_task(void *p, void *arg1, void *arg2)
{
    UdpServer *server = (UdpServer *)p;
    UdpSocket *udp = &server->udpsock;
    UdpTask *task = &server->task;

    (void)arg1;
    (void)arg2;

    LOG_INF("UDP socket listening on port %u: %s",
        (unsigned int)udp->port, task->name);

    while (1)
    {
        struct sockaddr_in *src_addr = &server->src_addr;
        socklen_t src_addr_len = sizeof(server->src_addr);
        int num_read;
        int dummy;

        num_read = UdpSocket_readfrom(udp->sock, server->data, server->data_len,
            src_addr, &src_addr_len, 1000);
        if (num_read < 0)
        {
            if (num_read == -ETIMEDOUT)
            {
                continue;
            }
            LOG_ERR("udp socket read error: %d", num_read);
            break;
        }

        /** @brief Call callback to allow rx and tx on socket. */
        server->cb(
            (void *)server,
            udp->sock,
            server->data,
            (uint16_t)num_read,
            &dummy);
    }

    LOG_WRN("Closing udp socket.");
    UdpSocket_close(udp->sock);
}

/******************************************************************************
    [docimport UdpServer_init]
*//**
    @brief Initializes a udp server.
    @param[in] server  Pointer to uninitialized UdpServer object.
    @param[in] port  Port number to use.
    @param[in] buf  Pointer to user-allocated buffer used for Rx. If NULL,
    buffer will be dynamically allocated.
    @param[in] buf_len  Length of the buffer.
    @param[in] task_stackSize  Size of the server task stack.
    @param[in] task_name  Name for the task.
    @param[in] task_prio  Task priority.
    @param[in] cb  User callback.
    @return Returns 0 on success, negative on error.
******************************************************************************/
int
UdpServer_init(
    UdpServer *server,
    uint16_t port,
    uint8_t *buf,
    uint32_t buf_len,
    uint16_t task_stackSize,
    char *task_name,
    uint8_t task_prio,
    UdpServer_cb *cb)
{
    UdpSocket *udp = &server->udpsock;
    UdpTask *task = &server->task;
    int rc;

    CHECK_COND_RETURN_MSG(!cb, -1, "A callback must be provided.");
    server->cb = cb;

    task->stackSize = task_stackSize;
    task->prio = task_prio;
    strncpy(task->name, task_name, sizeof(task->name));

    if (buf)
    {
        server->data = buf;
    }
    else
    {
        server->data = (uint8_t *)k_malloc(buf_len);
        CHECK_COND_RETURN_MSG(!server->data, -1, "Error allocating memory.");
    }
    server->data_len = buf_len;

    rc = UdpSocket_init(udp);
    CHECK_COND_RETURN(rc < 0, rc);

    rc = UdpSocket_bind(udp, port);
    CHECK_COND_RETURN(rc < 0, rc);

    rc = RTOS_TASK_CREATE_DYNAMIC(
        &task->handle,
        udp_server_task,
        task->name,
        task->stack,
        task->stackSize,
        (void *)server,
        task->prio);
    if (rc < 0)
    {
        LOG_ERR("Failed creating udp server task (%d)", rc);
        return rc;
    }

    return 0;
}

