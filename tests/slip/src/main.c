#include <stdint.h>
#include <zephyr/ztest.h>
#include "Random.h"
#include "slip.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(slip_tests);

ZTEST_SUITE(slip_tests, NULL, NULL, NULL, NULL, NULL);

ZTEST(slip_tests, test_simple_framer)
{
    uint8_t buf_in[] = {1, 2, 3, 4, 0xdb, 5, 6, 7, 8, 9};
    uint8_t buf_out[2*sizeof(buf_in)];
    uint8_t buf_out_ref[] = {0xc0, 1, 2, 3, 4, 0xdb, 0xdd, 5, 6, 7, 8, 9, 0xc0};
    int size;

    LOG_INF("In test_simple_framer");
    size = slip_framer(buf_in, sizeof(buf_in), buf_out, sizeof(buf_out));
    zassert_true(size > 0, "slip_framer returned %d", size);

    zassert_mem_equal(
        buf_out_ref,
        buf_out,
        size,
        "Failed framing.");

}

#define DEFRAMER_NUM_ITER    1000
#define DEFRAMER_SIZE_MAX    1600
#define DEFRAMER_SIZE_MIN    1
#define DEFRAMER_CHUNK_SIZE_MAX    22
#define DEFRAMER_CHUNK_SIZE_MIN    1

#define MIN(a, b)   (((a) < (b)) ? (a) : (b))

static bool memcheck(void *a, void *ref, uint16_t size)
{
    for (unsigned int i = 0; i < size; i++)
    {
        uint8_t *test = (uint8_t *)a;
        uint8_t *val = (uint8_t *)ref;

        if (test[i] != val[i])
        {
            LOG_INF("memcheck: a[%u]=0x%02x", i-1, test[i-1]);
            LOG_INF("memcheck: a[%u]=0x%02x; expected 0x%02x",
                i, test[i], val[i]);
            LOG_INF("memcheck: a[%u]=0x%02x", i+1, test[i+1]);
            return false;
        }
        
    }
    return true;
}

ZTEST(slip_tests, test_deframer)
{
    slip_deframer_ctx deframer;
    uint8_t buf_in[DEFRAMER_SIZE_MAX];
    uint8_t buf_framed[2*DEFRAMER_SIZE_MAX];
    uint8_t buf_deframed[DEFRAMER_SIZE_MAX];
    int i;
    int ret;

    ret = slip_deframer_init(&deframer, 2*DEFRAMER_CHUNK_SIZE_MAX);
    zassert_equal(ret, 0, "Deframer init failed.");

    for (i = 0; i < DEFRAMER_NUM_ITER; i++)
    {
        int framed_size;
        uint16_t idx = 0;
        uint16_t num_remaining;
        uint16_t ref_size = RANDOM_URANGE(uint16_t,
                                          DEFRAMER_SIZE_MIN,
                                          DEFRAMER_SIZE_MAX);

        /* Generate random buffer contents and frame it. */

        RANDOM_FILL(buf_in, ref_size);
        framed_size = slip_framer(buf_in, ref_size, buf_framed, sizeof(buf_framed));

        zassert_true((framed_size >= (ref_size + 2)),
                "Error in framer, framed size should be at least %u, was %d",
                ref_size + 2, framed_size);

        /* Now deframe it, simulating feeding it random chunks. */
        num_remaining = framed_size;
        while (num_remaining > 0)
        {
            uint16_t deframed_size;
            uint16_t chunk_size = RANDOM_URANGE(
                uint16_t,
                DEFRAMER_CHUNK_SIZE_MIN,
                DEFRAMER_CHUNK_SIZE_MAX);

            /* Clamp chunk_size at num_remaining. */
            chunk_size = MIN(num_remaining, chunk_size);

            deframed_size = slip_deframer(
                &deframer,
                &buf_framed[idx],
                chunk_size,
                buf_deframed,
                sizeof(buf_deframed));

            idx += chunk_size;
            num_remaining -= chunk_size;

            if (num_remaining == 0)
            {
                LOG_DBG("Iter[%4u] ref_size=%4u; framed_size=%4u; "
                    "deframed_size=%4u",
                    i, ref_size, framed_size, deframed_size);

                zassert_equal(
                    deframed_size,
                    ref_size,
                    "Error deframing: Iter[%4u] ref_size=%4u; framed_size=%4u; "
                    "deframed_size=%4u",
                    i, ref_size, framed_size, deframed_size);

                if (!memcheck(buf_deframed, buf_in, ref_size))
                {
                    zassert_true(false, "Failure on memchek: iter %u", i);
                }
            }
            else
            {
                /* Deframer output should be 0. */
                zassert_equal(deframed_size, 0,
                              "Deframer error %d", deframed_size);
            }
        }
    }

}
