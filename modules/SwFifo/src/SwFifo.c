/*******************************************************************************
 *  @file: SwFifo.c
 *  
 *  @brief: A simple software fifo for storing arbitrary objects.
*******************************************************************************/
#include <string.h>
#include <errno.h>
#include <zephyr/logging/log.h>
#include "SwFifo.h"
#include "CheckCond.h"

LOG_MODULE_REGISTER(SwFifo, CONFIG_SWFIFO_LOG_LEVEL);

/** @brief Increments the write index with circular wrap. */
#define inc_wrIdx(pf, n)                \
do {                                    \
    (pf)->wrIdx += (n);                 \
    if (pf->wrIdx > pf->depth) {        \
        pf->wrIdx -= (pf->depth + 1);   \
    }                                   \
} while (0)

/** @brief Increments the read index with circular wrap. */
#define inc_rdIdx(pf, n)                \
do {                                    \
    (pf)->rdIdx += (n);                 \
    if (pf->rdIdx > pf->depth) {        \
        pf->rdIdx -= (pf->depth + 1);   \
    }                                   \
} while (0)

/** @brief Macros to get pointers into memory for write and read. */
#define getMemPtr_wr(f)   ((f)->mem + ((f)->wrIdx * (f)->itemSize))
#define getMemPtr_rd(f)   ((f)->mem + ((f)->rdIdx * (f)->itemSize))

/** @brief Macro for current fifo count (number of items in fifo). */
#define count(pf)                                                 \
    (((pf)->wrIdx >= (pf)->rdIdx) ? ((pf)->wrIdx - (pf)->rdIdx) : \
    (((pf)->depth - (pf)->rdIdx) + (pf)->wrIdx + 1))

/** @brief Macro for checking fifo space available. */
#define avail(pf)        ((pf)->depth - count((pf)))

/** @brief Macro for checkinf fifo empty status. */
#define isEmpty(pf)      (((pf)->wrIdx == (pf)->rdIdx) ? 1 : 0)

/** @brief Macro for checking fifo full status. */
#define isFull(pf)       ((count((pf)) == (pf)->depth) ? 1 : 0)

#ifdef CONFIG_SWFIFO_LOCKING
/******************************************************************************
    lock
*//**
    @brief Takes the fifo lock if the fifo was created threadsafe.

    A spinlock is used rather than a mutex because a fifo may be written from an
    ISR and read from a thread. Mutexes are illegal in an ISR, and skipping the
    mutex when in an ISR would provide no exclusion at all - nothing can block an
    ISR, so a thread holding a mutex does not stop one. On a uniprocessor the
    spinlock reduces to masking interrupts, which is what that case requires.

    @param[in] fifo  Pointer to fifo object.
    @return Returns the key to pass to unlock().
******************************************************************************/
static inline k_spinlock_key_t
lock(SwFifo *fifo)
{
    k_spinlock_key_t key = {0};

    if (fifo->threadsafe)
    {
        key = k_spin_lock(&fifo->lock);
    }
    return key;
}

/******************************************************************************
    unlock
*//**
    @brief Releases the fifo lock if the fifo was created threadsafe.
    @param[in] fifo  Pointer to fifo object.
    @param[in] key  Key returned by the matching lock() call.
******************************************************************************/
static inline void
unlock(SwFifo *fifo, k_spinlock_key_t key)
{
    if (fifo->threadsafe)
    {
        k_spin_unlock(&fifo->lock, key);
    }
}

#else
/*  Locking compiled out. k_spinlock_key_t is still defined by kernel.h, so the
    call sites need no conditionals. */
static inline k_spinlock_key_t
lock(SwFifo *fifo)
{
    k_spinlock_key_t key = {0};

    (void)fifo;
    return key;
}

static inline void
unlock(SwFifo *fifo, k_spinlock_key_t key)
{
    (void)fifo;
    (void)key;
}
#endif

#define LOCAL_MIN(a, b)   (((a)<(b)) ? (a) : (b))

/** @brief Computes the adjusted memory size. */
#define MEM_SIZE(f, num)    ((num)*(f)->itemSize)

/******************************************************************************
    circWrite
*//**
    @brief Writes data to circular array.
******************************************************************************/
static void
circWrite(SwFifo *fifo, void *data, uint32_t num)
{
    uint32_t numToWrap;
    uint8_t *p = getMemPtr_wr(fifo);

    /* Check if the write is expected to wrap. */
    if (fifo->wrIdx + num > fifo->depth)
    {
        /* Number of writes to get to the address just prior to wrap. */
        numToWrap = fifo->depth - fifo->wrIdx + 1;
        /* Write to the last avail memory location prior to wrap. */
        memcpy(p, data, MEM_SIZE(fifo, numToWrap));
        /* Complete the write from the beginning of the circular mem. */
        memcpy(fifo->mem, (uint8_t *)data + numToWrap, MEM_SIZE(fifo, num-numToWrap));
    }
    else
    {
        /* Standard write with no wrap. */
        memcpy(p, data, MEM_SIZE(fifo, num));
    }
}

/******************************************************************************
    circRead
*//**
    @brief Reads data from circular array.
******************************************************************************/
static void
circRead(SwFifo *fifo, void *data, uint32_t num)
{
    uint32_t numToWrap;
    uint8_t *p = getMemPtr_rd(fifo);

    /* Check if the write is expected to wrap. */
    if (fifo->rdIdx + num > fifo->depth)
    {
        /* Number of reads to get to the address just prior to wrap. */
        numToWrap = fifo->depth - fifo->rdIdx + 1;
        /* Read up to the last avail memory location prior to wrap. */
        memcpy(data, p, MEM_SIZE(fifo, numToWrap));
        /* Complete the read from the beginning of the circular mem. */
        memcpy((uint8_t *)data + numToWrap, fifo->mem, MEM_SIZE(fifo, num-numToWrap));
    }
    else
    {
        /* Standard write with no wrap. */
        memcpy(data, p, MEM_SIZE(fifo, num));
    }
}

/******************************************************************************
    [docimport SwFifo_flush]
*//**
    @brief Flushes the fifo.
    @param[in] fifo  Pointer to fifo object.
******************************************************************************/
void
SwFifo_flush(SwFifo *fifo)
{
    k_spinlock_key_t key = lock(fifo);

    fifo->wrIdx = 0;
    fifo->rdIdx = 0;
    unlock(fifo, key);
}

/******************************************************************************
    [docimport SwFifo_isEmtpy]
*//**
    @brief Checks if the fifo is empty.
    @param[in] fifo  Pointer to fifo object.
    @return Returns true if the fifo is empty, false otherwise.
******************************************************************************/
bool
SwFifo_isEmpty(SwFifo *fifo)
{
    k_spinlock_key_t key = lock(fifo);
    int empty = isEmpty(fifo);

    unlock(fifo, key);
    return (empty) ? true : false;
}

/******************************************************************************
    [docimport SwFifo_isFull]
*//**
    @brief Checks if the fifo is full.
    @param[in] fifo  Pointer to fifo object.
    @return Returns true if the fifo is full, false otherwise.
******************************************************************************/
bool
SwFifo_isFull(SwFifo *fifo)
{
    k_spinlock_key_t key = lock(fifo);
    int full = isFull(fifo);

    unlock(fifo, key);
    return (full) ? true : false;
}

/******************************************************************************
    [docimport SwFifo_write]
*//**
    @brief Push a new object into the fifo.
    @param[in] fifo  Pointer to fifo object.
    @param[in] item  Pointer to the item to store.
    @param[in] num  Number of items to write.
    @return Returns 0 on success, -1 on not enough space.
******************************************************************************/
int
SwFifo_write(SwFifo *fifo, void *items, uint32_t num)
{
    LOG_DBG("%s: write enter: avail=%u; wrIdx=%u; rdIdx=%u",
        fifo->name,
        (unsigned int)count(fifo),
        (unsigned int)fifo->wrIdx,
        (unsigned int)fifo->rdIdx);

    k_spinlock_key_t key = lock(fifo);

    if (avail(fifo) < num)
    {
        /* Not enough space in fifo for requested write. */
        unlock(fifo, key);
        return -1;
    }

    /* Now copy the item to memory at the pointer location */
    circWrite(fifo, items, num);

    /* Advance the write index, checking for wrap. */
    inc_wrIdx(fifo, num);

    unlock(fifo, key);

    LOG_DBG("%s: write exit: avail=%u; wrIdx=%u; rdIdx=%u",
        fifo->name,
        (unsigned int)count(fifo),
        (unsigned int)fifo->wrIdx,
        (unsigned int)fifo->rdIdx);

    return 0;
}

/******************************************************************************
    [docimport SwFifo_peek]
*//**
    @brief Get next num items in fifo without removing it.
    @param[in] fifo  Pointer to fifo object.
    @param[in,out] dst  Pointer to destination item memory (must be able to fit
    an item of itemSize). dst will be NULL if fifo is empty.
    @param[in] num  Number of items to read.
    @return Returns the number of items read.
******************************************************************************/
uint32_t
SwFifo_peek(SwFifo *fifo, void *dst, uint32_t num)
{
    k_spinlock_key_t key = lock(fifo);
    /* The count must be sampled under the lock: a concurrent writer can change
       it between the sample and the read. */
    uint32_t numToRead = LOCAL_MIN(num, count(fifo));

    if (numToRead == 0)
    {
        unlock(fifo, key);
        return 0;
    }

    /* Read from circular memory. */
    circRead(fifo, dst, numToRead);

    unlock(fifo, key);
    return numToRead;
}

/******************************************************************************
    [docimport SwFifo_ack]
*//**
    @brief Removes the top num items. This function is typically called after a
    call to SwFifo_peek() when the items are ready to be removed.
    @param[in] fifo  Pointer to fifo object.
    @param[in] num  Number of items to ack.
******************************************************************************/
void
SwFifo_ack(SwFifo *fifo, uint32_t num)
{
    k_spinlock_key_t key = lock(fifo);

    inc_rdIdx(fifo, num);
    unlock(fifo, key);
}

/******************************************************************************
    [docimport SwFifo_read]
*//**
    @brief Reads the item from the fifo (read and remove).
    @param[in] fifo  Pointer to fifo object.
    @param[in,out] dst  Pointer to destination item memory (must be able to fit
    an item of itemSize). dst will be NULL if fifo is empty.
    @param[in] num  Number of items to read.
    @return Returns the number of items read.
******************************************************************************/
uint32_t
SwFifo_read(SwFifo *fifo, void *dst, uint32_t num)
{
    LOG_DBG("%s: read enter: avail=%u; wrIdx=%u; rdIdx=%u",
        fifo->name,
        (unsigned int)count(fifo),
        (unsigned int)fifo->wrIdx,
        (unsigned int)fifo->rdIdx);

    /*  Deliberately not implemented as SwFifo_peek() followed by SwFifo_ack().
        Those take the lock separately, which would leave the read non-atomic:
        a second reader could peek the same items before this one acks them.
        Spinlocks are not recursive, so the lock is taken once here instead. */
    k_spinlock_key_t key = lock(fifo);
    uint32_t numRead = LOCAL_MIN(num, count(fifo));

    if (numRead)
    {
        circRead(fifo, dst, numRead);
        inc_rdIdx(fifo, numRead);
    }
    unlock(fifo, key);

    LOG_DBG("%s: read exit: avail=%u; wrIdx=%u; rdIdx=%u",
        fifo->name,
        (unsigned int)count(fifo),
        (unsigned int)fifo->wrIdx,
        (unsigned int)fifo->rdIdx);

    return numRead;
}

/******************************************************************************
    [docimport SwFifo_getCount]
*//**
    @brief Gets the number of items currently in the fifo.
    @param[in] fifo  Pointer to fifo object.
******************************************************************************/
uint32_t
SwFifo_getCount(SwFifo *fifo)
{
    k_spinlock_key_t key = lock(fifo);
    uint32_t n = count(fifo);

    unlock(fifo, key);
    return n;
}

/******************************************************************************
    [docimport SwFifo_getAvail]
*//**
    @brief Gets the space available in the fifo.
    @param[in] fifo  Pointer to fifo object.
******************************************************************************/
uint32_t
SwFifo_getAvail(SwFifo *fifo)
{
    k_spinlock_key_t key = lock(fifo);
    uint32_t n = avail(fifo);

    unlock(fifo, key);
    return n;
}

/******************************************************************************
    [docimport SwFifo_init]
*//**
    @brief Initializes a software fifo.
    @param[in] fifo  Pointer to uninitialized object.
    @param[in] name  Name for the fifo.
    @param[in] depth  Desired depth of the fifo.
    @param[in] itemSize The sizeof the items to be stored.
    @param[in] mem  Pointer to statically allocated fifo mem. (Use
    SwFifo_getMemAllocSize() macro for proper sizing. Set to NULL to dynamically
    allocate.
    @param[in] memSize  Size of the allocated mem for validation (N/A if mem is
    NULL)
    @param[in] threadsafe  Set flag if fifo must be threadsafe.
    @return Returns 0 on success, -1 on error (out of memory)
******************************************************************************/
int
SwFifo_init(
    SwFifo *fifo,
    char *name,
    uint32_t depth,
    uint32_t itemSize,
    uint8_t *mem,
    uint32_t memSize,
    bool threadsafe)
{
    strncpy(fifo->name, name, sizeof(fifo->name)-1);
    fifo->depth    = depth;
    fifo->itemSize = itemSize;
    fifo->threadsafe = threadsafe;

#ifndef CONFIG_SWFIFO_LOCKING
    CHECK_COND_RETURN_MSG(threadsafe, -ENOTSUP,
        "Threadsafe fifo requested but CONFIG_SWFIFO_LOCKING is not enabled");
#endif

    /* Init the index pointers, */
    fifo->wrIdx = 0;
    fifo->rdIdx = 0;

    if (mem)
    {
        CHECK_COND_RETURN_MSG(memSize != (depth+1)*itemSize, -EINVAL,
            "Invalid statically allocated memory size");
        fifo->mem = mem;
    }
    else
    {
        /*  Allocate the fifo memory. The actual memory allocated is one larger
            than the depth requested to make determining full and empty easy. */
        fifo->mem = (uint8_t *)malloc((depth + 1) * itemSize);
        CHECK_COND_RETURN_MSG(!fifo->mem, -ENOMEM, "Could not allocate fifo memory");
    }

#ifdef CONFIG_SWFIFO_LOCKING
    /*  Zero the lock rather than assume the caller's storage is zeroed - a
        SwFifo may be embedded in a stack or heap object. On a uniprocessor
        without spinlock validation k_spinlock is an empty struct, so this
        compiles to nothing. */
    memset(&fifo->lock, 0, sizeof(fifo->lock));
#endif

    return 0;
}

/******************************************************************************
    [docimport SwFifo_fini]
*//**
    @brief De-inits a fifo. Only use if the SwFifo memory was dynamically
    allocated.
******************************************************************************/
void
SwFifo_fini(SwFifo *fifo)
{
    free(fifo->mem);
}
