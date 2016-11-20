#include "vm/frame.h"
#include <bitmap.h>
#include <list.h>
#include <hash.h>
#include <inttypes.h>
#include <round.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "userprog/pagedir.h"
#include "threads/pte.h"
#include "threads/palloc.h"
#include "threads/loader.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "vm/page.h"
#include "vm/swap.h"

#define FR_POOL_SIZE 2048
static int used_frame_pool = 0;
static struct frame_entry fr_entry[FR_POOL_SIZE];
void frame_init(void)
{
    list_init(&frame_alllist);
    lock_init(&frame_lock);
    evict_target=NULL;
}
uint8_t* frame_get_page(struct hash* frame_table,uint8_t* upage, enum palloc_flags pal_flags,bool writable)
{
    lock_acquire(&frame_lock);
    uint8_t* frame=(uint8_t*)palloc_get_page(pal_flags);
    struct frame_entry* new_entry;
    
    if(frame==NULL)
    {
        lock_release(&frame_lock);
        frame=eviction_target_find();
        if(frame==NULL)
        {
            return NULL;
        }
        lock_acquire(&frame_lock);
        if(pal_flags & PAL_ZERO)
            memset(frame,0,PGSIZE);
    }
    new_entry=&fr_entry[used_frame_pool];
    used_frame_pool++;

    lock_acquire(&(thread_current()->page_lock));
    new_entry->pagedir=thread_current()->pagedir;
    new_entry->kaddr=frame;
    new_entry->uaddr=upage;
    new_entry->pinned=false;
    new_entry->writable=writable;
    new_entry->alloc_to=thread_current();
    hash_insert(frame_table,&new_entry->elem);
    list_push_back(&frame_alllist,&new_entry->evict_elem);
    pagedir_set_page(new_entry->pagedir,new_entry->uaddr,new_entry->kaddr,new_entry->writable);
    lock_release(&(thread_current()->page_lock));

    lock_release(&frame_lock);
    return frame;
}
void frame_free_page(struct hash* frame_table,uint8_t* upage)
{
    struct frame_entry* frm=NULL;
    lock_acquire(&frame_lock);
    frm=find_frame(frame_table,upage);
    if(frm != NULL)
    {
        if(&frm->evict_elem == evict_target)
        {
            if(evict_target ==list_end(&frame_alllist))
                evict_target=list_next(evict_target);
            else
                evict_target=NULL;
        }
        lock_acquire(&(thread_current()->page_lock));
        list_remove(&frm->evict_elem);
        hash_delete(frame_table,&frm->elem);
        pagedir_clear_page(frm->pagedir,frm->uaddr);
        lock_release(&(thread_current()->page_lock));
        palloc_free_page(frm->kaddr);
    }
    lock_release(&frame_lock);
}
static hash_action_func frame_destructor;

void frame_table_destroy(struct hash* frame_table)
{
    lock_acquire(&frame_lock);
    hash_destroy(frame_table,frame_destructor);
    lock_release(&frame_lock);
}

static void frame_destructor(struct hash_elem *e, void *aux)
{
    struct frame_entry *frm;
    frm=hash_entry(e,struct frame_entry,elem);
    if(frm!=NULL)
    {
        if(&frm->evict_elem == evict_target)
        {
            if(evict_target ==list_end(&frame_alllist))
                evict_target=list_next(evict_target);
            else
                evict_target=NULL;
        }
       // lock_acquire(&(thread_current()->page_lock));
        list_remove(&frm->evict_elem);
        //pagedir_clear_page(frm->pagedir,frm->uaddr);
       // lock_release(&(thread_current()->page_lock));
       // palloc_free_page(frm->kaddr);
    }
}
struct frame_entry* find_frame(struct hash* frame_table,uint8_t* upage)
{
    struct hash_elem *e;
    struct frame_entry carrier;
    struct frame_entry* frm;
    
    carrier.uaddr=upage;
    e=hash_find(frame_table,&carrier.elem);
    if(e!=NULL)
    {
        frm=hash_entry(e,struct frame_entry,elem);
        return frm;
    }
    else 
        return NULL;
}

uint8_t* eviction_target_find(void)
{
    lock_acquire(&frame_lock);
    int loop_cnt=list_size(&frame_alllist) << 1;
    
    while(loop_cnt-- > 0)
    {
        if(evict_target == NULL || evict_target == list_end(&frame_alllist))
        {
            evict_target = list_begin(&frame_alllist);
        }
        struct frame_entry *e=list_entry(evict_target,struct frame_entry,evict_elem);

        if(e->pinned)
        {
            if(evict_target !=list_end(&frame_alllist))
            {
                evict_target=list_next(evict_target);
            }
            else
                evict_target=NULL;
            continue;
        }
        lock_acquire(&e->alloc_to->page_lock);
        if(pagedir_is_accessed(e->pagedir,e->uaddr))
        {
            pagedir_set_accessed(e->pagedir,e->uaddr,false);
            lock_release(&e->alloc_to->page_lock);
            if(evict_target !=list_end(&frame_alllist))
            {
                evict_target=list_next(evict_target);
            }
            else
                evict_target=NULL;
        }
        else
        {
            lock_release(&e->alloc_to->page_lock);
            unsigned offset=BITMAP_ERROR;
            if(evict_target !=list_end(&frame_alllist))
            {
                evict_target=list_next(evict_target);
            }
            else
                evict_target=NULL;
            if(e->writable)
            {
                offset=swap_write(e->kaddr);
            }
            if(offset!=BITMAP_ERROR || !e->writable)
            {
                lock_acquire(&e->alloc_to->page_lock);
                list_remove(&e->evict_elem);
                hash_delete(&e->alloc_to->frame_table,&e->elem);
                pagedir_clear_page(e->pagedir,e->uaddr);
                lock_release(&e->alloc_to->page_lock);
                uint8_t *kpage=e->kaddr;
                if(e->writable)
                {
                    if(suppage_find(&e->alloc_to->suppage_table,e->uaddr)!=NULL)
                        suppage_remove(&e->alloc_to->suppage_table,e->uaddr);
                    suppage_insert(&e->alloc_to->suppage_table,e->uaddr,NULL,offset,PGSIZE,false,true);
                }
                lock_release(&frame_lock);
                return kpage;
            }
            else
            {
                lock_release(&frame_lock);
                return NULL;
            }
        }
    }
    lock_release(&frame_lock);
    return NULL;

}

static hash_hash_func frame_hash_func;
static hash_less_func frame_hash_less;

static unsigned frame_hash_func(const struct hash_elem *e, void *aux)
{
    return hash_int((int)hash_entry(e,struct frame_entry,elem)->uaddr);
}

static bool frame_hash_less(const struct hash_elem *a,const struct hash_elem *b,void* aux)
{
    return hash_entry(a,struct frame_entry,elem)->uaddr < hash_entry(b,struct frame_entry,elem)->uaddr;
}

void frame_table_init(struct hash* frame_table)
{
    hash_init(frame_table, frame_hash_func,frame_hash_less,NULL);
}


