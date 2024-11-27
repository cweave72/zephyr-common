/*******************************************************************************
 *  @file: PbGeneric.c
 *  
 *  @brief: General functions for Protobuf handling.
*******************************************************************************/
#include <zephyr/logging/log.h>
#include "pb_encode.h"
#include "pb_decode.h"
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
