/*******************************************************************************
 *  @file: CheckErr.h
 *   
 *  @brief: Macros for error handling.
*******************************************************************************/
#ifndef CHECKCOND_H
#define CHECKCOND_H

#include <stdlib.h>
#include <zephyr/logging/log.h>

/** @brief Macro which checks a condition a returns 'ret' if it evaluates to true.
    @param[in] cond  The condition to test.
    @param[in] ret  What to return.
*/
#define CHECK_COND_RETURN(cond, ret)       \
do {                                       \
    if ((cond)) {                          \
        return (ret);                      \
    }                                      \
} while (0)

/** @brief Macro which checks a condition a returns if it evaluates to true.
    @param[in] cond  The condition to test.
*/
#define CHECK_COND_VOID_RETURN(cond)       \
do {                                       \
    if ((cond)) {                          \
        return;                            \
    }                                      \
} while (0)

/** @brief Macro which checks a condition, prints a msg and returns 'ret' if the
      condition is true.
    @param[in] cond  The condition to check.
    @param[in] msg  A message string to print.
    @param[in] ret  What to return.
*/
#define CHECK_COND_RETURN_MSG(cond, ret, msg)      \
do {                                               \
    if ((cond)) {                                  \
        LOG_ERR("%s: " #cond, msg);         \
        return (ret);                              \
    }                                              \
} while (0)

/** @brief Macro which checks a condition, prints a msg and returns if the
      condition is true.
    @param[in] cond  The condition to check.
    @param[in] msg  A message string to print.
*/
#define CHECK_COND_VOID_RETURN_MSG(cond, msg)      \
do {                                               \
    if ((cond)) {                                  \
        LOG_ERR("%s: " #cond, msg);         \
        return;                                    \
    }                                              \
} while (0)

/** @brief Macro which checks a condition and asserts if the
      condition is true.
    @param[in] cond  The condition asserted to be true.
    @param[in] msg  A message string to print.
*/
#define CHECK_COND_ASSERT(cond)                      \
do {                                                 \
    if ((cond)) {                                    \
        LOG_ERR("ASSERT FAILURE: " #cond);    \
        assert(0);                                   \
    }                                                \
} while (0)

/** @brief Macro which checks a condition and asserts if the
      condition is true.
    @param[in] cond  The condition asserted to be true.
    @param[in] msg  A message string to print.
*/
#define CHECK_COND_ASSERT_MSG(cond, msg)                          \
do {                                                              \
    if ((cond)) {                                                 \
        LOG_ERR("ASSERT FAILURE: %s (" #cond ")", msg);    \
        assert(0);                                                \
    }                                                             \
} while (0)

/** @brief Macro which checks a condition and jumpt to goto label if it evaluates to true.
    @param[in] cond  The condition to test.
    @param[in] label  goto label
*/
#define CHECK_COND_GOTO(cond, label)     \
do {                                       \
    if ((cond)) {                          \
        goto (label);                      \
    }                                      \
} while (0)

/** @brief Macro which checks a condition, prints a msg and jumpt to goto label
      if condition is true.
    @param[in] cond  The condition to check.
    @param[in] msg  A message string to print.
    @param[in] label  goto label
*/
#define CHECK_COND_GOTO_MSG(cond, ret, msg)        \
do {                                               \
    if ((cond)) {                                  \
        LOG_ERR("%s: " #cond, msg);         \
        goto (label);                              \
    }                                              \
} while (0)

#endif
