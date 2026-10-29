#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <list.h>
#include <hash.h>
#include <stdbool.h>
#include "threads/synch.h"
#include "threads/thread.h"

struct spt_entry
{
    void *uaddr;
    struct hash_elem hash_elem;
    
    /* For loading executables */
    bool writeable;
    uint32_t file_page;
    uint32_t mem_page;
    uint32_t page_offset;
    uint32_t read_bytes;
    uint32_t zero_bytes;
    
    /* Determining location */
    bool file_backed;
    bool swapped;
    bool all_zero;

    /* Location of file data*/
    struct file *file;
    int swap_slot;
};

unsigned spt_hash(const struct hash_elem *p_, void *aux UNUSED);
bool spt_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);

#endif /* VM/page.h */