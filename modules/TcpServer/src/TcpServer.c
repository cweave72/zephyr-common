/*******************************************************************************
 *  @file: TcpServer.c
 *  
 *  @brief: Library implementing a tcp server.
*******************************************************************************/
#include <string.h>
#include <zephyr/logging/log.h>

#include "CheckCond.h"
#include "TcpServer.h"

/** @brief Initialize the logging module. */
LOG_MODULE_REGISTER(TcpServer, CONFIG_TCPSERVER_LOG_LEVEL);

#define KEEPALIVE_IDLE          (int)5
#define KEEPALIVE_COUNT         (int)3
#define KEEPALIVE_INTERVAL      (int)5

/******************************************************************************
    tcp_server_task
*//**
    @brief Main task loop for Tcp server.
******************************************************************************/
static void
tcp_server_task(void *p, void *arg1, void *arg2)
{
    TcpServer *server = (TcpServer *)p;
    TcpSocket *tcp = &server->tcpsock;
    TcpTask *task = &server->task;

    (void)arg1;
    (void)arg2;

    LOG_INF("Starting TcpServer Task: %s.", task->name);

    /** @brief Listen on port. */
    if (TcpSocket_listen(tcp, 2) != 0)
    {
        goto cleanup;
    }

    while (1)
    {
        int sock;

        LOG_DBG("Socket accepting connections on port %u: %s",
            (unsigned int)tcp->port, task->name);

        /** @brief Accept incoming connections. */
        sock = TcpSocket_accept(tcp,
                                KEEPALIVE_IDLE,
                                KEEPALIVE_INTERVAL,
                                KEEPALIVE_COUNT);
        if (sock < 0)
        {
            LOG_ERR("Exiting task %s due to socket accept error.",
                task->name);
            break;
        }

        /** @brief Receive socket data until error or disconnect. */
        int read_done = 0;
        int callback_done = 0;

        while (1)
        {
            int num_read = 0;

            if (!read_done)
            {
                num_read = TcpSocket_read(sock, server->data, server->data_len);
                if (num_read < 0)
                {
                    LOG_ERR("Closing socket due to read error.");
                    break;
                }
                else if (num_read == 0)
                {
                    /*  Set flag to prevent further reading, but let the callback
                        continue to send data until it's finished.
                    */
                    read_done = 1;
                }
            }
            else
            {
                /*  Since we are done reading, yield for 1 tick so that other
                    threads can run, otherwise the callback would consume the
                    cpu entirely.
                */
                RTOS_TASK_SLEEP_ticks(1);
            }

            /** @brief Call callback to allow rx and tx on socket. */
            server->cb(
                (void *)server,
                sock,
                server->data,
                (uint16_t)num_read,
                &callback_done);

            /*  When both sided of the connection are done, shutdown the
                connection.
            */
            if (callback_done && read_done)
            {
                break;
            }
        }

        LOG_DBG("Closing socket connection.");
        TcpSocket_shutdown(sock, 2);
        TcpSocket_close(sock);
    }

cleanup:
    TcpSocket_close(tcp->sock);
}

/******************************************************************************
    [docimport TcpServer_init]
*//**
    @brief Initializes a TCP server.
    @param[in] server  Pointer to uninitialized TcpServer object.
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
TcpServer_init(
    TcpServer *server,
    uint16_t port,
    uint8_t *buf,
    uint32_t buf_len,
    uint16_t task_stackSize,
    char *task_name,
    uint8_t task_prio,
    TcpServer_cb *cb)
{
    TcpSocket *tcp = &server->tcpsock;
    TcpTask *task = &server->task;
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
        server->data = (uint8_t *)malloc(buf_len);
        CHECK_COND_RETURN_MSG(!server->data, -1, "Error allocating memory.");
    }
    server->data_len = buf_len;

    rc = TcpSocket_init(tcp);
    CHECK_COND_RETURN(rc < 0, rc);

    rc = TcpSocket_bind(tcp, port);
    CHECK_COND_RETURN(rc < 0, rc);

    rc = RTOS_TASK_CREATE_DYNAMIC(
        &task->handle,
        tcp_server_task,
        task->name,
        task->stack,
        task->stackSize,
        (void *)server,
        task->prio);
    if (rc < 0)
    {
        LOG_ERR("Failed creating tcp server task (%d)", rc);
        return rc;
    }

    return 0;
}
