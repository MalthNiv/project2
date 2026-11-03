#include "vm/frame.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/swap.h"

void frame_init(void)
{
  list_init(&alloc_frames);
  lock_init(&frame_lock);
}

bool frame_alloc(struct thread *cur, struct ft_entry *fte, struct spt_entry *spte, bool lock)
{
  void *kpage = palloc_get_page(PAL_USER | PAL_ZERO);

  if (!kpage)
  {
    fte = frame_eviction(spte);
    if (!fte)
    {
      return false;
    }
  }
  else
  {
    // new
    fte->kvaddr = kpage;
    fte->page = spte;
    lock_acquire(&frame_lock);
    list_push_back(&alloc_frames, &fte->elem);
    lock_release(&frame_lock);
  }

  fte->locked = lock;
  fte->owner = cur;
  return true;
}

struct ft_entry *frame_eviction(struct spt_entry *spte)
{
  // Getting our victims
  struct ft_entry *victim;
  bool found = false;
  
  lock_acquire(&frame_lock);
  struct list_elem *e = list_begin(&alloc_frames);

  for (int i = 0; i < 3; i++)
  {
    while (e != list_end(&alloc_frames))
    {
      struct ft_entry *temp = list_entry(e, struct ft_entry, elem);

      uint32_t *pd = temp->owner->pagedir;
      void *uaddr = temp->page->uaddr;

      bool a = pagedir_is_accessed(pd, uaddr);
      bool d = pagedir_is_dirty(pd, uaddr);

      if (!a && !d)
      {
        victim = temp;
        found = true;
        break;
      }
      else if (i > 0 && !a && d)
      {
        victim = temp;
        break;
      }
      else // default because victim cannot be null after the loop is done
      {
        victim = temp;
      }

      if (i == 1)
      {
        pagedir_set_accessed(pd, uaddr, false);
      }
    }

    if (found)
    {
      break;
    }
  }

  // Update spte of victim
  victim->page->swapped = true;

  // Do swap and set swap slot in spte
  int slot = alloc_swap_slot(victim->kvaddr);

  if (slot == -1)
  {
    lock_release(&frame_lock);
    return NULL;
  }

  victim->page = spte;
  victim->page->swap_slot = slot;
  lock_release(&frame_lock);

  // Clear pagedir
  pagedir_clear_page(victim->owner->pagedir, victim->page->uaddr);

  return victim;
}

void free_frame(struct spt_entry *spte)
{
  return;
}