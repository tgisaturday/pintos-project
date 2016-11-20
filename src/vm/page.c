#include "vm/page.h"
#include <bitmap.h>
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
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "vm/swap.h"

#define SP_POOL_SIZE 2048
static int used_suppage_pool = 0;
static struct suppage_entry sp_entry[SP_POOL_SIZE];

void suppage_insert(struct hash* suppage_table,uint8_t* upage, struct file *swap_file,size_t offset, size_t length,bool is_segment,bool writable)
{
    struct suppage_entry* new_entry;
    lock_acquire(&page_lock);
    new_entry=&sp_entry[used_suppage_pool];
    used_suppage_pool++;
    new_entry->uaddr=upage;
    hash_insert(suppage_table,&new_entry->elem);
    new_entry->swap_file=swap_file;
    new_entry->offset=offset;
    new_entry->length=length;
    new_entry->is_mmap=(is_segment ? false:(swap_file!=NULL));
    new_entry->is_segment=is_segment;
    new_entry->writable=writable;
    lock_release(&page_lock);
}
void suppage_remove(struct hash* suppage_table, uint8_t *upage)
{
    struct suppage_entry *to_remove;
    lock_acquire(&page_lock);
    to_remove=suppage_find(suppage_table,upage);
    if(to_remove!=NULL)
    {
        hash_delete(suppage_table,&to_remove->elem);
        if(to_remove->swap_file==NULL)
            swap_free(to_remove->offset);
    }
    lock_release(&page_lock);
}
static hash_action_func suppage_destructor;

void suppage_destroy(struct hash* suppage_table)
{
    hash_destroy(suppage_table,suppage_destructor);
}
void suppage_destructor(struct hash_elem *elem,void *aux)
{
    struct suppage_entry *sup;
    sup=hash_entry(elem, struct suppage_entry,elem);
    if(sup->swap_file==NULL)
        swap_free(sup->offset);
}
struct suppage_entry* suppage_find(struct hash* suppage_table, uint8_t* upage)
{
    struct hash_elem *e;
    struct suppage_entry carrier;
    struct suppage_entry* sup;
    
    carrier.uaddr=upage;
    e=hash_find(suppage_table,&carrier.elem);
    if(e!=NULL)
    {
        sup=hash_entry(e,struct suppage_entry,elem);
        return sup;
    }
    else
        return NULL;
}
static hash_hash_func suppage_hash_func;
static hash_less_func suppage_hash_less;

static unsigned suppage_hash_func(const struct hash_elem *e, void *aux)
{
    return hash_int((int)hash_entry(e,struct suppage_entry,elem)->uaddr);
}

static bool suppage_hash_less(const struct hash_elem *a,const struct hash_elem *b,void* aux)
{
    return hash_entry(a,struct suppage_entry,elem)->uaddr < hash_entry(b,struct suppage_entry,elem)->uaddr;
}

void suppage_table_init(struct hash* suppage_table)
{
    hash_init(suppage_table,suppage_hash_func,suppage_hash_less,NULL);
}
