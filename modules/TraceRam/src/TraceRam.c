/*******************************************************************************
 *  @file: TraceRam.c
 *  
 *  @brief: Module to manage the TraceRam backend.
*******************************************************************************/
#include <zephyr/logging/log.h>
#include <tracing_core.h>
#include "CircBuffer.h"
#include "TraceRam.h"

LOG_MODULE_REGISTER(TraceRam, CONFIG_TRACERAM_LOG_LEVEL);

/******************************************************************************
    [docimport TraceRam_getState]
*//**
    @brief Gets the traceram state.
******************************************************************************/
bool
TraceRam_getState(void)
{
    return is_tracing_enabled();
}

/******************************************************************************
    [docimport TraceRam_enable]
*//**
    @brief Enables tracing.
******************************************************************************/
void
TraceRam_enable(void)
{
    uint8_t cmd[] = "enable";
    tracing_cmd_handle(cmd, sizeof(cmd));
    if (!is_tracing_enabled())
    {
        LOG_ERR("Tracing was not enabled.");
    }
    LOG_DBG("TraceRam was enabled.");
}

/******************************************************************************
    [docimport TraceRam_disable]
*//**
    @brief Disables tracing.
******************************************************************************/
void
TraceRam_disable(void)
{
    uint8_t cmd[] = "disable";
    tracing_cmd_handle(cmd, sizeof(cmd));
    if (is_tracing_enabled())
    {
        LOG_ERR("Tracing was not disabled.");
    }
    LOG_DBG("TraceRam was disabled.");
}

/******************************************************************************
    [docimport TraceRam_getCount]
*//**
    @brief Gets the current number of bytes in the TraceRam circular buffer.
******************************************************************************/
uint32_t
TraceRam_getCount(void)
{
    return CircBuffer_getCount(&traceram_circ);
}

/******************************************************************************
    [docimport TraceRam_flush]
*//**
    @brief Flushes the TraceRam circular buffer.
******************************************************************************/
void
TraceRam_flush(void)
{
    CircBuffer_flush(&traceram_circ);
}

/******************************************************************************
    [docimport TraceRam_read]
*//**
    @brief Reads a block from the TraceRam circular buffer.
    TraceRam_disable() should be called prior to calls to this function.
    @param[in] buf  Pointer to buffer to hold data.
    @param[in] size  Number of bytes to read.
    @return Returns the number read or negative error code.
******************************************************************************/
int
TraceRam_read(uint8_t *buf, uint32_t size)
{
    int num = CircBuffer_read(&traceram_circ, buf, size);
    LOG_DBG("CircBuffer read %u bytes.", num);
    return num;
}
