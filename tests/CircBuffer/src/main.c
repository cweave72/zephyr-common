#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include "Random.h"
#include "CircBuffer.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(circbuffer_tests);

#define PRODUCER_THREAD_NAME "producer_thread"
#define CONSUMER_THREAD_NAME "consumer_thread"
static struct k_thread producer_thread;
//static struct k_thread consumer_thread;
static K_THREAD_STACK_DEFINE(producer_thread_stack, 2048);
//static K_THREAD_STACK_DEFINE(consumer_thread_stack, 2048);

#define CIRCBUFFER_DEPTH     1024
#define CIRCBUFFER_MAX_ITEMS 50
#define PRODUCER_RAND_MIN  50
#define PRODUCER_RAND_MAX  128

ZTEST_SUITE(circbuffer_tests, NULL, NULL, NULL, NULL, NULL);

static void
producer_thread_func(void *p1, void *p2, void *p3)
{
    int ret;
    CircBuffer circ;
    uint8_t buf[PRODUCER_RAND_MAX];

    ret = CircBuffer_init(&circ, CIRCBUFFER_DEPTH, NULL, 0, CIRCBUFFER_MAX_ITEMS);
    zassert_equal(ret, 0, "CircBuffer_init error %d", ret);

    while(1)
    {
        uint16_t ref_size = RANDOM_URANGE(uint16_t,
                                          PRODUCER_RAND_MIN,
                                          PRODUCER_RAND_MAX);
        uint16_t sleep_ms = RANDOM_URANGE(uint16_t, 10, 20);

        /* Generate random buffer contents and write it. */
        RANDOM_FILL(buf, ref_size);
        ret = CircBuffer_write(&circ, buf, ref_size);
        zassert_equal(ret, 0, "CircBuffer_write error %d", ret);

        LOG_INF("Sleep %u ms", sleep_ms);
        k_sleep(K_MSEC(sleep_ms));
    }

}

ZTEST(circbuffer_tests, test_circbuffer_writes)
{
    k_tid_t p_tid = k_thread_create(
        &producer_thread,
        producer_thread_stack,
        K_THREAD_STACK_SIZEOF(producer_thread_stack),
        producer_thread_func,
        NULL, NULL, NULL,
        K_LOWEST_APPLICATION_THREAD_PRIO,
        0,
        K_NO_WAIT);
    k_thread_name_set(&producer_thread, PRODUCER_THREAD_NAME);

    LOG_INF("In test_circbuffer_writes");

    k_sleep(K_MSEC(2500));
    k_thread_abort(p_tid);
}

ZTEST(circbuffer_tests, test_item_overflow)
{
    int ret;
    CircBuffer circ;
    uint8_t buf;

    ret = CircBuffer_init(&circ, CIRCBUFFER_DEPTH, NULL, 0, CIRCBUFFER_MAX_ITEMS);
    zassert_equal(ret, 0, "CircBuffer_init error %d", ret);

    for (int i = 0; i < CIRCBUFFER_MAX_ITEMS; i++)
    {
        buf = 0x04;
        ret = CircBuffer_write(&circ, &buf, 1);
        zassert_equal(ret, 0, "write error %d", ret);
    }

    buf = 0x04;
    ret = CircBuffer_write(&circ, &buf, 1);
    zassert_equal(ret, -1, "write error %d", ret);
}

ZTEST(circbuffer_tests, test_read_write)
{
    int ret;
    CircBuffer circ;
    uint8_t circbuf[CircBuffer_getMemAllocSize(CIRCBUFFER_DEPTH)];

    ret = CircBuffer_init(&circ, CIRCBUFFER_DEPTH, circbuf, sizeof(circbuf),
        CIRCBUFFER_MAX_ITEMS);
    zassert_equal(ret, 0, "CircBuffer_init error %d", ret);

    for (uint32_t i = 0; i < CIRCBUFFER_DEPTH; i++)
    {
        uint32_t read_check;
        ret = CircBuffer_write(&circ, (uint8_t *)&i, sizeof(uint32_t));
        zassert_equal(ret, 0, "write error %d", ret);
        ret = CircBuffer_read(&circ, (uint8_t *)&read_check, sizeof(uint32_t));
        LOG_INF("i=%u; read_check=%u", i, read_check);
        zassert_equal(ret, sizeof(uint32_t), "read error %d", ret);
        zassert_equal(i, read_check, "read validation error.");
    }

}
