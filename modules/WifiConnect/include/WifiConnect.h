/*******************************************************************************
 *  @file: WifiConnect.h
 *   
 *  @brief: Header providing exported Wifi connection functions.
*******************************************************************************/
#ifndef WIFICONNECT_H
#define WIFICONNECT_H

typedef void WifiConnect_error_cb(void);

/******************************************************************************
    [docexport WifiConnect_getState]
*//**
    @brief Gets the current interface state.
******************************************************************************/
bool
WifiConnect_getState(void);

/******************************************************************************
    [docexport WifoConnect_connect]
*//**
    @brief Performs a connection request.
    @param[in] ssid  SSID of network.
    @param[in] pass  Password.
    @param[in] cb  Callback on unrecoverable error (set to NULL if not used).
******************************************************************************/
int
WifiConnect_connect(const char *ssid, const char *pass, WifiConnect_error_cb *cb);

/******************************************************************************
    [docexport WifiConnect_getIpInfo]
*//**
    @brief Gets current IP address, Netmask and Gateway strings.
    @param[out] ip  Pointer to IP address string.
    @param[out] netmask  Pointer to netmask string.
    @param[out] gw  Pointer to gateway address string.
    @return Returns the IP status.
******************************************************************************/
bool
WifiConnect_getIpInfo(char **ip, char **netmask, char **gw);

/******************************************************************************
    [docexport WifiConnect_init]
*//**
    @brief Initializes a wifi connection.
    @param[in] ssid  SSID of network.
    @param[in] pass  Password.
    @param[in] cb  Callback on unrecoverable error (set to NULL if not used).
******************************************************************************/
int
WifiConnect_init(const char *ssid, const char *pass, WifiConnect_error_cb *cb);
#endif
