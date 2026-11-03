#include "swap.h"
#include "vm/frame.h"
#include "vm/page.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "devices/block.h"

#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"

void swap_init(void);
int alloc_swap_slot(const void *buffer);
void free_slot(int slot);

void swap_init(void)
{
    list_init(&open_spots);

    for (int i = 0; i < 17; i++)
    {
        struct open_spot *os;
        os->open = i;

        list_push_back(&open_spots, &os->elem);
    }

    for (int i = 0; i < 17; i++)
    {
        swap_space[i] = NULL;
    }
}

int alloc_swap_slot(const void *buffer)
{
    int swap_slot;

    if (list_empty(&open_spots))
    {
        return -1;
    }
    else
    {
        swap_slot = list_entry(list_pop_front(&open_spots), struct open_spot, elem)->open;
    }

    if (!swap_space[swap_slot])
    {
        struct block *b = block_get_role(BLOCK_SWAP);
        swap_space[swap_slot] = b;
    }

    block_write(swap_space[swap_slot], swap_space[swap_slot]->size, buffer);
    return swap_slot;
}

void free_slot(int slot)
{
    struct open_spot *os;
    os->open = slot;

    list_push_back(&open_spots, &os->elem);
}