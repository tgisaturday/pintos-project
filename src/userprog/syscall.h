#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "filesys/filesys.h"
#include "filesys/file.h"
void syscall_init (void);

void check_address(void* address,bool stack_growth);
void get_argument(void *esp, void **arg, int count);
int pibonacci(int n);
int sum_of_four_integers(int a, int b, int c, int d);

bool check_address_pinned(void* address, bool stack_growth);
int mmap(struct file *map_file, uint8_t* upage);
void munmap(int md);
#endif /* userprog/syscall.h */
