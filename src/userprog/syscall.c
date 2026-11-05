#include "userprog/syscall.h"

#include <stdio.h>
#include <syscall-nr.h>
#include <string.h>

#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

struct lock lock_file;

static void syscall_handler(struct intr_frame *);
void check_addr(void *start, void *end, int fd, struct intr_frame *f, bool is_fd);
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
  lock_init(&lock_file);
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame *f UNUSED)
{
  thread_current()->saved_esp = f->esp;
  check_addr(f->esp, (char *)f->esp + 4, 0, f, false);
  int sys_num = *((int *)f->esp);

  switch (sys_num)
  {
  case SYS_HALT:
    handle_halt();
    break;
  case SYS_EXIT:
    handle_exit(f);
    break;
  case SYS_EXEC:
    handle_exec(f);
    break;
  case SYS_WAIT:
    handle_wait(f);
    break;
  case SYS_CREATE:
    handle_create(f);
    break;
  case SYS_REMOVE:
    handle_remove(f);
    break;
  case SYS_OPEN:
    handle_open(f);
    break;
  case SYS_FILESIZE:
    handle_filesize(f);
    break;
  case SYS_READ:
    handle_read(f);
    break;
  case SYS_WRITE:
    handle_write(f);
    break;
  case SYS_SEEK:
    handle_seek(f);
    break;
  case SYS_TELL:
    handle_tell(f);
    break;
  case SYS_CLOSE:
    handle_close(f);
    break;
  default:
    bad_exit(f);
  }
}

void check_addr(void *start, void *end, int fd, struct intr_frame *f, bool is_fd)
{
  if (is_fd) // Check the file descriptor
  {
    if (fd < 2 || fd >= 130 || thread_current()->fd_table[fd - 2] == NULL)
    {
      bad_exit(f);
    }
  }
  else // Check a pointer
  {
    if (!start || !end || !is_user_vaddr(start) || !is_user_vaddr(end) ||
        start > end)
    {
      bad_exit(f);
    }

    void *start_page = pg_round_down(start);
    void *end_page = pg_round_down(end);

    while (start_page <= end_page)
    {
      if (!is_user_vaddr(start_page))
      {
        bad_exit(f);
      }

      struct spt_entry *spte = spte_lookup(&thread_current()->spt, start_page);

      if (spte == NULL && !check_and_grow(start_page, thread_current()->saved_esp, f))
      {
        // fix during stack growth
        bad_exit(f);
      }

      start_page = (char *)start_page + PGSIZE;
    }
  }
}

bool check_and_grow(void *addr, void *esp, struct intr_frame *f)
{
  void *max_size = (void *)((char *)PHYS_BASE - (2048 * PGSIZE));
  if (addr < max_size)
  {
    return false;
  }

  void *border = (void *)((char *)esp - 32);
  if (addr < border)
  {
    return false;
  }

  struct spt_entry *spte = spte_init(pg_round_down(addr), true, 0, 0, 0, NULL);
  if (!spte)
  {
    return false;
  }

  spte->file_backed = false;
  spte->swapped = false;
  spte->all_zero = true;
  hash_insert(&thread_current()->spt, &spte->hash_elem);

  struct ft_entry *frame = frame_alloc(thread_current(), spte, true);
  if (!frame)
  {
    free(spte);
    bad_exit(f);
  }
  memset(frame->kvaddr, 0, PGSIZE);

  bool set = pagedir_set_page(thread_current()->pagedir, spte->uaddr, frame->kvaddr,
                              spte->writable);

  if (!set)
  {
    free_frame(spte);
    bad_exit(f);
  }

  spte->is_loaded = true;
  lock_acquire(&frame_lock);
  frame->locked = false;
  lock_release(&frame_lock);
  return true;
}

void bad_exit(struct intr_frame *f)
{
  f->eax = -1;

  if (thread_current()->my_status != NULL)
  {
    thread_current()->my_status->exit_status = -1;
  }

  printf("%s: exit(%d)\n", thread_current()->name, -1);
  thread_exit();
}

void handle_halt(void) { shutdown_power_off(); }

void handle_exit(struct intr_frame *f)
{
  // Get exit status
  char *esp = (char *)f->esp + 4;
  check_addr(esp, (char *)esp + 4, 0, f, false);
  int status = *((int *)esp);

  // Status for zombies
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
  // Get command line
  char *esp = (char *)f->esp + 4;

  check_addr(esp, (char *)esp + 4, 0, f, false);
  char *cmd_line = *((char **)esp);

  char *end = cmd_line;
  while (*end != '\0')
  {
    end = end + 4;
  }
  check_addr(cmd_line, end, 0, f, false);

  f->eax = process_execute(cmd_line);
}

void handle_wait(struct intr_frame *f)
{
  // Get pid
  char *esp = (char *)f->esp + 4;
  check_addr(esp, (char *)esp + 4, 0, f, false);
  tid_t pid = *((tid_t *)esp);

  f->eax = process_wait(pid);
}

void handle_create(struct intr_frame *f)
{
  // Get file name
  char *esp = (char *)f->esp + 4;
  check_addr(esp, (char *)esp + 4, 0, f, false);
  char *file_name = *((char **)esp);

  char *end = file_name;
  while (*end != '\0')
  {
    end = end + 4;
  }
  check_addr(file_name, end, 0, f, false);

  // Get size
  esp += 4;
  check_addr(esp, (char *)esp + 4, 0, f, false);
  unsigned size = *((unsigned *)esp);

  // Synchronized file create
  lock_acquire(&lock_file);
  f->eax = filesys_create(file_name, size);
  lock_release(&lock_file);
}

void handle_remove(struct intr_frame *f)
{
  char *esp = (char *)f->esp + 4;
  check_addr(esp, (char *)esp + 4, 0, f, false);
  char *file_name = *((char **)esp);

  char *end = file_name;
  while (*end != '\0')
  {
    end = end + 4;
  }
  check_addr(file_name, end, 0, f, false);

  lock_acquire(&lock_file);
  f->eax = filesys_remove(file_name);
  lock_release(&lock_file);
}

void handle_open(struct intr_frame *f)
{
  // Get file name
  char *esp = (char *)f->esp + 4;
  check_addr(esp, (char *)esp + 4, 0, f, false);
  char *file_name = *((char **)esp);

  char *end = file_name;
  while (*end != '\0')
  {
    end = end + 4;
  }
  check_addr(file_name, end, 0, f, false);

  // Synchronized file access
  lock_acquire(&lock_file);
  struct file *file = filesys_open(file_name);
  lock_release(&lock_file);

  // Corner cases
  if (!file)
  {
    f->eax = -1;
    return;
  }

  struct thread *cur = thread_current();

  if (cur->open_spot == 128)
  {
    lock_acquire(&lock_file);
    file_close(file);
    lock_release(&lock_file);

    f->eax = -1;
    return;
  }

  // Allocating the file a file descriptor
  cur->fd_table[cur->open_spot] = file;
  f->eax = cur->open_spot + 2;

  if (cur->open_spot == cur->open_end)
  {
    cur->open_end++;
  }
  cur->open_spot = cur->open_end;
}

void handle_filesize(struct intr_frame *f)
{
  // Get file descriptor
  char *esp = (char *)f->esp + 4;
  check_addr(esp, (char *)esp + 4, 0, f, false);
  int fd = *((int *)esp);
  check_addr(NULL, NULL, fd, f, true);

  // Synchronized file access
  lock_acquire(&lock_file);
  f->eax = file_length(thread_current()->fd_table[fd - 2]);
  lock_release(&lock_file);
}

void handle_read(struct intr_frame *f)
{
  // Get file descriptor
  char *esp = (char *)f->esp + 4;
  check_addr(esp, (char *)esp + 4, 0, f, false);
  int fd = *((int *)esp);

  // Get buffer
  esp += 4;
  check_addr(esp, (char *)esp + 4, 0, f, false);
  uint8_t *buffer = *((uint8_t **)esp);

  // Get size
  esp += 4;
  check_addr(esp, (char *)esp + 4, 0, f, false);
  unsigned size = *((unsigned *)esp);

  // Check size bytes of buffer
  check_addr(buffer, (char *)buffer + size, 0, f, false);

  // Handle reading from input
  if (fd == 0)
  {
    for (unsigned i = 0; i < size; i++)
    {
      buffer[i] = input_getc();
    }
    f->eax = size;
    return;
  }
  check_addr(NULL, NULL, fd, f, true); // delayed checked because we needed fd == 0

  // Synchronized file access
  lock_acquire(&lock_file);
  f->eax = file_read(thread_current()->fd_table[fd - 2], buffer, size);
  lock_release(&lock_file);
}

void handle_write(struct intr_frame *f)
{
  // Get file descriptor
  char *esp = (char *)f->esp + 4;
  check_addr(esp, (char *)esp + 4, 0, f, false);
  int fd = *((int *)esp);

  // Get buffer
  esp += 4;
  check_addr(esp, (char *)esp + 4, 0, f, false);
  char *buffer = *((char **)esp);

  // Get size
  esp += 4;
  check_addr(esp, (char *)esp + 4, 0, f, false);
  unsigned size = *((unsigned *)esp);

  // Check size bytes of buffer
  check_addr(buffer, (char *)buffer + size, 0, f, false);

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
    putbuf(buffer, size);
    f->eax = original;
    return;
  }
  check_addr(NULL, NULL, fd, f, true); // delayed fd check

  // Synchronized file access
  lock_acquire(&lock_file);
  f->eax = file_write(thread_current()->fd_table[fd - 2], buffer, size);
  lock_release(&lock_file);
}

void handle_seek(struct intr_frame *f)
{
  // Get file descriptor
  char *esp = (char *)f->esp + 4;
  check_addr(esp, (char *)esp + 4, 0, f, false);
  int fd = *((int *)esp);
  check_addr(NULL, NULL, fd, f, true);

  // Get size
  esp += 4;
  check_addr(esp, (char *)esp + 4, 0, f, false);
  unsigned pos = *((unsigned *)esp);

  // Synchronized file access
  lock_acquire(&lock_file);
  file_seek(thread_current()->fd_table[fd - 2], pos);
  lock_release(&lock_file);
}

void handle_tell(struct intr_frame *f)
{
  // Get file descriptor
  char *esp = (char *)f->esp + 4;
  check_addr(esp, (char *)esp + 4, 0, f, false);
  int fd = *((int *)esp);
  check_addr(NULL, NULL, fd, f, true);

  // Synchronized file access
  lock_acquire(&lock_file);
  off_t res = file_tell(thread_current()->fd_table[fd - 2]);
  lock_release(&lock_file);
  f->eax = res;
}

void handle_close(struct intr_frame *f)
{
  // Get file descriptor
  char *esp = (char *)f->esp + 4;
  check_addr(esp, (char *)esp + 4, 0, f, false);
  int fd = *((int *)esp);
  check_addr(NULL, NULL, fd, f, true);

  // Synchronized file access
  struct thread *cur = thread_current();
  lock_acquire(&lock_file);
  file_close(cur->fd_table[fd - 2]);
  lock_release(&lock_file);
  cur->fd_table[fd - 2] = NULL;
  cur->open_spot = fd - 2;
}