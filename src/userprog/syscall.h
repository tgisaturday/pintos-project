#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

void check_address(void* address);
void get_argument(void *esp, void **arg, int count);
int pibonacci(int n);
int sum_of_four_integers(int a, int b, int c, int d);
#endif /* userprog/syscall.h */
