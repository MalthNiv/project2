#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H


#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/synch.h" //added for teh semaphore
tid_t process_execute(const char* file_name);
int process_wait(tid_t);
void process_exit(void);
void process_activate(void);


/* MACROS */
#define MAX_ARGS_PER_COMMAND 256




//create a struct to track children n their status
struct status_of_child{
 tid_t tid;
 int exit_status;
 struct semaphore parent;
 int count;
 struct list_elem elem;
 struct semaphore loaded;
 bool load;
 struct lock lock_count;
};
/* Global struct: Parsed array of arguments to be passed between process_execute
* and setup_stack. */
typedef struct Command {
 struct status_of_child *child_status;
 char* fn_copy;
 char* file_name;
 char* parsed_array[MAX_ARGS_PER_COMMAND + 1];
 int counter;
} Command;


#endif /* userprog/process.h */



