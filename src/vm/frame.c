#include "vm/frame.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"

void frame_init(struct ft_entry *fte);
void frame_alloc(struct thread *cur, struct ft_entry *fte, struct spt_entry *spte, bool lock);
bool frame_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
struct ft_entry *frame_eviction(struct spt_entry* spte);

void frame_init(struct ft_entry *fte)
{
  list_init(&alloc_frames);
  lock_init(&frame_lock);
}

void frame_alloc(struct thread *cur, struct ft_entry *fte,
                 struct spt_entry *spte, bool lock)
{
  void *kpage = palloc_get_page(PAL_USER);
  
  if (!kpage)
  {
    fte = frame_eviction(spte);
  } else {
    // new
    fte->kvaddr = kpage;
    fte->page = spte;
    lock_acquire(&frame_lock);
    list_push_back(&alloc_frames, &fte->elem);
    lock_release(&frame_lock);
  }

  fte->locked = lock;
  fte->owner = cur;
  fte->priority = 0;
}

bool frame_priority(const struct list_elem *a, const struct list_elem *b,
                    void *aux UNUSED)
{
  struct ft_entry *first = list_entry(a, struct ft_entry, can_elem);
  struct ft_entry *second = list_entry(b, struct ft_entry, can_elem);

  return (first->priority > second->priority);
}

struct ft_entry *frame_eviction(struct spt_entry* spte)
{
  struct list candidates;
  list_init(&candidates);
  struct ft_entry *victim = NULL;

  // Getting our victims
  lock_acquire(&frame_lock);
  struct list_elem *e = list_begin(&alloc_frames);

  for (int i = 0; i < 2; i++)
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
        break;
      }
      else if (!a && d)
      {
        temp->priority = 3;
      }
      else if (a && !d)
      {
        temp->priority = 2;
      }
      else
      {
        temp->priority = 1;
      }

      list_insert_ordered(&candidates, &temp->can_elem, frame_priority, NULL);
      pagedir_set_accessed(pd, uaddr, false);
    }

    if (!victim)
    {
      break;
    }
  }

  // Evicting said victim
  if (!victim)
  {
    victim = list_pop_front(&candidates);
  }

  // Update spte of victim
  victim->page->swapped = true;
  victim->page = spte;
  lock_release(&frame_lock);

  // TODO: Do swap and set swap slot in spte

  // Clear pagedir
  pagedir_clear_page(victim->owner->pagedir, victim->page->uaddr);

  return victim;
}