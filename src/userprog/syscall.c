#include "userprog/syscall.h"

#include <stdio.h>
#include <syscall-nr.h>

#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"

struct lock lock_file;

static void syscall_handler(struct intr_frame*);
void check_addr(void* addr, struct intr_frame* f);
void bad_exit(struct intr_frame* f);
void handle_halt(void);
void handle_exit(struct intr_frame* f);
void handle_exec(struct intr_frame* f);
void handle_wait(struct intr_frame* f);
void handle_create(struct intr_frame* f);
void handle_remove(struct intr_frame* f);
void handle_open(struct intr_frame* f);
void handle_filesize(struct intr_frame* f);
void handle_read(struct intr_frame* f);
void handle_write(struct intr_frame* f);
void handle_seek(struct intr_frame* f);
void handle_tell(struct intr_frame* f);
void handle_close(struct intr_frame* f);

int global_fd[128][2];

void syscall_init(void) {
  lock_init(&lock_file);
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame* f UNUSED) {
  check_addr(f->esp, f);
  check_addr((char*)f->esp + 1, f);
  check_addr((char*)f->esp + 2, f);
  check_addr((char*)f->esp + 3, f);

  int sys_num = *((int*)f->esp);

  switch (sys_num) {
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
  }
}

void check_addr(void* addr, struct intr_frame* f) {
  if (!addr || is_kernel_vaddr(addr) ||
      !pagedir_get_page(thread_current()->pagedir, addr)) {
    bad_exit(f);
  }
}

void bad_exit(struct intr_frame* f) {
  f->eax = -1;

  if (thread_current()->my_status != NULL) {
    thread_current()->my_status->exit_status = -1;
  }

  printf("%s: exit(%d)\n", thread_current()->name, -1);
  thread_exit();
}

void handle_halt(void) { shutdown_power_off(); }

void handle_exit(struct intr_frame* f) {
  char* esp = (char*)f->esp + 4;
  check_addr(esp, f);
  check_addr(esp + 3, f);
  int status = *((int*)esp);
  struct thread* cur = thread_current();
  if (cur->my_status != NULL) {
    cur->my_status->exit_status = status;
  }
  printf("%s: exit(%d)\n", cur->name, status);
  thread_exit();
}

void handle_exec(struct intr_frame* f) {
  uint32_t* p = thread_current()->pagedir;
  char* esp = (char*)f->esp + 4;
  check_addr(esp, f);
  check_addr(esp + 3, f);
  char* cmd_line = *((char**)esp);
  char* byte = cmd_line;

  check_addr(byte, f);

  while (*byte != '\0') {
    byte++;
    check_addr(byte, f);
  }
  f->eax = process_execute(cmd_line);
}

void handle_wait(struct intr_frame* f) {
  char* esp = (char*)f->esp + 4;
  check_addr(esp, f);
  check_addr(esp + 3, f);
  tid_t pid = *((tid_t*)esp);
  f->eax = process_wait(pid);
}

void handle_create(struct intr_frame* f) {
  char* esp = (char*)f->esp + 4;
  check_addr(esp, f);
  check_addr(esp + 3, f);
  char* file_name = *((char**)esp);
  check_addr(file_name, f);
  esp += 4;
  check_addr(esp, f);
  check_addr(esp + 3, f);
  unsigned size = *((unsigned*)esp);
  char* byte = file_name;
  check_addr(byte, f);
  while (*byte != '\0') {
    byte++;
    check_addr(byte, f);
  }
  lock_acquire(&lock_file);
  f->eax = filesys_create(file_name, size);
  lock_release(&lock_file);
}

void handle_remove(struct intr_frame* f) {
  struct thread* cur = thread_current();
  int fd = *((int*)f->esp) - 2;

  char* esp = (char*)f->esp + 4;
  char* file_name = *((char**)esp);
  cur->fd_table[fd] = NULL;
  --global_fd[fd][1];

  if (global_fd[fd][1] == 0) {
    if (!filesys_remove(file_name)) {
      f->eax = 0;
      thread_exit();
    }
    f->eax = 1;
  }
  return;
}

void handle_open(struct intr_frame* f) {
  char* esp = (char*)f->esp + 4;
  struct thread* cur = thread_current();
  check_addr(esp, f);
  check_addr(esp + 3, f);
  char* file_name = *((char**)esp);

  char* byte = file_name;
  check_addr(byte, f);
  while (*byte != '\0') {
    byte++;
    check_addr(byte, f);
  }
  lock_acquire(&lock_file);
  struct file* file = filesys_open(file_name);
  lock_release(&lock_file);

  if (!file) {
    f->eax = -1;
    return;
  }
  if (cur->open_spot == 128) {
    lock_acquire(&lock_file);
    file_close(file);
    lock_release(&lock_file);
    f->eax = -1;
    return;
  }

  cur->fd_table[cur->open_spot] = file;
  f->eax = cur->open_spot + 2;

  if (cur->open_spot == cur->open_end) {
    cur->open_end++;
  }
  ++global_fd[cur->open_spot][1];
  cur->open_spot = cur->open_end;
}

void handle_filesize(struct intr_frame* f) {
  char* esp = (char*)f->esp + 4;
  check_addr(esp, f);
  check_addr(esp + 3, f);
  int fd = *((int*)esp);
  struct thread* cur = thread_current();

  if (fd < 2 || fd >= 130 || cur->fd_table[fd - 2] == NULL) {
    f->eax = -1;
    return;
  }
  lock_acquire(&lock_file);
  f->eax = file_length(cur->fd_table[fd - 2]);
  lock_release(&lock_file);
}

void handle_read(struct intr_frame* f) {
  // Get the syscall args
  struct thread* cur = thread_current();
  char* esp = (char*)f->esp + 4;
  check_addr(esp, f);
  check_addr(esp + 3, f);
  int fd = *((int*)esp);
  esp += 4;
  check_addr(esp, f);
  check_addr(esp + 3, f);
  uint8_t* buffer = *((uint8_t**)esp);
  esp += 4;
  check_addr(esp, f);
  check_addr(esp + 3, f);
  unsigned size = *((unsigned*)esp);
  check_addr((void*)buffer, f);

  // Reading input
  if (fd == 0) {
    for (unsigned i = 0; i < size; i++) {
      buffer[i] = input_getc();
    }
    f->eax = size;
    return;
  }

  if (fd < 2 || fd >= 130 || cur->fd_table[fd - 2] == NULL) {
    f->eax = -1;
    return;
  }

  lock_acquire(&lock_file);
  f->eax = file_read(cur->fd_table[fd - 2], buffer, size);
  lock_release(&lock_file);
}

void handle_write(struct intr_frame* f) {
  // Get the syscall args
  char* esp = (char*)f->esp + 4;
  check_addr(esp, f);
  check_addr(esp + 3, f);
  int fd = *((int*)esp);
  esp += 4;
  check_addr(esp, f);
  check_addr(esp + 3, f);

  char* buffer = *((char**)esp);
  esp += 4;
  check_addr(esp, f);
  check_addr(esp + 3, f);
  struct thread* cur = thread_current();
  unsigned size = *((unsigned*)esp);
  check_addr(buffer, f);

  // Handle writing to console
  if (fd == 1) {
    unsigned original = size;
    while (size > 300)  // split into chunks of 300 as per method details
    {
      putbuf(buffer, 300);
      size -= 300;
      buffer += 300;
    }
    putbuf(buffer, size);
    f->eax = original;
    return;
  }
  if (fd < 2 || fd >= 130 || cur->fd_table[fd - 2] == NULL) {
    f->eax = -1;
    return;
  }
  lock_acquire(&lock_file);
  f->eax = file_write(cur->fd_table[fd - 2], buffer, size);
  lock_release(&lock_file);
}

void handle_seek(struct intr_frame* f) {
  char* esp = (char*)f->esp + 4;
  check_addr(esp, f);
  check_addr(esp + 3, f);

  int fd = *((int*)esp);
  esp += 4;
  check_addr(esp, f);
  check_addr(esp + 3, f);
  unsigned pos = *((unsigned*)esp);

  struct thread* cur = thread_current();
  if (fd < 2 || fd >= 130 || cur->fd_table[fd - 2] == NULL) {
    return;
  }
  lock_acquire(&lock_file);
  file_seek(cur->fd_table[fd - 2], pos);
  lock_release(&lock_file);
}

void handle_tell(struct intr_frame* f) {
  char* esp = (char*)f->esp + 4;
  check_addr(esp, f);
  check_addr(esp + 3, f);
  int fd = *((int*)esp);
  struct thread* cur = thread_current();
  if (fd < 2 || fd >= 130 || cur->fd_table[fd - 2] == NULL) {
    f->eax = -1;
    return;
  }
  lock_acquire(&lock_file);
  off_t res = file_tell(cur->fd_table[fd - 2]);
  f->eax = res;
  lock_release(&lock_file);
}

void handle_close(struct intr_frame* f) {
  char* esp = (char*)f->esp + 4;
  check_addr(esp, f);
  check_addr(esp + 3, f);
  int fd = *((int*)esp);
  struct thread* cur = thread_current();

  if (fd < 2 || fd >= 130 || cur->fd_table[fd - 2] == NULL) {
    return;
  }
  lock_acquire(&lock_file);
  file_close(cur->fd_table[fd - 2]);
  lock_release(&lock_file);
  cur->fd_table[fd - 2] = NULL;
  cur->open_spot = fd - 2;
}