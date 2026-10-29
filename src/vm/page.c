#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <hash.h>
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "page.h"

/* Returns a hash value for page p. */
unsigned spt_hash(const struct hash_elem *p_, void *aux UNUSED)
{
    const struct spt_entry *p = hash_entry(p_, struct spt_entry, hash_elem);
    return hash_bytes(&p->uaddr, sizeof p->uaddr);
}

/* Returns true if page a precedes page b. */
bool spt_less(const struct hash_elem *a_, const struct hash_elem *b_,
               void *aux UNUSED)
{
    const struct spt_entry *a = hash_entry(a_, struct spt_entry, hash_elem);
    const struct spt_entry *b = hash_entry(b_, struct spt_entry, hash_elem);

    return a->uaddr < b->uaddr;
}