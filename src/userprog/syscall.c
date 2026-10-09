#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler(struct intr_frame *);

void syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame *f UNUSED)
{
  int sys_num = *((int*)f->esp);
  f->esp = (char*)f->esp + 4;

  switch (sys_num) {
    case 0:
      handle_halt(f);
      break;
    case 1:
      handle_exit(f);
      break;
    case 2:
      handle_exec(f);
      break;
    case 3:
      handle_wait(f);
      break;
    case 4:
      handle_create(f);
      break;
    case 5:
      handle_remove(f);
      break;
    case 6:
      handle_open(f);
      break;
    case 7:
      handle_filesize(f);
      break;
    case 8:
      handle_read(f);
      break;
    case 9:
      handle_write(f);
      break;
    case 10:
      handle_seek(f);
      break;
    case 11:
      handle_tell(f);
      break;
    case 12:
      handle_close(f);
      break;
  }
  
}

void handle_halt(struct intr_frame *f) {
  shutdown_power_off();
}

void handle_exit(struct intr_frame *f) {
  uint32_t status = *((int*)f->esp);
  f->eax = status; 
  thread_exit(); 
}

void handle_exec(struct intr_frame *f) {
  return;
}

void handle_wait(struct intr_frame *f) {
  return;
}

void handle_create(struct intr_frame *f) {
  char* file_name = *((char*)f->esp);
  f->esp = (char*)f->esp + 4;
  uint32_t size = *((int*)f->esp);

  if(!filesys_create(file_name, size)) {
    thread_exit();
  }
}

void handle_remove(struct intr_frame *f) {
  return;
}

void handle_open(struct intr_frame *f) {
  return;
}

void handle_filesize(struct intr_frame *f) {
  return;
}

void handle_read(struct intr_frame *f) {
  return;
}

void handle_write(struct intr_frame *f) {
  return;
}

void handle_seek(struct intr_frame *f) {
  return;
}

void handle_tell(struct intr_frame *f) {
  return;
}

void handle_close(struct intr_frame *f) {
  return;
}