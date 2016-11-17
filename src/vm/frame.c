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

uint8_t* frame_get_page(struct hash* frame_table,uint8_t* upage, enum palloc_flags pal_flags)
{
    lock_acquire(&frame_lock);
    uint8_t* frame=(uint8_t*)palloc_get_page(pal_flags);
    struct frame_entry* new_entry;
    if(frame!=NULL)
    {
        new_entry=&fr_entry[used_frame_pool];
        used_frame_pool++;
        new_entry->kaddr=frame;
        new_entry->uaddr=upage;
        new_entry->alloc_to=thread_current();
        hash_insert(frame_table,&new_entry->elem);
        list_push_back(&frame_alllist,&new_entry->evict_elem);
        new_entry->pagedir=thread_current()->pagedir;
        lock_release(&frame_lock);
        return frame;
    }
    else
    {
        lock_release(&frame_lock);
        return NULL;
    }
}
void frame_free_page(struct hash* frame_table,uint8_t* upage)
{
    struct frame_entry* frm=NULL;
    lock_acquire(&frame_lock);
    frm=find_frame(frame_table,upage);
    if(frm != NULL)
    {
        list_remove(&frm->evict_elem);
        hash_delete(frame_table,&frm->elem);
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
    struct frame_entry *frame;
    frame=hash_entry(e,struct frame_entry,elem);
    if(frame!=NULL)
    {
        list_remove(&frame->evict_elem);
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


