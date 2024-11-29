/*******************************************************************************
 *  @file: NvParms.h
 *   
 *  @brief: Header for NvParms.c
*******************************************************************************/
#ifndef NVPARMS_H
#define NVPARMS_H

#define NVPARMS_TYPE_HEX       0
#define NVPARMS_TYPE_STRING    1
#define NVPARMS_TYPE_INVALID   2

/******************************************************************************
    [docexport NvParms_load]
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
NvParms_load(const char *name, uint8_t type, void *dest, size_t len);

/******************************************************************************
    [docexport NvParms_init]
*//**
    @brief Initializes use of NvParms.
******************************************************************************/
int
NvParms_init(void);
#endif
