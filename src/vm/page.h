#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "threads/thread.h"
#include "filesys/file.h"

struct suppage_entry
{
    uint8_t* uaddr;
    struct file* swap_file;
    size_t offset;
    size_t length;
    bool is_mmap;
    bool is_segment;
    bool writable;
    struct hash_elem elem;
};
struct lock page_lock;

void suppage_init(void);
void page_lock_acquire(void);
void page_lock_release(void);

void suppage_insert(struct hash* suppage_table,uint8_t* upage, struct file *swap_file,size_t offset, size_t length, bool is_segment, bool writable);
void suppage_remove(struct hash* suppage_table, uint8_t *upage);
void suppage_destroy(struct hash* suppage_table);
struct suppage_entry* suppage_find(struct hash* suppage_table, uint8_t* upage);
void suppage_table_init(struct hash* suppage_table);
#endif
