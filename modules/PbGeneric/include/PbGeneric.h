/*******************************************************************************
 *  @file: PbGeneric.h
 *   
 *  @brief: Header for PbGeneric.c
*******************************************************************************/
#ifndef PB_GENERIC_H
#define PB_GENERIC_H

#include <stdint.h>
#include "pb_decode.h"
#include "pb_encode.h"

/******************************************************************************
    [docexport Pb_pack]
*//**
    @brief Packs a message struct to a buffer of bytes.
    @param[in] buf  Pointer to destination of packed protobuf message buffer.
    @param[in] buflen  Length of the buffer.
    @param[in] src  Pointer to the source struct to pack.
    @param[in] fields  Pointer to the protobuf message fields object.
    @return Returns the number of bytes written to the stream.
******************************************************************************/
uint32_t
Pb_pack(uint8_t *buf, uint32_t buflen, void *src, const void *fields);

/******************************************************************************
    [docexport Pb_unpack]
*//**
    @brief Unpacks a protobuf blob.
    @param[in] buf  Pointer to packed protobuf message buffer.
    @param[in] len  Length of the message.
    @param[in] target  Pointer to the target struct to unpack into.
    @param[in] fields  Pointer to the protobuf message fields object.
    @return Returns true on success; false on failure.
******************************************************************************/
bool
Pb_unpack(uint8_t *buf, uint32_t len, void *target, const void *fields);

/******************************************************************************
    [docexport Pb_pack_delimted]
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
    const void *fields);

/******************************************************************************
    [docexport Pb_unpack_delimited]
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
    const void *fields);
#endif
