#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <list.h>
#include <stdbool.h>
#include <stdlib.h>

#include "devices/block.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "vm/frame.h"
#include "vm/page.h"

struct block *swap_space[17] UNUSED; // FIX IDIOTS 10:24 pm
struct list open_spots;

struct open_spot {
    int open;
    struct list_elem elem;
};

void swap_init(void);
int alloc_swap_slot(const void *buffer);
void free_slot(int slot);

#endif /* VM/swap.h */