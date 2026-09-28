/*******************************************************************************
 *  @file: FsApiPb.h
 *
 *  @brief: Reads protobuf blob files into nanopb structs.
 *
 *  A blob file holds one raw protobuf message, with no header. Branding
 *  (fsapi-brand) writes blob files from .pb.yaml files, and fsapi-cli pbput
 *  replaces them on a running device.
*******************************************************************************/
#ifndef FSAPI_PB_H
#define FSAPI_PB_H

/******************************************************************************
    [docexport FsApi_unpack_file]
*//**
    @brief Reads a protobuf blob file and unpacks it into a nanopb struct. The
    function reads the file in small pieces, so the file size has no limit.
    On failure, the target struct can hold part of the data. Clear it before
    a fallback to default values.
    @param[in] path  Absolute path of the blob file.
    @param[out] target  Pointer to the nanopb struct of the message.
    @param[in] fields  The nanopb fields object of the message, for example
      netconf_NetConf_fields.
    @return 0 on success. -ENOENT if the file does not exist. -EBADMSG if the
      file is not a valid message. Another negative errno on a file system
      error.
******************************************************************************/
int
FsApi_unpack_file(const char *path, void *target, const void *fields);

#endif /* FSAPI_PB_H */
