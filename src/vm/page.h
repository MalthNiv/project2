#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <list.h>
#include <hash.h>
#include <stdbool.h>
#include <stdlib.h>

#include "threads/synch.h"
#include "threads/thread.h"

struct spt_entry
{
    void *uaddr;
    struct hash_elem hash_elem;

    /* For loading executables */
    bool is_loaded;
    bool writable;
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

void spte_init(struct spt_entry *spte, void *uaddr_, bool w, uint32_t ofs, uint32_t rb, uint32_t zb,
               struct file *file_);
unsigned spt_hash(const struct hash_elem *p_, void *aux UNUSED);
bool spt_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
struct spt_entry *spte_lookup(struct hash *spt, const void *addr);
void free_spte(struct hash_elem *e, void *aux UNUSED);

#endif /* VM/page.h */