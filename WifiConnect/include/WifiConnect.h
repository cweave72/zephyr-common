/*******************************************************************************
 *  @file: WifiConnect.h
 *   
 *  @brief: Header providing exported Wifi connection functions.
*******************************************************************************/
#ifndef WIFICONNECT_H
#define WIFICONNECT_H



/******************************************************************************
    [docexport WifoConnect_connect]
*//**
    @brief Performs a connection request.
    @param[in] ssid  SSID of network.
    @param[in] pass  Password.
******************************************************************************/
int
WifiConnect_connect(const char *ssid, const char *pass);

/******************************************************************************
    [docexport WifiConnect_init]
*//**
    @brief Initializes a wifi connection.
******************************************************************************/
void
WifiConnect_init(void);
#endif
