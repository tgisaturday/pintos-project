#include "vm/swap.h"
#include <hash.h>
#include <bitmap.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "filesys/file.h"
#include "devices/block.h"
#include "threads/thread.h"

static struct lock swap_lock;
static struct bitmap *swap_slots;
static struct block *swap_block;

void swap_init(void)
{
    swap_slots = bitmap_create(1024);
    swap_block = block_get_role(BLOCK_SWAP);
    lock_init(&swap_lock);
}

void swap_read(uint8_t *uaddr,const unsigned offset)
{
    lock_acquire(&swap_lock);
    unsigned i=0;
    for(i=0;i<8;i++)
    {
        block_read(swap_block,offset*8+i,uaddr+512*i);
    }
    lock_release(&swap_lock);
}

unsigned swap_write(uint8_t *uaddr)
{
    lock_acquire(&swap_lock);
    unsigned offset=0;
    unsigned i=0;
    offset=bitmap_scan_and_flip(swap_slots,0,1,false);

    if(offset == BITMAP_ERROR)
    {
        lock_release(&swap_lock);
        return BITMAP_ERROR;
    }
    for(i=0;i<8;i++)
    {
        block_write(swap_block,offset*8+i,uaddr+512*i);
    }
    lock_release(&swap_lock);
    return offset;
}

void swap_free(const unsigned offset)
{
    lock_acquire(&swap_lock);
    bitmap_set(swap_slots,offset,false);
    lock_release(&swap_lock);
}
