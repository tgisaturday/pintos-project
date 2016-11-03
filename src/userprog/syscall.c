#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "process.h"
#include "pagedir.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include <string.h>
static void syscall_handler (struct intr_frame *);


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}
void check_address(void* address)
{
    struct thread *cur=thread_current();
    if (address==NULL)
    {
        thread_current()->sync.exit_status=-1;
        thread_exit();//exit(-1);
    }
    else if(is_kernel_vaddr(address))
    {
        thread_current()->sync.exit_status=-1;
        thread_exit();//exit(-1);
    }
    else if(pagedir_get_page(cur->pagedir,address)==NULL)
    {
        thread_current()->sync.exit_status=-1;
        thread_exit();//exit(-1);
    }
}
void get_argument(void *esp, void **arg, int count)
{
    int i;
    for(i=0;i<count;i++)
    {
        arg[i]=esp;
        esp=(void*)((uintptr_t)esp+4);
    }
}
int pibonacci(int n)
{
    int num=1;
    int prev=-1;
    int sum;
    int i;
    for(i=0;i<=n;i++)
    {
        sum=num+prev;
        prev=num;
        num=sum;
    }
    return num;
}
int sum_of_four_integers(int a, int b, int c, int d)
{
    return a+b+c+d;
}
//the first 4 byte of f->esp is syscall_num
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  //make this work by using syscall_num stored in user_stack
  //check if f->esp is pointing somewhere in user memory.
  //check if data stored in f->esp is address in user memory.
  void *esp=f->esp;
  int syscall_num;
  void *arg[10];
  int i;
  int exit_status;
  int read_count;
  struct thread *t=thread_current();
  tid_t tid;
  
  check_address(esp);
  syscall_num=*(int*)esp;
  esp=(void*)((uintptr_t)esp+4);
  switch(syscall_num)
  {
      case SYS_HALT: //Project 1
          shutdown_power_off();
          NOT_REACHED();
          break;
      case SYS_EXIT: //Project 1
          get_argument(esp,arg,1);
          check_address(arg[0]);
          t->sync.exit_status=*(int*)arg[0];
          //modify everything in thread_exit()
          thread_exit();
          break;
      case SYS_EXEC: //Project 1
          get_argument(esp,arg,1);
          check_address(arg[0]);
          check_address(*(char**)arg[0]);
          tid=process_execute(*(char**)(arg[0]));
          sema_down(&(t->sync.exec));
          if(t->sync.exit_status==-1)
              f->eax=-1;
          else
              f->eax=tid;
          //use and modify process_execute
          break;
      case SYS_WAIT: //Project 1
          get_argument(esp,arg,1);
          check_address(arg[0]);
          if(*(int*)arg[0]<0)
          {
              thread_current()->sync.exit_status=-1;
              thread_exit();//exit(-1);
          }
          exit_status=process_wait(*(int*)arg[0]);
          f->eax=exit_status;
          break;
      case SYS_CREATE:
          break;
      case SYS_REMOVE:
          break;
      case SYS_OPEN:
          break;
      case SYS_FILESIZE:
          break;
      case SYS_READ: //Project 1
          get_argument(esp,arg,3);
          for(i=0;i<3;i++) 
              check_address(arg[i]);
          if(*(int*)arg[0]==0)
          {
              read_count=0;
              for(i=0;i<*(int*)arg[2];i++)
              {
                  (*(char**)arg[1])[i]=input_getc();
                  read_count++;
                  if((*(char**)arg[1])[i]==0)
                      break;
              }
              f->eax=read_count;
          }
          break;
      case SYS_WRITE: //Project 1
          get_argument(esp,arg,3);
          for(i=0;i<3;i++)
              check_address(arg[i]);
          if(*(int*)arg[0]==1)
              putbuf(*(char**)(arg[1]),*(int*)(arg[2]));
          break;
      case SYS_SEEK:
          break;
      case SYS_TELL:
          break;
      case SYS_CLOSE:
          break;
      case SYS_MMAP:
          break;
      case SYS_MUNMAP:
          break;
      case SYS_CHDIR:
          break;
      case SYS_MKDIR:
          break;
      case SYS_READDIR:
          break;
      case SYS_ISDIR:
          break;
      case SYS_INUMBER:
          break;
      case PIBONACCI:
          get_argument(esp,arg,1);
          check_address(arg[0]);
          f->eax=pibonacci(*(int*)arg[i]);
          break;
      case SUM_OF_FOUR_INTEGERS:
          get_argument(esp,arg,4);
          for(i=0;i<3;i++)
              check_address(arg[i]);
          f->eax=sum_of_four_integers(*(int*)arg[0],*(int*)arg[1],*(int*)arg[2],*(int*)arg[3]);
          break;
      default:
          printf("Fucking! Not Guhyeoned!");
          thread_exit();
  };
}
