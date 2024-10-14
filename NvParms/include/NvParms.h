/*******************************************************************************
 *  @file: NvParms.h
 *   
 *  @brief: Header for NvParms.c
*******************************************************************************/
#ifndef NVPARMS_H
#define NVPARMS_H


typedef enum NvParms_type
{
    NVPARMS_TYPE_BIN = 0,
    NVPARMS_TYPE_STRING,
    NVPARMS_TYPE_INVALID
} NvParms_type;

/******************************************************************************
    [docexport NvParams_load]
*//**
    @brief Description.
    @param[in] name  Name of key to retrieve.
    @param[in] type  Type of the data to be retrieved.
    @param[in] dest  Destination to write value.
    @param[in] len  Destination length.
    @return Return the size of the loaded value on success, negative error on
    failure.
******************************************************************************/
int
NvParams_load(const char *name, NvParms_type type, void *dest, size_t len);

/******************************************************************************
    [docexport NvParms_init]
*//**
    @brief Initializes use of NvParms.
******************************************************************************/
int
NvParms_init(void);
#endif
