#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init(void);

extern int global_fd[128][2];

#endif /* userprog/syscall.h */
