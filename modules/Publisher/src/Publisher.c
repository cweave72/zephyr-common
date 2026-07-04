/*******************************************************************************
 *  @file: Publisher.c
 *
 *  @brief: Encapsulates MQTT topic publishing.
*******************************************************************************/
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "PbGeneric.h"
#include "Publisher.h"

LOG_MODULE_REGISTER(Publisher, CONFIG_PUBLISHER_LOG_LEVEL);

static MqttClient mqttClient;
static char clientId[24];

Publisher_topic_t sysmsg_topic;
static Publisher_topic_t sysid_topic;
static Publisher_SystemId  sysid;

/******************************************************************************
    [docimport Publisher_createTopic]
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
    Publisher_topic_type_t type)
{
    char *_topic_str = topic->topic_str;
    LOG_INF("Creating topic: %s", topic_str);

    topic->type = type;
    snprintf(_topic_str, sizeof(topic->topic_str), "%s/%s", topic_str, clientId);
    MqttClient_setTopic(&topic->client_topic, _topic_str, MQTT_QOS_0_AT_MOST_ONCE);
}

/******************************************************************************
    [docimport Publisher_setProtobuf]
*//**
    @brief Sets the parameters for a protobuf type topic.
******************************************************************************/
void
Publisher_setProtobuf(
    Publisher_topic_t *topic,
    void *fields,
    void *src,
    uint8_t *buf,
    uint32_t bufsize)
{
    Publisher_pb_t *pb = &topic->data.protobuf;
    
    pb->fields = fields;
    pb->src = src;
    pb->buf = buf;
    pb->bufsize = bufsize;
}

/******************************************************************************
    [docimport Publisher_publish]
*//**
    @brief Sends the topic.
    @param[in] topic  Pointer to registered topic. Data is embedded in the topic
    object
    @return Returns 0 on success. Negative error on fail.
******************************************************************************/
int
Publisher_publish(Publisher_topic_t *topic)
{
    Publisher_bytes_t *bytes = &topic->data.bytes;
    Publisher_str_t *string = &topic->data.string;
    Publisher_msg_t *msg = &topic->data.msg;
    Publisher_pb_t *pb = &topic->data.protobuf;
    int ret;
    uint32_t packed_len;

    /* Publish topic specific to type. */
    switch (topic->type)
    {
    case PUB_TYPE_BYTES:
        ret = MqttClient_publish(&mqttClient,
                                 &topic->client_topic,
                                 bytes->bytes,
                                 bytes->size);
        break;

    case PUB_TYPE_STRING:
        ret = MqttClient_publish(&mqttClient,
                                 &topic->client_topic,
                                 (uint8_t *)string->str,
                                 string->size);
        break;

    case PUB_TYPE_MSG:
        msg->buf[0] = msg->level;
        ret = MqttClient_publish(&mqttClient,
                                 &topic->client_topic,
                                 (uint8_t *)msg->buf,
                                 msg->size+1);
        break;

    case PUB_TYPE_PROTOBUF:
    {
        packed_len = Pb_pack(pb->buf, pb->bufsize, pb->src, pb->fields);
        if (packed_len > 0)
        {
            ret = MqttClient_publish(&mqttClient,
                                     &topic->client_topic,
                                     pb->buf,
                                     packed_len);
        }
        else
        {
            LOG_ERR("Error packing protobuf for publishing (packed_len=%u).", packed_len);
            ret = -EINVAL;
        }
        break;
    }

    default:
        ret = -EINVAL;
        break;
    }

    return ret;
}

/******************************************************************************
    [docimport Publisher_sendSysId]
*//**
    @brief Sends the System ID message.
******************************************************************************/
void
Publisher_sendSysId(void)
{
    Publisher_publish(&sysid_topic);
}

/******************************************************************************
    [docimport Publisher_init]
*//**
    @brief Publisher initializer. Derives a client id from the last octet
      of the device's IP address and initializes the underlying MqttClient.
    @param[in] ip_addr  The device's IP address string (e.g. "192.168.1.42").
    @return Returns 0 on success or negative error.
******************************************************************************/
int
Publisher_init(const char *ip_addr)
{
    const char *last_dot = strrchr(ip_addr, '.');
    const char *last_octet = last_dot ? (last_dot + 1) : ip_addr;
    uint8_t *buf;

    snprintf(clientId, sizeof(clientId), "pubid-%s", last_octet);

    Publisher_createTopic(&sysmsg_topic, "sysmsg", PUB_TYPE_MSG);

    buf = k_malloc(Publisher_SystemId_size);
    if (!buf)
    {
        LOG_ERR("Error allocating memory for publisher system Id message.");
        return -ENOMEM;
    }

    Publisher_setProtobuf(
        &sysid_topic, 
        (void *)Publisher_SystemId_fields,
        &sysid,
        buf,
        Publisher_SystemId_size);

    /* Copy provided ip address into the system id struct. */
    strncpy(sysid.ip, ip_addr, sizeof(sysid.ip));

    Publisher_createTopic(&sysid_topic, "sysid", PUB_TYPE_PROTOBUF);

    LOG_INF("Initializing Publisher with clientId: %s", clientId);
    return MqttClient_init(&mqttClient, clientId);
}
