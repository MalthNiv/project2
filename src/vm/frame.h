#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include <stdbool.h>

#include "threads/synch.h"
#include "threads/thread.h"
// #include <page.h>

static struct lock frame_lock;
static struct list alloc_frames;

struct spt_entry {
  bool writeable;
  bool file_backed;
  bool swapped;
  bool bss;
  struct file* file;
  int swap_slot;
  void* uaddr;
};

struct ft_entry {
  void* kvaddr;
  struct spt_entry* page;
  bool locked;
  struct thread* owner;
  struct list_elem elem;
  struct list_elem can_elem;
  int priority;
};

#endif /* VM/frame.h */