#include "swap.h"
#include "vm/frame.h"
#include "vm/page.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <bitmap.h>
#include "devices/block.h"

#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"

struct block *swap_space;
struct bitmap *swap_slots;
struct lock swap_lock;

/* Initializes the swap setup. */
void swap_init(void)
{
    lock_init(&swap_lock);
    swap_space = block_get_role(BLOCK_SWAP);
    block_sector_t size = block_size(swap_space);

    size_t num_slots = size / (NUM_SECTORS);
    swap_slots = bitmap_create(num_slots);
}

/* Gets a swap slot and writes the data from frame to the sectors. */
int alloc_swap_slot(void *addr)
{
    lock_acquire(&swap_lock);
    size_t swap_slot = bitmap_scan_and_flip(swap_slots, 0, 1, false);
    lock_release(&swap_lock);

    if (swap_slot == BITMAP_ERROR)
    {
        return -1;
    }

    block_sector_t start_slot = swap_slot * NUM_SECTORS;
    for (int i = 0; i < NUM_SECTORS; i++)
    {
        void *buf = (char *)addr + (i * BLOCK_SECTOR_SIZE);
        block_write(swap_space, start_slot + i, buf);
    }

    return swap_slot;
}

/* Writes the data from the slot to the frame and frees the slot. */
void free_slot(size_t slot, void *addr)
{
    block_sector_t start_slot = slot * NUM_SECTORS;
    for (int i = 0; i < NUM_SECTORS; i++)
    {
        void *buf = (char *)addr + (i * BLOCK_SECTOR_SIZE);
        block_read(swap_space, start_slot + i, buf);
    }

    lock_acquire(&swap_lock);
    bitmap_reset(swap_slots, slot);
    lock_release(&swap_lock);
}