#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <hash.h>
#include <bitmap.h>
#include <stdio.h>

#include "page.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "userprog/syscall.h"

/* SPT entry initialization */
struct spt_entry *spte_init(void *uaddr_, bool w, uint32_t ofs, uint32_t rb, uint32_t zb,
                            struct file *file_)
{
    struct spt_entry *spte = malloc(sizeof(struct spt_entry));
    if (!spte)
    {
        return NULL;
    }

    spte->uaddr = uaddr_;

    spte->writable = w;
    spte->page_offset = ofs;
    spte->read_bytes = rb;
    spte->zero_bytes = zb;

    spte->file = file_;
    spte->swap_slot = -1;
    spte->is_loaded = false;
    spte->file_backed = false;
    spte->swapped = false;
    spte->all_zero = false;
    lock_init(&spte->spte_lock);

    return spte;
}

/* Returns a hash value for page p. */
unsigned spt_hash(const struct hash_elem *p_, void *aux UNUSED)
{
    const struct spt_entry *p = hash_entry(p_, struct spt_entry, hash_elem);
    return hash_bytes(&p->uaddr, sizeof p->uaddr);
}

/* Returns true if page a precedes page b. */
bool spt_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
    const struct spt_entry *a = hash_entry(a_, struct spt_entry, hash_elem);
    const struct spt_entry *b = hash_entry(b_, struct spt_entry, hash_elem);

    return a->uaddr < b->uaddr;
}

/* Returns the spte for an address of NULL if there is no corresponding entry. */
struct spt_entry *spte_lookup(struct hash *spt, const void *addr)
{
    struct spt_entry spte;
    struct hash_elem *e;

    spte.uaddr = pg_round_down(addr);
    e = hash_find(spt, &spte.hash_elem);
    return e != NULL ? hash_entry(e, struct spt_entry, hash_elem) : NULL;
}

/* Loads a page of data and attaches a frame to an spte */
struct ft_entry *alloc_and_load(struct spt_entry *spte)
{
    lock_acquire(&spte->spte_lock);
    // Get a frame to map the spte
    struct ft_entry *frame = frame_alloc(thread_current(), spte, true);
    if (!frame)
    {
        lock_release(&spte->spte_lock);
        return NULL;
    }

    // Based on boolean, load in data
    bool swap_from = false;

    if (spte->swapped)
    {
        free_slot(spte->swap_slot, frame->kvaddr);
        spte->swapped = false;
        swap_from = true;
    }
    else if (spte->file_backed && !spte->all_zero)
    {

        bool held = lock_held_by_current_thread(&lock_file);
        if (!held)
        {
            lock_acquire(&lock_file);
        }
        // Load this page
        int bytes_read = file_read_at(spte->file, frame->kvaddr, spte->read_bytes, spte->page_offset);
        if (!held)
        {
            lock_release(&lock_file);
        }
        if (bytes_read != (int)spte->read_bytes)
        {
            free_frame(spte);
            lock_release(&spte->spte_lock);
            return NULL;
        }
        memset((void *)((char *)frame->kvaddr + spte->read_bytes), 0, spte->zero_bytes);
    }
    else if (spte->all_zero)
    {
        memset(frame->kvaddr, 0, PGSIZE);
    }

    // Add the page to the process's address space
    bool set = pagedir_get_page(thread_current()->pagedir, spte->uaddr) == NULL &&
               pagedir_set_page(thread_current()->pagedir, spte->uaddr, frame->kvaddr,
                                spte->writable);

    if (!set)
    {
        free_frame(spte);
        lock_release(&spte->spte_lock);
        return NULL;
    }

    if (swap_from)
    {
        pagedir_set_dirty(thread_current()->pagedir, spte->uaddr, true);
    }
    lock_release(&spte->spte_lock);
    return frame;
}

/* Pinning buffer pages for read() and write() so page faults do not occur during
file_read() and file_write() */
bool pin_buffer_pages(void *buffer, unsigned size, bool w)
{
    void *start_page = pg_round_down(buffer);
    void *end = (void *)((char *)buffer + size - 1);
    void *end_page = pg_round_down(end);

    while (start_page <= end_page)
    {
        struct spt_entry *spte = spte_lookup(&thread_current()->spt, start_page);
        if (!spte)
        {
            return false;
        }

        if (!spte->writable && w)
        {
            return false;
        }

        if (!spte->is_loaded)
        {
            struct ft_entry *frame = alloc_and_load(spte);
            if (!frame)
            {
                return false;
            }

            spte->is_loaded = true;
        }
        else
        {
            pin_frame(spte);
        }

        start_page = (char *)start_page + PGSIZE;
    }

    return true;
}

/* Unpin buffer pages after the read() or write() has completed */
bool unpin_buffer_pages(void *buffer, unsigned size)
{
    void *start_page = pg_round_down(buffer);
    void *end = (void *)((char *)buffer + size - 1);
    void *end_page = pg_round_down(end);

    while (start_page <= end_page)
    {
        struct spt_entry *spte = spte_lookup(&thread_current()->spt, start_page);
        if (!spte)
        {
            return false;
        }

        unpin_frame(spte);
        start_page = (char *)start_page + PGSIZE;
    }

    return true;
}

/* Frees the spte and the frame/swap slot associated with it */
void free_spte(struct hash_elem *e, void *aux UNUSED)
{
    struct spt_entry *spte = hash_entry(e, struct spt_entry, hash_elem);
    lock_acquire(&spte->spte_lock);

    if (spte->is_loaded)
    {
        free_frame(spte);
    }
    else if (spte->swapped)
    {
        lock_acquire(&swap_lock);
        bitmap_reset(swap_slots, spte->swap_slot);
        lock_release(&swap_lock);
    }
    lock_release(&spte->spte_lock);
    free(spte);
}