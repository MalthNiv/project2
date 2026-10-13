#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

static void syscall_handler(struct intr_frame *);
void handle_halt(void);
void handle_exit(struct intr_frame *f);
void handle_exec(struct intr_frame *f);
void handle_wait(struct intr_frame *f);
void handle_create(struct intr_frame *f);
void handle_remove(struct intr_frame *f);
void handle_open(struct intr_frame *f);
void handle_filesize(struct intr_frame *f);
void handle_read(struct intr_frame *f);
void handle_write(struct intr_frame *f);
void handle_seek(struct intr_frame *f);
void handle_tell(struct intr_frame *f);
void handle_close(struct intr_frame *f);

void syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame *f UNUSED)
{
  int sys_num = *((int *)f->esp);
  f->esp = (char *)f->esp + 4;

  switch (sys_num)
  {
  case 0:
    handle_halt();
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

void handle_halt(void)
{
  shutdown_power_off();
}

void handle_exit(struct intr_frame *f)
{
  uint32_t status = *((int *)f->esp);
  f->eax = status;
  thread_exit();
}

void handle_exec(struct intr_frame *f)
{
  return;
}

void handle_wait(struct intr_frame *f)
{
  return;
}

void handle_create(struct intr_frame *f)
{
  char *file_name = ((char *)f->esp);
  f->esp = (char *)f->esp + 4;
  uint32_t size = *((int *)f->esp);

  if (!filesys_create(file_name, size))
  {
    thread_exit();
  }
}

void handle_remove(struct intr_frame *f)
{
  return;
}

void handle_open(struct intr_frame *f)
{
  char *file_name = ((char *)f->esp);

  struct file *file = filesys_open(file_name);
  struct thread *cur = thread_current();
  if (!file || cur->open_spot == 128)
  {
    f->eax = -1;
  }
  else
  {
    cur->fd_table[cur->open_spot] = file;
    f->eax = cur->open_spot + 2;

    if (cur->open_spot == cur->open_end)
    {
      cur->open_end++;
    }
    cur->open_spot = cur->open_end;
  }
}

void handle_filesize(struct intr_frame *f)
{
  int fd = *((int *)f->esp);
  struct thread *cur = thread_current();

  f->eax = file_length(cur->fd_table[fd - 2]);
}

void handle_read(struct intr_frame *f)
{
  // Get the syscall args
  int fd = *((int *)f->esp);
  f->esp = (char *)f->esp + 4;

  uint8_t *buffer = ((uint8_t *)f->esp);
  f->esp = (char *)f->esp + 4;

  unsigned size = *((int32_t *)f->esp);

  // Reading input
  if (fd == 0)
  {
    for (unsigned i = 0; i < size; i++)
    {
      buffer[i] = input_getc();
    }
    f->eax = size;
  }
  else
  {
    struct thread *cur = thread_current();
    f->eax = file_read(cur->fd_table[fd - 2], buffer, size);
  }
}

void handle_write(struct intr_frame *f)
{
  // Get the syscall args
  int fd = *((int *)f->esp);
  f->esp = (char *)f->esp + 4;

  void *buffer = ((void *)f->esp);
  f->esp = (char *)f->esp + 4;

  unsigned size = *((int32_t *)f->esp);

  // Handle writing to console
  if (fd == 1)
  {
    unsigned original = size;
    while (size > 300) // split into chunks of 300 as per method details
    {
      putbuf((char *)buffer, 300);
      size -= 300;
      buffer = (char *)buffer + 300;
    }

    putbuf((char *)buffer, size);
    f->eax = original;
  }
  else
  {
    struct thread *cur = thread_current();
    f->eax = file_write(cur->fd_table[fd - 2], buffer, size);
  }
}

void handle_seek(struct intr_frame *f)
{
  int fd = *((int *)f->esp);
  f->esp = (char *)f->esp + 4;
  unsigned pos = *((unsigned *)f->esp);

  struct thread *cur = thread_current();
  file_seek(cur->fd_table[fd - 2], pos);
}

void handle_tell(struct intr_frame *f)
{
  int fd = *((int *)f->esp);
  struct thread *cur = thread_current();

  off_t res = file_tell(cur->fd_table[fd - 2]);
  f->eax = res;
}

void handle_close(struct intr_frame *f)
{
  int fd = *((int *)f->esp);
  struct thread *cur = thread_current();

  file_close(cur->fd_table[fd - 2]);
  cur->fd_table[fd - 2] = NULL;
  cur->open_spot = fd - 2;
}