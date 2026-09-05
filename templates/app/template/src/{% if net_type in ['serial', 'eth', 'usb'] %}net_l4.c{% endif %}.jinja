/*******************************************************************************
 *  @file: net_l4.c
 *
 *  @brief: Shared connection-manager bring-up. See net_l4.h.
*******************************************************************************/
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_config.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/conn_mgr_monitor.h>

#include "net_l4.h"
#if defined(CONFIG_APP_USE_STATIC_IP)
#include "net_ip.h"
#endif

LOG_MODULE_DECLARE(app, CONFIG_APP_LOG_LEVEL);

#define EVENT_MASK (NET_EVENT_L4_CONNECTED | \
                    NET_EVENT_L4_DISCONNECTED)

static struct net_mgmt_event_callback mgmt_cb;
static bool connected = false;

static K_SEM_DEFINE(l4_connected, 0, 1);

static void
l4_event_handler(
    struct net_mgmt_event_callback *cb,
    uint32_t mgmt_event,
    struct net_if *iface)
{
    ARG_UNUSED(iface);
    ARG_UNUSED(cb);

    switch (mgmt_event)
    {
    case NET_EVENT_L4_CONNECTED:
        LOG_INF("NET_EVENT_L4_CONNECTED");
        connected = true;
        k_sem_give(&l4_connected);
        break;
    case NET_EVENT_L4_DISCONNECTED:
        if (connected)
        {
            LOG_INF("Network disconnected event");
            connected = false;
        }
        k_sem_reset(&l4_connected);
        break;
    default:
        break;
    }
}

/******************************************************************************
    [docimport app_net_l4_bringup]
*//**
    @brief Applies addressing, starts the network and blocks until L4 is up.
    @return Returns 0 on success, negative on error.
******************************************************************************/
int
app_net_l4_bringup(void)
{
    int ret;

    if (!IS_ENABLED(CONFIG_NET_CONNECTION_MANAGER))
    {
        LOG_ERR("Must set CONFIG_NET_CONNECTION_MANAGER=y in Kconfig.");
        return -1;
    }

    net_mgmt_init_event_callback(&mgmt_cb, l4_event_handler, EVENT_MASK);
    net_mgmt_add_event_callback(&mgmt_cb);
    conn_mgr_mon_resend_status();

#if defined(CONFIG_APP_USE_STATIC_IP)
    app_net_static_ip_apply();
#endif

    ret = net_config_init_app(NULL, "Initializing network");
    if (ret < 0)
    {
        LOG_ERR("Failed network init (%d)", ret);
        return ret;
    }

    LOG_INF("Waiting for network connection...");
    k_sem_take(&l4_connected, K_FOREVER);
    LOG_INF("Network connected.");

    return 0;
}
