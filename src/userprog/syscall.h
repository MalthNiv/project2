#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/synch.h"
#include "threads/interrupt.h"

void syscall_init(void);
void bad_exit(struct intr_frame* f);


extern struct lock lock_file;

#endif /* userprog/syscall.h */
