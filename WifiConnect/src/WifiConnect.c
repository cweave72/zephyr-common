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

static RTOS_FLAGS_CREATE(wifi_flags);

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;

static bool initialized = false;

static void 
handle_connect_result(struct net_mgmt_event_callback *cb)
{
    struct wifi_status *status = (struct wifi_status *)cb->info;

    if (status->status)
    {
        LOG_ERR("Connection request failed (%d)\n", status->status);
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
        LOG_ERR("Error on disconnect (%d)", status->status);
    }
    else
    {
        LOG_INF("Disconnected");
        RTOS_FLAGS_SET(&wifi_flags, FLAG_DISCONNECTED);
    }
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

    if (net_mgmt(NET_REQUEST_WIFI_CONNECT,
                 iface,
                 &wifi_params,
                 sizeof(struct wifi_connect_req_params)))
    {
        LOG_ERR("WiFi Connection Request Failed");
    }
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
    uint32_t flags;

    if (!initialized)
    {
        WifiConnect_init();
    }
    
    LOG_DBG("Staring Wifi connection process.");
    RTOS_TASK_SLEEP_ms(3000);

    connect(ssid, pass);
    LOG_DBG("Waiting for connection...");

    flags = RTOS_PEND_ALL_FLAGS_MS(
        &wifi_flags,
        FLAG_CONNECTED | FLAG_IP_OBTAINED,
        10000);
    if (flags == 0)
    {
        LOG_ERR("Timeout on connection request.");
        return -1;
    }
    RTOS_FLAGS_CLR(&wifi_flags, FLAG_CONNECTED | FLAG_IP_OBTAINED);

    status();
    LOG_INF("Wifi successfully connected.");

    return 0;
}

/******************************************************************************
    [docimport WifiConnect_init]
*//**
    @brief Initializes a wifi connection.
******************************************************************************/
void
WifiConnect_init(void)
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

    initialized = true;
}
