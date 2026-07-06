/*******************************************************************************
 *  @file: Publisher.h
 *
 *  @brief: Encapsulates MQTT topic publishing.
*******************************************************************************/
#ifndef PUBLISHER_H
#define PUBLISHER_H

#include <stdio.h>
#include "MqttClient.h"

typedef enum Publisher_topic_type_t
{
    PUB_TYPE_BYTES    = 0,
    PUB_TYPE_STRING   = 1,
    PUB_TYPE_MSG      = 2,
    PUB_TYPE_PROTOBUF = 3
} Publisher_topic_type_t;

#define PUB_MSG_LEVEL_CRITICAL 0
#define PUB_MSG_LEVEL_ERROR    1
#define PUB_MSG_LEVEL_WARN     2
#define PUB_MSG_LEVEL_INFO     3
#define PUB_MSG_LEVEL_DEBUG    4

typedef struct Publisher_bytes_t
{
    /* Buffer allocated by the caller. */
    uint8_t *bytes;
    /* Size of the valid data in bytes. */
    uint32_t size;
} Publisher_bytes_t;

typedef struct Publisher_str_t
{
    char str[CONFIG_PUBLISHER_MAX_STRING_SIZE];
    uint32_t size;
} Publisher_str_t;

typedef struct Publisher_msg_t
{
    uint8_t level;
    uint8_t buf[CONFIG_PUBLISHER_MAX_STRING_SIZE+1];
    uint32_t size;
} Publisher_msg_t;

typedef struct Publisher_pb_t
{
    /* Protobuf fields. */
    void *fields;
    /* Protobuf loaded source struct pointer. */
    void *src;
    /* Buffer to pack into. */
    uint8_t *buf;
    /* Size of the buffer. */
    uint32_t bufsize;
} Publisher_pb_t;

/** @brief Publisher topic object.
*/
typedef struct Publisher_topic_t
{
    /* The topic type. */
    Publisher_topic_type_t type;

    /* Fixed topic string. */
    char topic_str[128];

    /* The underlying Mqtt client topic object. */
    MqttClient_PubTopic client_topic;

    union {
        Publisher_bytes_t bytes;
        Publisher_str_t string;
        Publisher_msg_t msg;
        Publisher_pb_t protobuf;
    } data;

} Publisher_topic_t;

extern Publisher_topic_t sysmsg_topic;

/* Macro helper for creating a publisher string. */
#define Publisher_setString(topic, fmt, ...)                                \
    (topic)->data.string.size = snprintf((topic)->data.string.str,          \
                    sizeof((topic)->data.string.str), fmt, ##__VA_ARGS__);

#define PUBLISHER_SETMSG(fmt, ...)                                         \
    sysmsg_topic.data.msg.size = snprintf(sysmsg_topic.data.msg.buf + 1,   \
        sizeof(sysmsg_topic.data.msg.buf), fmt, ##__VA_ARGS__);

#define PUBLISHER_SENDMSG(_level)             \
    sysmsg_topic.data.msg.level = (_level);   \
    Publisher_publish(&sysmsg_topic);

#define Publisher_sendMsg_CRITICAL(fmt, ...)    \
do {                                            \
    PUBLISHER_SETMSG(fmt, ##__VA_ARGS__);       \
    PUBLISHER_SENDMSG(PUB_MSG_LEVEL_CRITICAL);  \
} while (0)

#define Publisher_sendMsg_ERROR(fmt, ...)      \
do {                                           \
    PUBLISHER_SETMSG(fmt, ##__VA_ARGS__);      \
    PUBLISHER_SENDMSG(PUB_MSG_LEVEL_ERROR);    \
} while (0)

#define Publisher_sendMsg_WARN(fmt, ...)  \
do {                                         \
    PUBLISHER_SETMSG(fmt, ##__VA_ARGS__);    \
    PUBLISHER_SENDMSG(PUB_MSG_LEVEL_WARN);   \
} while (0)

#define Publisher_sendMsg_INFO(fmt, ...)        \
do {                                            \
    PUBLISHER_SETMSG(fmt, ##__VA_ARGS__);       \
    PUBLISHER_SENDMSG(PUB_MSG_LEVEL_INFO);      \
} while (0)

#define Publisher_sendMsg_DEBUG(fmt, ...)       \
do {                                            \
    PUBLISHER_SETMSG(fmt, ##__VA_ARGS__);       \
    PUBLISHER_SENDMSG(PUB_MSG_LEVEL_DEBUG);     \
} while (0)

/******************************************************************************
    [docexport Publisher_createTopic]
*//**
    @brief Creates a topic. The QoS will be MQTT_QOS_0_AT_MOST_ONCE.
    @param[in] topic  Pointer to topic object to initialize.
    @param[in] topic_str  The topic string.
    @param[in] type  The topic type.
******************************************************************************/
void
Publisher_createTopic(
    Publisher_topic_t *topic,
    const char *topic_str,
    Publisher_topic_type_t type);

/******************************************************************************
    [docexport Publisher_setProtobuf]
*//**
    @brief Sets the parameters for a protobuf type topic.
******************************************************************************/
void
Publisher_setProtobuf(
    Publisher_topic_t *topic,
    void *fields,
    void *src,
    uint8_t *buf,
    uint32_t bufsize);

/******************************************************************************
    [docexport Publisher_publish]
*//**
    @brief Sends the topic.
    @param[in] topic  Pointer to registered topic. Data is embedded in the topic
    object
    @return Returns 0 on success. Negative error on fail.
******************************************************************************/
int
Publisher_publish(Publisher_topic_t *topic);

/******************************************************************************
    [docexport Publisher_sendSysId]
*//**
    @brief Sends the System ID message.
******************************************************************************/
void
Publisher_sendSysId(void);

/******************************************************************************
    [docexport Publisher_init]
*//**
    @brief Publisher initializer. Derives a client id from the last octet
      of the device's IP address and initializes the underlying MqttClient.
    @param[in] ip_addr  The device's IP address string (e.g. "192.168.1.42").
    @return Returns 0 on success or negative error.
******************************************************************************/
int
Publisher_init(const char *ip_addr);
#endif /* PUBLISHER_H */
