/*******************************************************************************
 *  @file: Random.h
 *   
 *  @brief: Wrapper for esp_random utilities.
*******************************************************************************/
#ifndef RANDOM_H
#define RANDOM_H

#include <zephyr/random/random.h>

/** @brief Get a random number in the range 0 to 255. */
#define RANDOM_U8()           sys_rand8_get()

/** @brief Get a random number in the range 0 to 2**16-1. */
#define RANDOM_U16()          sys_rand16_get()

/** @brief Get a random number in the range 0 to 2**32-1. */
#define RANDOM_U32()          sys_rand32_get()

/** @brief Get a random number in the range 0 to n-1 */
#define RANDOM_UINT(type, n)     ((type)(((uint64_t)sys_rand32_get() * ((n)-1)) >> 32))

/** @brief Get a random number between left and right. */
#define RANDOM_U8_RANGE(left, right)                                  \
({                                                                    \
    uint8_t delta = right - left;                                     \
    uint8_t ret = RANDOM_UINT(uint8_t, delta) + left;                 \
    ret;                                                              \
})

/** @brief Get a random number between left and right. */
#define RANDOM_URANGE(type, left, right)                        \
({                                                              \
    type delta = right - left;                                  \
    type ret = RANDOM_UINT(type, delta) + left;                 \
    ret;                                                        \
})

/** @brief Get a random 0 or 1. */
#define RANDOM_BIN()        (RANDOM_U32() & 0x80000000) ? 1 : 0

/** @brief Fill buffer with random bytes. */
#define RANDOM_FILL(buf, size)   sys_rand_get((void *)(buf), (size))

#endif
