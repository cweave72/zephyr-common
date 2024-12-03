/*******************************************************************************
 *  @file: PbGeneric.c
 *  
 *  @brief: General functions for Protobuf handling.
*******************************************************************************/
#include <zephyr/logging/log.h>
#include "PbGeneric.h"

LOG_MODULE_REGISTER(PbGeneric, CONFIG_PBGENERIC_LOG_LEVEL);

/******************************************************************************
    [docimport Pb_pack]
*//**
    @brief Packs a message struct to a buffer of bytes.
    @param[in] buf  Pointer to destination of packed protobuf message buffer.
    @param[in] buflen  Length of the buffer.
    @param[in] src  Pointer to the source struct to pack.
    @param[in] fields  Pointer to the protobuf message fields object.
    @return Returns the number of bytes written to the stream.
******************************************************************************/
uint32_t
Pb_pack(uint8_t *buf, uint32_t buflen, void *src, const void *fields)
{
    bool status;
    pb_ostream_t stream = pb_ostream_from_buffer(buf, buflen);
    status = pb_encode(&stream, (pb_msgdesc_t *)fields, src);
    if (!status)
    {
        LOG_ERR("pb_encode failure: %s", PB_GET_ERROR(&stream));
        return 0;
    }
    return stream.bytes_written;
}

/******************************************************************************
    [docimport Pb_unpack]
*//**
    @brief Unpacks a protobuf blob.
    @param[in] buf  Pointer to packed protobuf message buffer.
    @param[in] len  Length of the message.
    @param[in] target  Pointer to the target struct to unpack into.
    @param[in] fields  Pointer to the protobuf message fields object.
    @return Returns true on success; false on failure.
******************************************************************************/
bool
Pb_unpack(uint8_t *buf, uint32_t len, void *target, const void *fields)
{
    bool status;
    pb_istream_t stream = pb_istream_from_buffer(buf, len);
    
    status = pb_decode(&stream, (pb_msgdesc_t *)fields, target);
    if (!status)
    {
        LOG_ERR("pb_decode failure: %s", PB_GET_ERROR(&stream));
    }
    return status;
}

/******************************************************************************
    [docimport Pb_pack_delimted]
*//**
    @brief Packs a message struct to a buffer of bytes, delimited with varints.
    @param[in] stream  Pointer to stream context.
    @param[in] src  Pointer to the source struct to pack.
    @param[in] fields  Pointer to the protobuf message fields object.
    @return Returns the number of bytes written to the stream.
******************************************************************************/
uint32_t
Pb_pack_delimited(
    pb_ostream_t *stream,
    void *src,
    const void *fields)
{
    bool status = pb_encode_ex(
        stream,
        (pb_msgdesc_t *)fields,
        src,
        PB_ENCODE_DELIMITED);
    if (!status)
    {
        LOG_ERR("pb_encode_ex failure: %s", PB_GET_ERROR(stream));
        return 0;
    }
    return stream->bytes_written;
}

/******************************************************************************
    [docimport Pb_unpack_delimited]
*//**
    @brief Unpacks a protobuf blob delimted with varints.
    @param[in] stream  Pointer to stream context.
    @param[in] target  Pointer to the target struct to unpack into.
    @param[in] fields  Pointer to the protobuf message fields object.
    @return Returns true on success; false on failure.
******************************************************************************/
bool
Pb_unpack_delimited(
    pb_istream_t *stream,
    void *target,
    const void *fields)
{
    bool status = pb_decode_ex(
        stream,
        (pb_msgdesc_t *)fields,
        target,
        PB_DECODE_DELIMITED);
    if (!status)
    {
        LOG_ERR("pb_decode failure_ex: %s", PB_GET_ERROR(stream));
    }
    return status;
}
