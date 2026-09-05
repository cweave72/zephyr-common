/*******************************************************************************
 *  @file: net_ip.c
 *
 *  @brief: Static IPv4 addressing. See net_ip.h.
*******************************************************************************/
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>

#include "net_ip.h"

LOG_MODULE_DECLARE(app, CONFIG_APP_LOG_LEVEL);

/******************************************************************************
    [docimport app_net_static_ip_apply]
*//**
    @brief Applies CONFIG_APP_IPV4_ADDR/MASK/GW to the default interface.
******************************************************************************/
void
app_net_static_ip_apply(void)
{
    struct net_if *iface;
    struct net_if_addr *ifaddr;
    struct in_addr my_ipv4_addr;
    struct in_addr my_ipv4_mask;
    struct in_addr my_ipv4_gw;

    iface = net_if_get_default();

    LOG_INF("Setting static IP address for iface:");
    LOG_INF("  addr: %s", CONFIG_APP_IPV4_ADDR);
    LOG_INF("  mask: %s", CONFIG_APP_IPV4_MASK);
    LOG_INF("  gw  : %s", CONFIG_APP_IPV4_GW);

    net_addr_pton(AF_INET, CONFIG_APP_IPV4_ADDR, &my_ipv4_addr);
    net_addr_pton(AF_INET, CONFIG_APP_IPV4_MASK, &my_ipv4_mask);
    net_addr_pton(AF_INET, CONFIG_APP_IPV4_GW, &my_ipv4_gw);

    ifaddr = net_if_ipv4_addr_add(iface, &my_ipv4_addr, NET_ADDR_MANUAL, 0);
    if (!ifaddr)
    {
        LOG_ERR("Error setting IP address");
        return;
    }

    net_if_ipv4_set_netmask_by_addr(iface, &my_ipv4_addr, &my_ipv4_mask);
    net_if_ipv4_set_gw(iface, &my_ipv4_gw);
}
