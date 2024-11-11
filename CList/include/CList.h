/*******************************************************************************
 *  @file: CList.h
 *   
 *  @brief: Circular Linked List.
*******************************************************************************/
#ifndef CLIST_H
#define CLIST_H

typedef struct CList CList;

/**
 * struct CList - Entry of a circular double linked list
 * @next:   next entry
 * @prev:   previous entry
 *
 * Each entry in a list must embed a CList object. This object contains
 * pointers to its next and previous elements, which can be freely accessed by
 * the API user at any time. Note that the list is circular, and the list head
 * is linked in the list as well.
 *
 * The list head must be initialized via CLIST_INIT before use. There is no
 * reason to initialize entry objects before linking them. However, if you need
 * a boolean state that tells you whether the entry is linked or not, you should
 * initialize the entry via CLIST_INIT as well.
 */
struct CList {
    CList *next;
    CList *prev;
};

#define CLIST_ANCHOR()\
    CList anchor

/** @brief Initialize the list. */
#define CLIST_INIT(list) { .next = &(list), .prev = &(list) }

/* Test if a list is empty. */
#define CList_isEmpty(plist)   (((plist)->next == (plist)) ? 1 : 0)

/** @brief List iterator.
 *  @iter CList * loop variable
 *  @plist CList * Pointer to the list being iterated over.
*/
#define CLIST_ITER(iter, plist)   \
    for (iter = (plist)->next; (iter) != (plist); iter = (iter)->next)

/** @brief List safe iterator.
 *  @iter CList * loop variable
 *  @safe CList * loop variable which allows the iter to maintain loop
    integrity by tracking the next item, allowing the current item to be
    modified.
 *  @plist CList * Pointer to the list being iterated over.
*/
#define CLIST_ITER_SAFE(iter, safe, plist)               \
    for (iter = (plist)->next, safe = (iter->next);      \
        (iter) != (plist);                               \
        iter = (safe), safe = (safe)->next)


/** @brief Flush an entire list. */
#define CLIST_FLUSH(iter, plist)                     \
do {                                                 \
    CList *safe;                                     \
    for (iter = (plist)->next, safe = (iter->next);  \
        CList_init(iter) != (plist);                 \
        iter = safe, safe = safe->next) {}           \
} while (0)

/** @brief Get the container entry from the list anchor. */
#define CList_entry(entry, entry_t)     ((entry_t *)((void *)entry))

/** @brief List iterate over entries.
 *  @entry Pointer to list entry.
 *  @plist Pointer to list
*/
#define CLIST_ITER_ENTRY(entry, plist)                                   \
    for (entry = CList_entry((plist)->next, __typeof__(*entry));          \
         &(entry)->anchor != (plist);                                     \
         entry = CList_entry((entry)->anchor.next, __typeof__(*entry)))

/******************************************************************************
    CList_init
*//**
    @brief Initialize a list.

    @param[in] list  Pointer to the list to initialize.
    @return Returns the initialized list.
******************************************************************************/
static inline CList *
CList_init(CList *list)
{
    *list = (CList)CLIST_INIT(*list);
    return list;
}


/******************************************************************************
    CList_insertBefore
*//**
    @brief Inserts an item into a list before a given item.

    @param[in] target  Pointer to the target item to insert before.
    @param[in] new  Pointer to new item to insert.
******************************************************************************/
static inline void
CList_insertBefore(CList *target, CList *new)
{
    CList *prev = target->prev;

    prev->next = new;
    new->prev = prev;
    new->next = target;
    target->prev = new;
}

/******************************************************************************
    CList_insertAfter
*//**
    @brief Inserts an item into a list after a given item.

    @param[in] target  Pointer to the target item to insert after.
    @param[in] new  Pointer to new item to insert.
******************************************************************************/
static inline void
CList_insertAfter(CList *target, CList *new)
{
    CList *next = target->next;

    target->next = new;
    new->prev = target;
    new->next = next;
    next->prev = new;
}

/******************************************************************************
    CList_remove
*//**
    @brief Removes the item from the list. Does not impact the target object, it
    must be dealt with (i.e. freed) by the caller.

    @param[in] target  Pointer to the target item to remove.
******************************************************************************/
static inline void
CList_remove(CList *target)
{
    CList *prev = target->prev;
    CList *next = target->next;

    prev->next = next;
    next->prev = prev;
}

/******************************************************************************
    CList_prepend
*//**
    @brief Prepends item to the beginning of a list.

    @param[in] plist  Pointer to list to prepend to.
    @param[in] new  Pointer to new item to add.
******************************************************************************/
#define CList_prepend(plist, pnew)\
    CList_insertAfter((plist), (pnew))

/******************************************************************************
    CList_append
*//**
    @brief Appends item to the end of a list.

    @param[in] plist  Pointer to list to prepend to.
    @param[in] pnew  Pointer to new item to add.
******************************************************************************/
#define CList_append(plist, pnew)\
    CList_insertBefore((plist), (CList *)(pnew))

/******************************************************************************
    CList_head
*//**
    @brief Returns a pointer to the list head.
    @param[in] plist  Pointer to list object.
******************************************************************************/
#define CList_head(plist)\
    (CList_isEmpty((plist)) ? NULL : (plist)->next)

/******************************************************************************
    CList_tail
*//**
    @brief Returns a pointer to the list tail.
    @param[in] plist  Pointer to list object.
******************************************************************************/
#define CList_tail(plist)\
    (CList_isEmpty((plist)) ? NULL : (plist)->prev)
#endif
