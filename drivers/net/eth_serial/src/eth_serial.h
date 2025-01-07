/*******************************************************************************
 *  @file: eth_serial.h
 *   
 *  @brief: Header for eth-serial driver.
*******************************************************************************/
#ifndef ETH_SERIAL_H
#define ETH_SERIAL_H

#include <stdbool.h>
#include <zephyr/device.h>
#include <zephyr/net_buf.h>
#include <zephyr/net/net_pkt.h>
#include <zephyr/net/net_if.h>

#if !defined(CONFIG_ETH_SERIAL_BUF_SIZE)
#define SERIAL_BUFFER_SIZE  CONFIG_ETH_SERIAL_BUF_SIZE
#else
#define SERIAL_BUFFER_SIZE  16

/** @brief Driver context object (modeled after drivers/net/slip.h)*/
struct eth_serial_context {
    bool init_done;
    uint8_t serial_buf[SERIAL_BUFFER_SIZE];
    uint8_t mac_addr[6];
    struct net_linkaddr ll_addr;
    int (*framer)(uint8_t *buf_in, uint32_t buf_in_len, uint8_t *enc_out, uint32_t max_enc_len);
    int (*deframer)(void *self, uint8_t *buf_in, uint32_t buf_in_len, uint8_t *buf_out, uint32_t max_buf_out);
    void *deframer_state;
};

#endif
