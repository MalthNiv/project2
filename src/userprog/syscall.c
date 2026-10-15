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
static struct lock *file_lock;

bool verify_address(uint32_t *pd, const void *addr);
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
  file_lock = malloc(sizeof(struct lock));
  lock_init(file_lock);
}

static void syscall_handler(struct intr_frame *f UNUSED)
{
  int sys_num = *((int *)f->esp);
  f->esp = (char *)f->esp + 4;

  // printf("enter esp=%p\n", f->esp);

  switch (sys_num)
  {
  case 0:
    handle_halt();
    // printf("leave esp=%p\n", f->esp);
    break;
  case 1:
    handle_exit(f);
    // printf("leave esp=%p\n", f->esp);
    break;
  case 2:
    handle_exec(f);
    // printf("leave esp=%p\n", f->esp);
    break;
  case 3:
    handle_wait(f);
    // printf("leave esp=%p\n", f->esp);
    break;
  case 4:
    handle_create(f);
    // printf("leave esp=%p\n", f->esp);
    break;
  case 5:
    handle_remove(f);
    // printf("leave esp=%p\n", f->esp);
    break;
  case 6:
    handle_open(f);
    // printf("leave esp=%p\n", f->esp);
    break;
  case 7:
    handle_filesize(f);
    // printf("leave esp=%p\n", f->esp);
    break;
  case 8:
    handle_read(f);
    // printf("leave esp=%p\n", f->esp);
    break;
  case 9:
    handle_write(f);
    // printf("leave esp=%p\n", f->esp);
    break;
  case 10:
    handle_seek(f);
    // printf("leave esp=%p\n", f->esp);
    break;
  case 11:
    handle_tell(f);
    // printf("leave esp=%p\n", f->esp);
    break;
  case 12:
    handle_close(f);
    // printf("leave esp=%p\n", f->esp);
    break;
  }
}

bool verify_address(uint32_t *pd, const void *addr)
{
  if (!addr || is_kernel_vaddr(addr) || !pagedir_get_page(pd, addr))
  {
    return false;
  }
  return true;
}

void handle_halt(void)
{
  shutdown_power_off();
}

void handle_exit(struct intr_frame *f)
{
  int status = *((int *)f->esp);
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
  char *cmd_line = ((char *)f->esp);
  if (!verify_address(thread_current()->pagedir, cmd_line))
  {
    thread_exit();
    NOT_REACHED();
  }

  lock_acquire(file_lock);
  f->eax = process_execute(cmd_line);
  lock_release(file_lock);
}

void handle_wait(struct intr_frame *f)
{
  tid_t pid = *((tid_t *)f->esp);
  f->eax = process_wait(pid);
}

void handle_create(struct intr_frame *f)
{
  void *original = f->esp;

  char *file_name = *((char **)f->esp);
  f->esp = (char *)f->esp + 4;
  uint32_t size = *((int *)f->esp);

  f->esp = (char *)f->esp - 8;

  if (!verify_address(thread_current()->pagedir, file_name) || !filesys_create(file_name, size))
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
  char *file_name = *((char **)f->esp);
  struct thread *cur = thread_current();

  if (!verify_address(cur->pagedir, file_name))
  {
    thread_exit();
    NOT_REACHED();
  }

  lock_acquire(file_lock);
  struct file *file = filesys_open(file_name);
  lock_release(file_lock);
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

  lock_acquire(file_lock);
  f->eax = file_length(cur->fd_table[fd - 2]);
  lock_release(file_lock);
}

void handle_read(struct intr_frame *f)
{
  void *original = f->esp;

  // Get the syscall args
  int fd = *((int *)f->esp);
  f->esp = (char *)f->esp + 4;

  uint8_t *buffer = *((uint8_t **)f->esp);
  f->esp = (char *)f->esp + 4;

  unsigned size = *((int32_t *)f->esp);
  f->esp = (char *)f->esp - 12;

  if (!verify_address(thread_current()->pagedir, buffer))
  {
    thread_exit();
    NOT_REACHED();
  }

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
    lock_acquire(file_lock);
    f->eax = file_read(cur->fd_table[fd - 2], buffer, size);
    lock_release(file_lock);
  }
}

void handle_write(struct intr_frame *f)
{
  // void *original = f->esp;

  // Get the syscall args
  int fd = *((int *)f->esp);
  f->esp = (char *)f->esp + 4;

  char *buffer = *((char **)f->esp);
  f->esp = (char *)f->esp + 4;

  unsigned size = *((int32_t *)f->esp);
  f->esp = (char *)f->esp - 12;

  if (!verify_address(thread_current()->pagedir, buffer))
  {
    thread_exit();
    NOT_REACHED();
  }

  // Handle writing to console
  if (fd == 1)
  {
    unsigned org = size;
    while (size > 300) // split into chunks of 300 as per method details
    {
      putbuf(buffer, 300);
      size -= 300;
      buffer = buffer + 300;
    }

    putbuf(buffer, size);
    f->eax = org;
  }
  else
  {
    struct thread *cur = thread_current();
    lock_acquire(file_lock);
    f->eax = file_write(cur->fd_table[fd - 2], buffer, size);
    lock_release(file_lock);
  }
}

void handle_seek(struct intr_frame *f)
{
  // void *original = f->esp;
  int fd = *((int *)f->esp);
  f->esp = (char *)f->esp + 4;
  unsigned pos = *((unsigned *)f->esp);
  f->esp = (char *)f->esp - 8;

  struct thread *cur = thread_current();
  lock_acquire(file_lock);
  file_seek(cur->fd_table[fd - 2], pos);
  lock_release(file_lock);
}

void handle_tell(struct intr_frame *f)
{
  int fd = *((int *)f->esp);
  struct thread *cur = thread_current();

  lock_acquire(file_lock);
  off_t res = file_tell(cur->fd_table[fd - 2]);
  lock_release(file_lock);
  f->eax = res;
}

void handle_close(struct intr_frame *f)
{
  int fd = *((int *)f->esp);
  struct thread *cur = thread_current();

  lock_acquire(file_lock);
  file_close(cur->fd_table[fd - 2]);
  lock_release(file_lock);
  cur->fd_table[fd - 2] = NULL;
  cur->open_spot = fd - 2;
}