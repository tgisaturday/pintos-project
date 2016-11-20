#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <hash.h>
#include <bitmap.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "filesys/file.h"
#include "devices/block.h"
#include "threads/thread.h"

void swap_init(void);
void swap_read(uint8_t *uaddr,const unsigned offset);
unsigned swap_write(uint8_t *uaddr);
void swap_free(const unsigned offset);

#endif
