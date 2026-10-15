#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/synch.h"

void syscall_init(void);

extern int global_fd[128][2];
extern struct lock lock_file;

#endif /* userprog/syscall.h */
