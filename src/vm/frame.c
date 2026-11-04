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

struct lock frame_lock;
struct list alloc_frames;

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
  victim = list_entry(e, struct ft_entry, elem);

  for (int i = 0; i < 3; i++)
  {
    while (e != list_end(&alloc_frames))
    {
      struct ft_entry *temp = list_entry(e, struct ft_entry, elem);

      if (!temp->locked)
      {
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

        if (found)
        {
          break;
        }
      }

      e = list_next(e);
    }
  }

  // Update spte of victim
  victim->page->is_loaded = false;

  // Do swap and set swap slot in spte if page is dirty
  if (pagedir_is_dirty(victim->owner->pagedir, victim->page->uaddr))
  {
    victim->page->swapped = true;
    int slot = alloc_swap_slot(victim->kvaddr);

    if (slot == -1)
    {
      lock_release(&frame_lock);
      return NULL;
    }

    victim->page->swap_slot = slot;
  }

  // Clear pagedir
  pagedir_clear_page(victim->owner->pagedir, victim->page->uaddr);
  victim->page = spte;

  lock_release(&frame_lock);
  return victim;
}

void free_frame(struct spt_entry *spte)
{
  lock_acquire(&frame_lock);

  if (!list_empty(&alloc_frames))
  {
    for (struct list_elem *e = list_front(&alloc_frames); e != list_end(&alloc_frames);
         e = list_next(e))
    {
      struct ft_entry *fte = list_entry(e, struct ft_entry, elem);

      if (fte->page->uaddr == spte->uaddr && fte->owner == thread_current())
      {
        if (fte->owner->pagedir != NULL)
        {
          pagedir_clear_page(fte->owner->pagedir, fte->page->uaddr);
          palloc_free_page(fte->kvaddr);
        }
        list_remove(&fte->elem);
        lock_release(&frame_lock);
        free(fte);
        return;
      }
    }
  }

  lock_release(&frame_lock);
}