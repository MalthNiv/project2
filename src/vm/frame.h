#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include <stdbool.h>
#include <stdlib.h>

#include "threads/synch.h"
#include "threads/thread.h"

struct lock frame_lock UNUSED;
struct list alloc_frames UNUSED;

struct ft_entry
{
  void *kvaddr;
  struct spt_entry *page;
  bool locked;
  struct thread *owner;
  struct list_elem elem;
};

void frame_init(void);
bool frame_alloc(struct thread *cur, struct ft_entry *fte, struct spt_entry *spte, bool lock);
bool frame_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
struct ft_entry *frame_eviction(struct spt_entry *spte);
void free_frame(struct ft_entry *fte);

#endif /* VM/frame.h */