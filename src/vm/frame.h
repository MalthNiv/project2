#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include <stdbool.h>

#include "threads/synch.h"
#include "threads/thread.h"
// #include <page.h>

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
  struct sp_entry* page;
  bool locked;
  struct thread* owner;
  struct list_elem elem;
  int priority;
};

static struct list alloc_frames;

#endif /* VM/frame.h */