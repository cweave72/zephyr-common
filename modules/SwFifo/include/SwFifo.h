/*******************************************************************************
 *  @file: SwFifo.h
 *   
 *  @brief: A simple software fifo component.
*******************************************************************************/
#ifndef SWFIFO_H
#define SWFIFO_H

#include <stdint.h>
#include <stdbool.h>
//#include "RtosUtils.h"

/** @brief Software fifo object.
*/
typedef struct SwFifo
{
    /** @brief Name for the fifo. */
    char name[16];
    /** @brief Depth */
    uint32_t depth;
    /** @brief Item size. */
    uint32_t itemSize;
    /** @brief Current write index. */
    uint32_t wrIdx;
    /** @brief Current read index. */
    uint32_t rdIdx;
    /** @brief flag indicating fifo should be threadsafe. */
    bool threadsafe;
    /** @brief Lock mutex */
    //RTOS_MUTEX lock;
    /** @brief Memory for the fifo (allocated on init). */
    uint8_t *mem;
} SwFifo;

/** @brief Macro for obtaining the proper memory allocation size for a SwFifo
      when statically allocating memory.
    Ex: static uint8_t fifomem[SWFIFO_MEMSIZE(15, sizeof(int))];
  */
#define SwFifo_getMemAllocSize(depth, itemsz)   (((depth)+1)*(itemsz))

/******************************************************************************
    [docexport SwFifo_flush]
*//**
    @brief Flushes the fifo.
    @param[in] fifo  Pointer to fifo object.
******************************************************************************/
void
SwFifo_flush(SwFifo *fifo);

/******************************************************************************
    [docexport SwFifo_isEmtpy]
*//**
    @brief Checks if the fifo is empty.
    @param[in] fifo  Pointer to fifo object.
    @return Returns true if the fifo is empty, false otherwise.
******************************************************************************/
bool
SwFifo_isEmpty(SwFifo *fifo);

/******************************************************************************
    [docexport SwFifo_isFull]
*//**
    @brief Checks if the fifo is full.
    @param[in] fifo  Pointer to fifo object.
    @return Returns true if the fifo is full, false otherwise.
******************************************************************************/
bool
SwFifo_isFull(SwFifo *fifo);

/******************************************************************************
    [docexport SwFifo_write]
*//**
    @brief Push a new object into the fifo.
    @param[in] fifo  Pointer to fifo object.
    @param[in] item  Pointer to the item to store.
    @param[in] num  Number of items to write.
    @return Returns 0 on success, -1 on not enough space.
******************************************************************************/
int
SwFifo_write(SwFifo *fifo, void *items, uint32_t num);

/******************************************************************************
    [docexport SwFifo_peek]
*//**
    @brief Get next num items in fifo without removing it.
    @param[in] fifo  Pointer to fifo object.
    @param[in,out] dst  Pointer to destination item memory (must be able to fit
    an item of itemSize). dst will be NULL if fifo is empty.
    @param[in] num  Number of items to read.
    @return Returns the number of items read.
******************************************************************************/
uint32_t
SwFifo_peek(SwFifo *fifo, void *dst, uint32_t num);

/******************************************************************************
    [docexport SwFifo_ack]
*//**
    @brief Removes the top num items. This function is typically called after a
    call to SwFifo_peek() when the items are ready to be removed.
    @param[in] fifo  Pointer to fifo object.
    @param[in] num  Number of items to ack.
******************************************************************************/
void
SwFifo_ack(SwFifo *fifo, uint32_t num);

/******************************************************************************
    [docexport SwFifo_read]
*//**
    @brief Reads the item from the fifo (read and remove).
    @param[in] fifo  Pointer to fifo object.
    @param[in,out] dst  Pointer to destination item memory (must be able to fit
    an item of itemSize). dst will be NULL if fifo is empty.
    @param[in] num  Number of items to read.
    @return Returns the number of items read.
******************************************************************************/
uint32_t
SwFifo_read(SwFifo *fifo, void *dst, uint32_t num);

/******************************************************************************
    [docexport SwFifo_getCount]
*//**
    @brief Gets the number of items currently in the fifo.
    @param[in] fifo  Pointer to fifo object.
******************************************************************************/
uint32_t
SwFifo_getCount(SwFifo *fifo);

/******************************************************************************
    [docexport SwFifo_getAvail]
*//**
    @brief Gets the space available in the fifo.
    @param[in] fifo  Pointer to fifo object.
******************************************************************************/
uint32_t
SwFifo_getAvail(SwFifo *fifo);

/******************************************************************************
    [docexport SwFifo_init]
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
    bool threadsafe);

/******************************************************************************
    [docexport SwFifo_fini]
*//**
    @brief De-inits a fifo. Only use if the SwFifo memory was dynamically
    allocated.
******************************************************************************/
void
SwFifo_fini(SwFifo *fifo);
#endif
