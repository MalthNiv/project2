#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"

static void syscall_handler(struct intr_frame *);
void bad_exit(struct intr_frame *f);
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
  if (!f->esp || is_kernel_vaddr(f->esp) || !pagedir_get_page(thread_current()->pagedir, f->esp))
  {
    bad_exit(f);
  }

  int sys_num = *((int *)f->esp);
  // printf("%d", sys_num);

  switch (sys_num)
  {
  case SYS_HALT:
    handle_halt();
    //  printf("leave esp=%p\n",f->esp);
    break;
  case 1:
    handle_exit(f);
    // printf("leave esp=%p\n",f->esp);
    break;
  case 2:
    handle_exec(f);
    //  printf("leave esp=%p\n",f->esp);
    break;
  case 3:
    handle_wait(f);
    // printf("leave esp=%p\n",f->esp);
    break;
  case 4:
    handle_create(f);
    // printf("leave esp=%p\n",f->esp);
    break;
  case 5:
    handle_remove(f);
    // printf("leave esp=%p\n",f->esp);
    break;
  case 6:
    handle_open(f);
    //  printf("leave esp=%p\n",f->esp);
    break;
  case 7:
    handle_filesize(f);
    // printf("leave esp=%p\n",f->esp);
    break;
  case 8:
    handle_read(f);
    //  printf("leave esp=%p\n",f->esp);
    break;
  case 9:
    // printf("I went in here oops");
    handle_write(f);
    break;
  case 10:
    handle_seek(f);
    //  printf("leave esp=%p\n",f->esp);
    break;
  case 11:
    handle_tell(f);
    // printf("leave esp=%p\n",f->esp);
    break;
  case 12:
    handle_close(f);
    // printf("leave esp=%p\n",f->esp);
    break;
  default:
    bad_exit(f);
  }
}

void bad_exit(struct intr_frame *f)
{
  f->eax = -1;
  printf("%s: exit(%d)\n", thread_current()->name, -1);
  thread_exit();
}

void handle_halt(void)
{
  shutdown_power_off();
}

void handle_exit(struct intr_frame *f)
{
  char *esp = (char *)f->esp + 4;
  int status = *((int *)esp);
  struct thread *cur = thread_current();
  if (cur->my_status != NULL)
  {
    cur->my_status->exit_status = status;
  }
  printf("%s: exit(%d)\n", cur->name, status);
  thread_exit();
}

void handle_exec(struct intr_frame *f)
{
  return;
}

void handle_wait(struct intr_frame *f)
{
  char *esp = (char *)f->esp + 4;
  tid_t pid = *((tid_t *)esp);
  f->eax = process_wait(pid);
}

void handle_create(struct intr_frame *f)
{
  char *esp = (char *)f->esp + 4;
  char *file_name = *((char **)esp);
  esp += 4;
  unsigned size = *((unsigned *)esp);
  f->eax = filesys_create(file_name, size);
}

void handle_remove(struct intr_frame *f)
{
  return;
}

void handle_open(struct intr_frame *f)
{
  char *esp = (char *)f->esp + 4;
  char *file_name = *((char **)esp);
  struct file *file = filesys_open(file_name);
  struct thread *cur = thread_current();
  if (!file || cur->open_spot == 128)
  {
    f->eax = -1;
    return;
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
  char *esp = (char *)f->esp + 4;
  int fd = *((int *)esp);
  struct thread *cur = thread_current();

  f->eax = file_length(cur->fd_table[fd - 2]);
}

void handle_read(struct intr_frame *f)
{
  // Get the syscall args
  char *esp = (char *)f->esp + 4;
  int fd = *((int *)esp);
  esp += 4;
  uint8_t *buffer = *((uint8_t **)esp);
  esp += 4;

  unsigned size = *((unsigned *)esp);

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
  char *esp = (char *)f->esp + 4;
  int fd = *((int *)esp);
  esp += 4;

  char *buffer = *((char **)esp);
  esp += 4;
  unsigned size = *((unsigned *)esp);

  // Handle writing to console
  if (fd == 1)
  {
    unsigned original = size;
    while (size > 300) // split into chunks of 300 as per method details
    {
      putbuf(buffer, 300);
      size -= 300;
      buffer += 300;
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
  char *esp = (char *)f->esp + 4;
  int fd = *((int *)esp);
  esp += 4;
  unsigned pos = *((unsigned *)esp);

  struct thread *cur = thread_current();
  file_seek(cur->fd_table[fd - 2], pos);
}

void handle_tell(struct intr_frame *f)
{
  char *esp = (char *)f->esp + 4;
  int fd = *((int *)esp);
  struct thread *cur = thread_current();

  off_t res = file_tell(cur->fd_table[fd - 2]);
  f->eax = res;
}

void handle_close(struct intr_frame *f)
{
  char *esp = (char *)f->esp + 4;
  int fd = *((int *)esp);
  struct thread *cur = thread_current();

  file_close(cur->fd_table[fd - 2]);
  cur->fd_table[fd - 2] = NULL;
  cur->open_spot = fd - 2;
}