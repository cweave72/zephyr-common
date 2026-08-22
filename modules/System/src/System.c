/*******************************************************************************
 *  @file: System.c
 *
 *  @brief: System-level functions.
*******************************************************************************/
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(System, CONFIG_SYSTEM_LOG_LEVEL);


/******************************************************************************
    [docimport System_init]
*//**
    @brief Initializes the system library.
******************************************************************************/
void
System_init(void)
{
    LOG_INF("Initializing System library.");
}
