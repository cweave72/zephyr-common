/*******************************************************************************
 *  @file: TraceRam.h
 *   
 *  @brief: Header for TraceRam module.
*******************************************************************************/
#ifndef TRACERAM_H
#define TRACERAM_H

#include "CircBuffer.h"

/** @brief Exported global */
extern CircBuffer traceram_circ;



/******************************************************************************
    [docexport TraceRam_getState]
*//**
    @brief Gets the traceram state.
******************************************************************************/
bool
TraceRam_getState(void);

/******************************************************************************
    [docexport TraceRam_enable]
*//**
    @brief Enables tracing.
******************************************************************************/
void
TraceRam_enable(void);

/******************************************************************************
    [docexport TraceRam_disable]
*//**
    @brief Disables tracing.
******************************************************************************/
void
TraceRam_disable(void);

/******************************************************************************
    [docexport TraceRam_getCount]
*//**
    @brief Gets the current number of bytes in the TraceRam circular buffer.
******************************************************************************/
uint32_t
TraceRam_getCount(void);

/******************************************************************************
    [docexport TraceRam_flush]
*//**
    @brief Flushes the TraceRam circular buffer.
******************************************************************************/
void
TraceRam_flush(void);

/******************************************************************************
    [docexport TraceRam_read]
*//**
    @brief Reads a block from the TraceRam circular buffer.
    TraceRam_disable() should be called prior to calls to this function.
    @param[in] buf  Pointer to buffer to hold data.
    @param[in] size  Number of bytes to read.
    @return Returns the number read or negative error code.
******************************************************************************/
int
TraceRam_read(uint8_t *buf, uint32_t size);
#endif
