/*******************************************************************************
 *  @file: MqttClient.h
 *   
 *  @brief: Header for MqttClient module.
*******************************************************************************/
#ifndef MQTTCLIENT_H
#define MQTTCLIENT_H

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>

#if !defined(CONFIG_MQTTCLIENT_SERVER_PORT)
#define MQTTCLIENT_SERVER_PORT      1883
#else
#define MQTTCLIENT_SERVER_PORT      CONFIG_MQTTCLIENT_SERVER_PORT
#endif

#if !defined(CONFIG_MQTTCLIENT_SERVER_ADDR)
#error "CONFIG_MQTTCLIENT_SERVER_ADDR must be defined."
#else
#define MQTTCLIENT_SERVER_ADDR      CONFIG_MQTTCLIENT_SERVER_ADDR
#endif

#if !defined(CONFIG_MQTTCLIENT_POLL_TIMEOUT_MS)
#define MQTTCLIENT_POLL_TIMEOUT_MS   1000
#else
#define MQTTCLIENT_POLL_TIMEOUT_MS   CONFIG_MQTTCLIENT_POLL_TIMEOUT_MS
#endif

#if !defined(CONFIG_MQTTCLIENT_PUBLISH_RX_BUFFER_SIZE)
#define MQTTCLIENT_PUBLISH_RX_BUFFER_SIZE   256
#else
#define MQTTCLIENT_PUBLISH_RX_BUFFER_SIZE   CONFIG_MQTTCLIENT_PUBLISH_RX_BUFFER_SIZE
#endif

#if !defined(CONFIG_MQTTCLIENT_RX_BUFFER_SIZE)
#define MQTTCLIENT_RX_BUFFER_SIZE   256
#else
#define MQTTCLIENT_RX_BUFFER_SIZE   CONFIG_MQTTCLIENT_RX_BUFFER_SIZE
#endif

#if !defined(CONFIG_MQTTCLIENT_TX_BUFFER_SIZE)
#define MQTTCLIENT_TX_BUFFER_SIZE   256
#else
#define MQTTCLIENT_TX_BUFFER_SIZE   CONFIG_MQTTCLIENT_TX_BUFFER_SIZE
#endif

typedef struct MqttClient_PubTopic
{
    struct mqtt_topic topic;
    struct mqtt_binstr payload;
    uint16_t msg_id;
} MqttClient_PubTopic;

struct client_task
{
    /** @brief Task stack size. */
    uint16_t stackSize;
    /** @brief Task name. */
    char name[16];
    /** @brief Task prio. */
    uint8_t prio;
    /** @brief Internal */
    /** @brief Task Stack pointer. */
    RTOS_TASK_STACK *stack;
    /** @brief Loop task handle. */
    RTOS_TASK handle;
};

/** @brief Main MqttClient object.
*/
typedef struct MqttClient
{
    /** @brief The client object. */
    struct mqtt_client mclient;
    /** @brief Broker object. */
    struct sockaddr_storage broker;
    /** @brief Bool indicating connection status. */
    bool connected;
    /** @brief File descriptor for tcp polling. */
    struct pollfd fds[1];
    /** @brief Poll timeout (ms). */
    uint16_t poll_timeout_ms;
    /** @brief Buffer to store recieved publish payload. */
    uint8_t publish_rx_buffer[MQTTCLIENT_PUBLISH_RX_BUFFER_SIZE];
    /** @brief Flag indicating publish payload received. */
    bool publish_rx_ready;
    /** @brief Internal thread task. */
    struct client_task task;
    /** @brief Buffers. */
    uint8_t rx_buffer[MQTTCLIENT_RX_BUFFER_SIZE];
    uint8_t tx_buffer[MQTTCLIENT_TX_BUFFER_SIZE];

} MqttClient;

/******************************************************************************
    [docexport MqttClient_setTopic]
*//**
    @brief Creates a publish topic. Use for calls to MqttClient_publish.
    @param[in] topic  Pointer to MqttClient_PubTopic instance.
    @param[in] topic_str  Topic string.
    @param[in] qos  QoS for message (use enum mqtt_qos).
    MQTT_QOS_0_AT_MOST_ONCE
    MQTT_QOS_1_AT_LEAST_ONCE
    MQTT_QOS_2_EXACTLY_ONCE
******************************************************************************/
void
MqttClient_setTopic(
    MqttClient_PubTopic *tp,
    const char *topic_str,
    uint8_t qos);

/******************************************************************************
    [docexport MqttClient_publish]
*//**
    @brief Publish data to a topic.
    @param[in] mqc  Pointer to MqttClient instance.
    @param[in] topic  Pointer to MqttClient_PubTopic instance.
    @param[in] payload  Payload data buffer.
    @param[in] payload_len  Length of the payload to publish.
******************************************************************************/
int
MqttClient_publish(
    MqttClient *mqc,
    MqttClient_PubTopic *tp,
    uint8_t *payload,
    uint32_t payload_len);

/******************************************************************************
    [docexport MqttClient_connect]
*//**
    @brief Performs mqtt connect to server.
    @param[in] mqc  Pointer to MqttClient instance.
    @return Returns 0 on success or negative error.
******************************************************************************/
int
MqttClient_connect(MqttClient *mqc);

/******************************************************************************
    [docexport MqttClient_init]
*//**
    @brief Initialize the MqttClient object for use.
    @param[in] mqc  Pointer to MqttClient object to initialize.
    @param[in] client_id  Unique string for client id.
******************************************************************************/
int
MqttClient_init(MqttClient *mqc, char *client_id);
#endif
