#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/malloc.h"

tid_t process_execute(const char* file_name);
int process_wait(tid_t);
void process_exit(void);
void process_activate(void);

/* MACROS */
#define MAX_ARGS_PER_COMMAND 256

/* Global struct: Parsed array of arguments to be passed between process_execute
 * and setup_stack. */
typedef struct Command {
  char* fn_copy;
  char* file_name;
  char* parsed_array[MAX_ARGS_PER_COMMAND + 1];
  int counter;
} Command;

#endif /* userprog/process.h */
