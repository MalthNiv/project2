#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/thread.h"
#include "threads/synch.h" 
#include <stdbool.h>
#include <list.h>
//#include <page.h>

struct spt_entry{
    bool writeable;
    bool file_backed;
    bool seapped;
    bool bss;
    struct file* file;
    int swap_slot;
    void *uaddr;
}; 

struct ft_entry {
    void* kvaddr;
    struct sp_entry* page;
    bool locked;
    struct thread* owner;
    struct list_elem elm; 
    int priority; 
};

static struct list alloc_frames;


#endif /* VM/frame.h */