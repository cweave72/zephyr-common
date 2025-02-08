/*******************************************************************************
 *  @file: MqttClient.c
 *  
 *  @brief: Module wrapping mqtt library interface for implementing an mqtt
    client.
*******************************************************************************/
#include <string.h>
#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>
#include "RtosUtils.h"
#include "MqttClient.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MqttClient, CONFIG_MQTTCLIENT_LOG_LEVEL);

/******************************************************************************
    event_handler
*//**
    @brief Mqtt event handler.
******************************************************************************/
static void
event_handler(struct mqtt_client *const client, const struct mqtt_evt *event)
{
    int err;
    
    MqttClient *mqc = (MqttClient *)client->user_data;

    LOG_DBG("MQTT event for client: %s", client->client_id.utf8);

    switch (event->type)
    {
    case MQTT_EVT_CONNACK:
        if (event->result != 0)
        {
            mqc->connected = false;
            LOG_ERR("MQTT connect failed %d", event->result);
            break;
        }
        
        mqc->connected = true;
        LOG_INF("MQTT client connected.");
        break;

    case MQTT_EVT_DISCONNECT:
        LOG_INF("MQTT client disconnected.");
        mqc->connected = false;

    case MQTT_EVT_SUBACK:
        if (event->result != 0)
        {
            LOG_ERR("MQTT SUBACK error %d", event->result);
            break;
        }
        LOG_INF("MQTT SUBACK message id: %u", event->param.suback.message_id);
        break;

    case MQTT_EVT_UNSUBACK:
        if (event->result != 0)
        {
            LOG_ERR("MQTT UNSUBACK error %d", event->result);
            break;
        }
        LOG_INF("MQTT UNSUBACK message id: %u",
            event->param.unsuback.message_id);
        break;

    case MQTT_EVT_PUBREC:
        /* Published message receive confirmation QoS 2*/
        if (event->result != 0)
        {
            LOG_ERR("MQTT PUBREC QoS 2 error %d", event->result);
            break;
        }

        LOG_INF("MQTT PUBREC QoS 2 message id: %u",
            event->param.pubrec.message_id);

        const struct mqtt_pubrel_param rel_param = {
            .message_id = event->param.pubrec.message_id
        };

        /* Must release the QoS2 publish message. */
        err = mqtt_publish_qos2_release(client, &rel_param);
        if (err < 0)
        {
            LOG_ERR("MQTT Failed to send PUBREL: %d", err);
        }
        break;

    case MQTT_EVT_PUBACK:
        /* Ack for publlished message with QoS 1. */
        if (event->result != 0)
        {
            LOG_ERR("MQTT PUBACK (QoS 1) error %d", event->result);
            break;
        }
        LOG_INF("MQTT PUBACK (QoS 1) packet id: %u",
            event->param.puback.message_id);
        break;

    case MQTT_EVT_PUBLISH:
        /* Publish message received to a subscribed topic. */
        if (event->result != 0)
        {
            LOG_ERR("MQTT PUBLISH error %d", event->result);
            break;
        }
        LOG_INF("MQTT PUBLISH msg recv'd: id=%u",
            event->param.publish.message_id);
        LOG_INF("MQTT PUBLISH topic: %s",
            event->param.publish.message.topic.topic.utf8);

        /* Get the payload. */
        int num = mqtt_read_publish_payload(
            client,
            mqc->publish_rx_buffer, 
            sizeof(mqc->publish_rx_buffer));
        if (num <= 0)
        {
            LOG_ERR("MQTT mqtt_read_publish_payload error: %d", num);
            break;
        }

        /* If QoS2, must send acknowledment. */
        if (event->param.publish.message.topic.qos == MQTT_QOS_2_EXACTLY_ONCE)
        {
            const struct mqtt_pubrec_param param = {
                .message_id = event->param.publish.message_id
            };
            err = mqtt_publish_qos2_receive(client, &param);
            if (err < 0)
            {
                LOG_ERR("MQTT PUBLISH QoS2 ack error: %d", err);
            }
            LOG_DBG("MQTT PUBLISH QoS2 ack id: %u",
                event->param.publish.message_id);
        }

        LOG_HEXDUMP_DBG(mqc->publish_rx_buffer, num, "PUBLISH rx payload");
        break;

    case MQTT_EVT_PUBREL:
        /* Release of Publish message with Qos2. */
        if (event->result != 0)
        {
            LOG_ERR("MQTT PUBREL (QoS 2) error %d", event->result);
            break;
        }
        LOG_INF("MQTT PUBREL (QoS 2) message id: %u.",
            event->param.pubrel.message_id);
        break;

    case MQTT_EVT_PUBCOMP:
        /* Confirmation of PUBLISH message with QoS 2. */
        if (event->result != 0)
        {
            LOG_ERR("MQTT PUBCOMP (QoS 2) error %d", event->result);
            break;
        }
        LOG_INF("MQTT PUBCOMP (QoS 2) message id: %u",
            event->param.pubcomp.message_id);
        break;

    case MQTT_EVT_PINGRESP:
        LOG_INF("MQTT PINGRESP packet received.");
        break;

    default:
        break;
    }
}

/******************************************************************************
    call_mqtt_input
*//**
    @brief Calls mqtt_input for client.
    @return Retuns 0 on success, negative code (errno) on error.
******************************************************************************/
static int
call_mqtt_input(struct mqtt_client *client)
{
    int ret;
    if ((ret = mqtt_input(client)) != 0)
    {
        LOG_ERR("mqtt_input error: %d", ret);
    }
    return ret;
}

/******************************************************************************
    wait_poll_input
*//**
    @brief Waits for data from server.
    @param[in] mqc  Pointer to MqttClient instance.
    @return Return 0 on successful read, -1 on error.
******************************************************************************/
static int
wait_poll_input(MqttClient *mqc)
{
    struct pollfd *pfds = mqc->fds;
    struct mqtt_client *client = &mqc->mclient;
    int ready;

    /* poll():
         0 : timeout waiting for fd ready.
        <0 : errno
        >0 : number of elements in pollfds whose revents are nonzero
    */
    if ((ready = poll(pfds, 1, (int)mqc->poll_timeout_ms)) < 0)
    {
        LOG_ERR("wait_poll_input error: %d", errno);
        return -1;
    }

    if (ready == 0)
    {
        goto ret_err;
    }

    /* There is only one fd to check. */
    if (pfds[0].revents & POLLIN)
    {
        /* Data received, let mqtt lib handle it. */
        LOG_DBG("MQTT packet received.");
        return call_mqtt_input(client);
    }
    if (pfds[0].revents & POLLHUP)
    {
        /* Hangup received. */
        LOG_WRN("MQTT POLLHUP.");
        goto ret_err;
    }
    if (pfds[0].revents & POLLERR)
    {
        /* Error */
        LOG_WRN("MQTT POLLERR.");
    }

ret_err:
    return -1;
}

/******************************************************************************
    client_thread
*//**
    @brief Main task loop for client.
******************************************************************************/
static void
client_thread(void *p, void *arg1, void *arg2)
{
    MqttClient *mqc = (MqttClient *)p;
    struct client_task *task = &mqc->task;
    struct mqtt_client *client = &mqc->mclient;

    (void)arg1;
    (void)arg2;

    LOG_INF("Starting mqtt client thread: %s.", task->name);

    while (1)
    {
        int ret;

        if (mqc->connected)
        {
            /* Poll for input data. */
            wait_poll_input(mqc);

            //ret = mqtt_ping(client);
            //if (ret < 0)
            //{
            //    LOG_ERR("mqtt_ping failed: %d", ret);
            //}

            mqtt_live(client);
            wait_poll_input(mqc);
        }
        else
        {
            LOG_INF("Attempting client connect: %s", client->client_id.utf8);
            MqttClient_connect(mqc);
        }

        RTOS_TASK_SLEEP_ms(1000);
    }
}

/******************************************************************************
    [docimport MqttClient_setTopic]
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
    uint8_t qos)
{
    tp->topic.qos = qos;
    tp->topic.topic.utf8 = (uint8_t *)topic_str;
    tp->topic.topic.size = strlen(topic_str);
    tp->msg_id = 0;
}

/******************************************************************************
    [docimport MqttClient_publish]
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
    uint32_t payload_len)
{
    struct mqtt_publish_param param;

    if (!mqc->connected)
    {
        LOG_WRN("Not connected, ignoring publish.");
        return 0;
    }

    param.message.topic.qos        = tp->topic.qos;
    param.message.topic.topic.utf8 = tp->topic.topic.utf8;
    param.message.topic.topic.size = tp->topic.topic.size;
    param.message.payload.data     = payload;
    param.message.payload.len      = payload_len;
    param.message_id               = tp->msg_id;
    param.dup_flag                 = 0;
    param.retain_flag              = 0;
    tp->msg_id++;

    return mqtt_publish(&mqc->mclient, &param);
}

/******************************************************************************
    [docimport MqttClient_connect]
*//**
    @brief Performs mqtt connect to server.
    @param[in] mqc  Pointer to MqttClient instance.
    @return Returns 0 on success or negative error.
******************************************************************************/
int
MqttClient_connect(MqttClient *mqc)
{
    struct mqtt_client *client = &mqc->mclient;
    int ret;

    ret = mqtt_connect(client);
    if (ret != 0)
    {
        LOG_ERR("mqtt_connect: %d", ret);
    }

    /*  On successfull connect, create file descriptors and wait for reply from
        server. */
    mqc->fds[0].fd     = client->transport.tcp.sock;
    mqc->fds[0].events = ZSOCK_POLLIN;

    return wait_poll_input(mqc);
}

/******************************************************************************
    [docimport MqttClient_init]
*//**
    @brief Initialize the MqttClient object for use.
    @param[in] mqc  Pointer to MqttClient object to initialize.
    @param[in] client_id  Unique string for client id.
******************************************************************************/
int
MqttClient_init(MqttClient *mqc, char *client_id)
{
    struct mqtt_client *client = &mqc->mclient;
    struct sockaddr_in *broker = (struct sockaddr_in *)&mqc->broker;
    int rc;
    
    LOG_INF("Initializing client: %s", client_id);
    mqc->connected = false;
    mqc->publish_rx_ready = false;
    mqc->poll_timeout_ms = MQTTCLIENT_POLL_TIMEOUT_MS;

    /* Init the Zephyr mqtt_client object. */
    mqtt_client_init(client);

    /* Initialize the broker */
    broker->sin_family = AF_INET;
    broker->sin_port   = htons(MQTTCLIENT_SERVER_PORT);
    inet_pton(AF_INET, MQTTCLIENT_SERVER_ADDR, &broker->sin_addr);

    client->broker           = broker;
    client->evt_cb           = event_handler;
    client->client_id.utf8   = (uint8_t *)client_id;
    client->client_id.size   = strlen(client_id);
    client->password         = NULL;
    client->user_name        = NULL;
    client->protocol_version = MQTT_VERSION_3_1_1;
    client->rx_buf           = mqc->rx_buffer;
    client->rx_buf_size      = sizeof(mqc->rx_buffer);
    client->tx_buf           = mqc->tx_buffer;
    client->tx_buf_size      = sizeof(mqc->tx_buffer);
    client->transport.type   = MQTT_TRANSPORT_NON_SECURE;
    client->user_data        = mqc;

    /* Configure thread task. */
    mqc->task.stackSize = CONFIG_MQTTCLIENT_STACK_SIZE;
    mqc->task.prio      = CONFIG_MQTTCLIENT_THREAD_PRIO;
    strncpy(mqc->task.name, client_id, sizeof(mqc->task.name));

    rc = RTOS_TASK_CREATE_DYNAMIC(
        &mqc->task.handle,
        client_thread,
        mqc->task.name,
        mqc->task.stack,
        mqc->task.stackSize,
        (void *)mqc,
        mqc->task.prio);
    if (rc < 0)
    {
        LOG_ERR("Failed creating mqtt client task (%d)", rc);
    }

    return rc;
}
