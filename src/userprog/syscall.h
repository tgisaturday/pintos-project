#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

void check_address(void* address);
void get_argument(void *esp, void **arg, int count);
#endif /* userprog/syscall.h */
