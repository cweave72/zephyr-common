/*******************************************************************************
 *  @file: WifiConnect.c
 *  
 *  @brief: Library which allows connecting to local wifi network.
*******************************************************************************/
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/logging/log.h>
#include <errno.h>
#include "WifiConnect.h"
#include "RtosUtils.h"

/** @brief Initialize the logging module. */
LOG_MODULE_REGISTER(WifiConnect, CONFIG_WIFICONNECT_LOG_LEVEL);

#define FLAG_CONNECTED      ((uint32_t)0x1 << 0)
#define FLAG_DISCONNECTED   ((uint32_t)0x1 << 1)
#define FLAG_IP_OBTAINED    ((uint32_t)0x1 << 2)
#define FLAG_DO_CONNECT     ((uint32_t)0x1 << 3)

static K_SEM_DEFINE(connect_sem, 0, 1);
static K_SEM_DEFINE(monitor_started, 0, 1);

static RTOS_FLAGS_CREATE(wifi_flags);

#define MONITOR_STACK_SIZE (2*1024)
static RTOS_TASK mon_thread;
static RTOS_TASK_STACK mon_thread_stack;
static void monitor_thread(void *arg0, void *arg1, void *arg2);

struct monitor_thread_args
{
    const char *ssid;
    const char *pass;
};

static struct monitor_thread_args monitor_args;

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;

static bool initialized = false;

static void 
handle_connect_result(struct net_mgmt_event_callback *cb)
{
    struct wifi_status *status = (struct wifi_status *)cb->info;

    if (status->status)
    {
        char *reason;

        switch (status->conn_status)
        {
        case WIFI_STATUS_CONN_FAIL:
            reason = "likely incorrect key";
            break;
        case WIFI_STATUS_CONN_WRONG_PASSWORD:
            reason = "wrong password";
            break;
        case WIFI_STATUS_CONN_TIMEOUT:
            reason = "timed out";
            break;
        case WIFI_STATUS_CONN_AP_NOT_FOUND:
            reason = "AP not found";
            break;
        case WIFI_STATUS_CONN_LAST_STATUS:
            reason = "last status";
            break;
        default:
            reason = "unknown";
        }

        LOG_ERR("Connection request failed: reason=%s", reason);
    }
    else
    {
        RTOS_FLAGS_SET(&wifi_flags, FLAG_CONNECTED);
    }
}

static void 
handle_disconnect_result(struct net_mgmt_event_callback *cb)
{
    struct wifi_status *status = (struct wifi_status *)cb->info;

    if (status->status)
    {
        char *reason;
        switch (status->disconn_reason)
        {
        case WIFI_REASON_DISCONN_UNSPECIFIED:
            reason = "unspecified";
            break;
        case WIFI_REASON_DISCONN_USER_REQUEST:
            reason = "user request";
            break;
        case WIFI_REASON_DISCONN_AP_LEAVING:
            reason = "AP leaving";
            break;
        case WIFI_REASON_DISCONN_INACTIVITY:
            reason = "inactivity";
            break;
        default:
            reason = "unknown";
        }

        LOG_ERR("Wifi disconnect: reason=%s (%d)", reason, status->disconn_reason);
    }
    else
    {
        LOG_INF("Wifi disconnect success");
    }

    RTOS_FLAGS_SET(&wifi_flags, FLAG_DISCONNECTED);
}

static void
handle_ipv4_result(struct net_if *iface)
{
    int i = 0;
    char buf[NET_IPV4_ADDR_LEN];

    for (i = 0; i < NET_IF_MAX_IPV4_ADDR; i++)
    {
        if (iface->config.ip.ipv4->unicast[i].ipv4.addr_type != NET_ADDR_DHCP)
        {
            continue;
        }

        LOG_INF("IPv4 address: %s",
                net_addr_ntop(AF_INET,
                              &iface->config.ip.ipv4->unicast[i].ipv4.address.in_addr,
                              buf, sizeof(buf)));
        LOG_INF("Subnet: %s",
                net_addr_ntop(AF_INET,
                              &iface->config.ip.ipv4->unicast[i].netmask,
                              buf, sizeof(buf)));
        LOG_INF("Gateway: %s",
                net_addr_ntop(AF_INET,
                              &iface->config.ip.ipv4->gw,
                              buf, sizeof(buf)));
        }

        RTOS_FLAGS_SET(&wifi_flags, FLAG_IP_OBTAINED);
}

static void
event_handler(
    struct net_mgmt_event_callback *cb,
    uint32_t mgmt_event,
    struct net_if *iface)
{
    switch (mgmt_event)
    {
        case NET_EVENT_WIFI_CONNECT_RESULT:
            handle_connect_result(cb);
            break;

        case NET_EVENT_WIFI_DISCONNECT_RESULT:
            handle_disconnect_result(cb);
            break;

        case NET_EVENT_IPV4_ADDR_ADD:
            handle_ipv4_result(iface);
            break;

        default:
            break;
    }
}

static void
connect(const char *ssid, const char *pass)
{
    struct net_if *iface = net_if_get_default();

    struct wifi_connect_req_params wifi_params = {0};

    wifi_params.ssid = ssid;
    wifi_params.psk = pass;
    wifi_params.ssid_length = strlen(ssid);
    wifi_params.psk_length = strlen(pass);
    wifi_params.channel = WIFI_CHANNEL_ANY;
    wifi_params.security = WIFI_SECURITY_TYPE_PSK;
    wifi_params.band = WIFI_FREQ_BAND_2_4_GHZ; 
    wifi_params.mfp = WIFI_MFP_OPTIONAL;

    LOG_INF("Connecting to SSID: %s", wifi_params.ssid);

    net_mgmt(NET_REQUEST_WIFI_CONNECT,
             iface,
             &wifi_params,
             sizeof(struct wifi_connect_req_params));
}

static void
status(void)
{
    struct net_if *iface = net_if_get_default();
    
    struct wifi_iface_status status = {0};

    if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS,
                 iface,
                 &status,
                 sizeof(struct wifi_iface_status)))
    {
        LOG_ERR("WiFi Status Request Failed");
    }

    if (status.state >= WIFI_STATE_ASSOCIATED) {
        LOG_INF("SSID: %-32s", status.ssid);
        LOG_INF("Band: %s", wifi_band_txt(status.band));
        LOG_INF("Channel: %d", status.channel);
        LOG_INF("Security: %s", wifi_security_txt(status.security));
        LOG_INF("RSSI: %d", status.rssi);
    }
}

static void disconnect(void)
{
    struct net_if *iface = net_if_get_default();

    if (net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0))
    {
        LOG_ERR("WiFi Disconnection Request Failed");
    }
}

/******************************************************************************
    [docimport WifiConnect_getState]
*//**
    @brief Gets the current interface state.
******************************************************************************/
bool
WifiConnect_getState(void)
{
    struct net_if *iface = net_if_get_default();
    struct wifi_iface_status status = {0};

    if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS,
                 iface,
                 &status,
                 sizeof(struct wifi_iface_status)))
    {
        LOG_ERR("WiFi Status Request Failed");
        return -1;
    }

    return (status.state >= WIFI_STATE_ASSOCIATED) ? true : false;
}

/******************************************************************************
    [docimport WifoConnect_connect]
*//**
    @brief Performs a connection request.
    @param[in] ssid  SSID of network.
    @param[in] pass  Password.
******************************************************************************/
int
WifiConnect_connect(const char *ssid, const char *pass)
{
    if (!initialized)
    {
        WifiConnect_init(ssid, pass);
    }

    RTOS_FLAGS_SET(&wifi_flags, FLAG_DO_CONNECT);

    if (k_sem_take(&connect_sem, K_MSEC(30000)) != 0)
    {
        LOG_ERR("Timeout waiting for wifi connection.");
        return -1;
    }
    
    return 0;
}

/******************************************************************************
    [docimport WifiConnect_init]
*//**
    @brief Initializes a wifi connection.
    @param[in] ssid  SSID of network.
    @param[in] pass  Password.
******************************************************************************/
int
WifiConnect_init(const char *ssid, const char *pass)
{
    net_mgmt_init_event_callback(
        &wifi_cb,
        event_handler,
        NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT);

    net_mgmt_init_event_callback(
        &ipv4_cb,
        event_handler,
        NET_EVENT_IPV4_ADDR_ADD);

    net_mgmt_add_event_callback(&wifi_cb);
    net_mgmt_add_event_callback(&ipv4_cb);

    monitor_args.ssid = ssid;
    monitor_args.pass = pass;

    int ret = RTOS_TASK_CREATE_DYNAMIC(
        &mon_thread,
        monitor_thread,
        "wifi_monitor",
        &mon_thread_stack,
        MONITOR_STACK_SIZE,
        (void *)&monitor_args,
        10);
    if (ret != 0)
    {
        LOG_ERR("Failed creating monitor thread (%d)", ret);
        return ret;
    }

    k_sem_take(&monitor_started, K_FOREVER);
    initialized = true;

    LOG_INF("WifiConnect_init complete.");

    return 0;
}

/******************************************************************************
    monitor_thread
*//**
    @brief Wifi monitor thread.
******************************************************************************/
static void
monitor_thread(void *arg0, void *arg1, void *arg2)
{
    struct monitor_thread_args *args = (struct monitor_thread_args *)arg0;
    const char *ssid = args->ssid;
    const char *pass = args->pass;
    bool give_sem_on_connect = false;
    struct net_if *iface = net_if_get_default();

    (void)arg1;
    (void)arg2;

    LOG_INF("Wifi monitor thread started. %s, %s", ssid, pass);
    k_sem_give(&monitor_started);

    while (1)
    {
        uint32_t flags;

        flags = RTOS_PEND_ANY_FLAGS_MS(
            &wifi_flags,
            FLAG_DO_CONNECT | FLAG_CONNECTED | FLAG_IP_OBTAINED | FLAG_DISCONNECTED,
            10000);

        if (flags == 0)
        {
            /* Maintenance timeout. */
            LOG_DBG("Wifi monitor heartbeat.");
            continue;
        }

        if (flags & FLAG_DO_CONNECT)
        {
            LOG_INF("Wifi connect initiated.");
            RTOS_FLAGS_CLR(&wifi_flags, FLAG_DO_CONNECT);
            give_sem_on_connect = true;
            connect(ssid, pass);
        }

        if (flags & FLAG_CONNECTED)
        {
            LOG_INF("Wifi connected.");
            status();
            RTOS_FLAGS_CLR(&wifi_flags, FLAG_CONNECTED);
            if (give_sem_on_connect)
            {
                k_sem_give(&connect_sem);
                give_sem_on_connect = false;
            }
        }

        if (flags & FLAG_IP_OBTAINED)
        {
            LOG_INF("Wifi IP obtained.");
            RTOS_FLAGS_CLR(&wifi_flags, FLAG_IP_OBTAINED);
        }

        if (flags & FLAG_DISCONNECTED)
        {
            RTOS_FLAGS_CLR(&wifi_flags, FLAG_DISCONNECTED);

            /* Sleep before attempting to reconnect. */
            RTOS_TASK_SLEEP_ms(3000);

            net_if_down(iface);
            LOG_INF("Interface down.");
            RTOS_TASK_SLEEP_ms(500);

            net_if_up(iface);
            RTOS_TASK_SLEEP_ms(500);
            LOG_INF("Interface up.");

            LOG_INF("Attempting to reconnect...");
            connect(ssid, pass);
        }
    }
}
