#include <zephyr/logging/log.h>
#include <tracing_backend.h>
#include "CircBuffer.h"

LOG_MODULE_DECLARE(TraceRam, CONFIG_TRACERAM_LOG_LEVEL);

CircBuffer traceram_circ;
static uint8_t circbuf[CircBuffer_getMemAllocSize(CONFIG_TRACERAM_DEPTH)];

static void ram_output(
    const struct tracing_backend *backend,
    uint8_t *data,
    uint32_t length)
{
    int ret;

    (void)backend;

    ret = CircBuffer_write(&traceram_circ, data, length);
    if (ret < 0)
    {
        LOG_ERR("CircBuffer_write returned: %d", ret);
    }
}

static void ram_init(void)
{
    int ret;

    printf("Initializing CircBuffer for TraceRam.\n");
    ret = CircBuffer_init(
        &traceram_circ, CONFIG_TRACERAM_DEPTH, circbuf, sizeof(circbuf), 1000);
    if (ret < 0)
    {
        LOG_ERR("CircBuffer_init returned: %d", ret);
    }
}

const struct tracing_backend_api traceram_api = {
    .init = ram_init,
    .output = ram_output
};

TRACING_BACKEND_DEFINE(tracing_backend_ram, traceram_api);
