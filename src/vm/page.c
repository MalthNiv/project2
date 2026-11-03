#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <hash.h>

#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "page.h"

void spte_init(struct spt_entry *spte, void *uaddr_, bool w, uint32_t ofs, uint32_t rb, uint32_t zb,
               struct file *file_)
{
    spte->uaddr = uaddr_;

    spte->writable = w;
    spte->page_offset = ofs;
    spte->read_bytes = rb;
    spte->zero_bytes = zb;

    spte->file = file_;
    spte->swap_slot = 0;
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
struct spt_entry *spte_lookup(struct hash spt, const void *addr)
{
    struct spt_entry spte;
    struct hash_elem *e;

    spte.uaddr = pg_round_down(addr);
    e = hash_find(&spt, &spte.hash_elem);
    return e != NULL ? hash_entry(e, struct spt_entry, hash_elem) : NULL;
}