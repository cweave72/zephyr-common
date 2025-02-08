/*******************************************************************************
 *  @file: NvParms.c
 *  
 *  @brief: Wrapper for getting/setting nonvolatile parameters.
 *  Based on zephyr/subsys/settings/src/settings.shell.c
 *    cmd_read()
*******************************************************************************/
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include "NvParms.h"

/** @brief Initialize the logging module. */
LOG_MODULE_REGISTER(NvParms, CONFIG_NVPARMS_LOG_LEVEL);

enum value_types {
    VALUE_HEX = 0,
    VALUE_STRING,
};

struct read_callback_params {
    void *dest;
    ssize_t dest_max_size;
    const enum value_types value_type;
    bool value_found;
    ssize_t read_size;
};

static int
settings_read_callback(
    const char *key,
    size_t len,
    settings_read_cb read_cb,
    void *cb_arg,
    void *param)
{
    struct read_callback_params *params = param;
    ssize_t num_read_bytes = MIN(len, params->dest_max_size);
    uint8_t *buffer = (uint8_t *)params->dest;

    /* Process only the exact match and ignore descendants of the searched name */
    if (settings_name_next(key, NULL) != 0)
    {
        return 0;
    }

    params->value_found = false;
    params->read_size = 0;

    num_read_bytes = read_cb(cb_arg, buffer, num_read_bytes);
    LOG_DBG("num_read_bytes = %d", num_read_bytes);

    if (num_read_bytes < 0)
    {
        LOG_ERR("Failed to read value: %d", (int) num_read_bytes);
        return num_read_bytes;
    }

    if (num_read_bytes == 0)
    {
        LOG_WRN("Value is empty");
        return 0;
    }

    if (params->value_type == NVPARMS_TYPE_STRING)
    {
        if (buffer[num_read_bytes-1] != '\0')
        {
            LOG_ERR("Invalid string read.");
            return -EINVAL;
        }
    }

    params->value_found = true;
    params->read_size = num_read_bytes;
    return 0;
}

/******************************************************************************
    [docimport NvParms_load]
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
NvParms_load(const char *name, uint8_t type, void *dest, size_t len)
{
    int rc;

    struct read_callback_params params = {
        .dest          = dest,
        .dest_max_size = len,
        .value_type    = type
    };

    if (type >= NVPARMS_TYPE_INVALID)
    {
        LOG_ERR("Invalid type for %s: %u.", name, type);
        return -EINVAL;
    }

    rc = settings_load_subtree_direct(name, settings_read_callback, &params);
    if (rc < 0 || params.read_size == 0)
    {
        LOG_ERR("Error retrieving %s: %d (read_size=%u)",
            name, rc, params.read_size);
        return -1;
    }

    return params.read_size;
}

/******************************************************************************
    [docimport NvParms_init]
*//**
    @brief Initializes use of NvParms.
******************************************************************************/
int
NvParms_init(void)
{
    int rc = settings_subsys_init();
    if (rc)
    {
        LOG_ERR("Settings init error : %d", rc);
    }
    return rc;
}
