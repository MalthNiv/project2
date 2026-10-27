#include "vm/frame.h"

#include "threads/malloc.h"
#include "threads/palloc.h"

void frame_init(struct ft_entry* fte) {
  fte->kvaddr = NULL;
  fte->page = NULL;
  fte->owner = NULL;
  fte->locked = false;
}

void frame_alloc(struct thread* cur, struct ft_entry* fte,
                 struct sp_entry* spte, bool lock) {
  void* kpage = palloc_get_page(PAL_USER);
  if (kpage != NULL) {
    fte->page = spte;
    fte->locked = lock;
    fte->kvaddr = kpage;
    list_push_back(&alloc_frames, &fte->elem);
    return;
  }
  // NEED TO IMPLEMENT EVICTION
}