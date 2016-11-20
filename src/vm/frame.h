#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdbool.h>
#include <stdint.h>
#include <hash.h>
#include "threads/palloc.h"

struct frame_entry
{
    uint8_t *uaddr;
    uint8_t *kaddr;
    uint32_t *pagedir;

    struct thread* alloc_to;
    bool writable;
    bool pinned;

    struct hash_elem elem;
    struct list_elem evict_elem;
};

struct lock frame_lock;
struct list frame_alllist;

uint8_t* frame_get_page(struct hash* frame_table,uint8_t* upage, enum palloc_flags pal_flags,bool writable);
void frame_free_page(struct hash* frame_table,uint8_t* upage);
void frame_table_destroy(struct hash* frame_table);
struct frame_entry* find_frame(struct hash* frame_table,uint8_t* upage);
uint8_t* eviction_target_find(void);
void frame_table_init(struct hash* frame_table);


#endif
