#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <list.h>
#include <stdbool.h>
#include <stdlib.h>
#include <bitmap.h>

#include "devices/block.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/frame.h"
#include "vm/page.h"

#define NUM_SECTORS 8

struct block* swap_space;
struct bitmap* swap_slots;
struct lock swap_lock;

void swap_init(void);
int alloc_swap_slot(void *buffer);
void free_slot(size_t slot, void* addr);

#endif /* VM/swap.h */