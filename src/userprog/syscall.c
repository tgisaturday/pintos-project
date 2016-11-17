#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "process.h"
#include "pagedir.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include <string.h>
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
static void syscall_handler (struct intr_frame *);

#define STACK_SIZE_LIMIT (uint8_t*)((uint8_t*)(PHYS_BASE) - (uint8_t*)(8*1024*1024))
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
int mmap(struct file *map_file, uint8_t* upage)
{
    lock_acquire(&mmap_lock);
    int mmap_length=file_length(map_file);
    int mmap_page_num=(mmap_length + PGSIZE -1) / PGSIZE;
    /* if not page-alligned */
    if((unsigned)upage != ((unsigned)upage / PGSIZE) * PGSIZE || upage + mmap_page_num * PGSIZE >= (uint8_t*)PHYS_BASE)
    {
        lock_release(&mmap_lock);
        return -1;
    }
    /* if in STACK */
    if(STACK_SIZE_LIMIT <= upage && upage + mmap_page_num * PGSIZE >= (uint8_t*)PHYS_BASE)
    {
        lock_release(&mmap_lock);
        return -1;
    }
    int mmap_left=mmap_length;
    int offset = 0;
    uint8_t *upage_cur=upage;
    struct file* file_cur=map_file;

    /* if zero-sized file */
    if(mmap_page_num < 1 || mmap_length < 1)
    {
        lock_release(&mmap_lock);
        return -1;
    }

    int old_offset=file_tell(file_cur);

    /* if page is already assigned */
    int i;
    for(i=0;i<mmap_page_num;i++)
    {
        if(suppage_find(&thread_current()->suppage_table, upage + PGSIZE*i))
        {
            lock_release(&mmap_lock);
            return -1;
        }
    }

    /* add to suppage. mmap loading is lazy. */
    while(mmap_left > 0)
    {
        suppage_insert(&thread_current()->suppage_table,upage_cur,file_cur,offset,mmap_left >= PGSIZE ? PGSIZE : mmap_left,false,true);
        mmap_left-=PGSIZE;
        upage_cur+=PGSIZE;
        offset+=PGSIZE;
    }
    file_seek(file_cur,old_offset);

    struct mmap_entry* new_mmap = (struct mmap_entry*)malloc(sizeof(struct mmap_entry));
    new_mmap->md=thread_current()->md_gen;
    thread_current()->md_gen++;
    new_mmap->file=file_cur;
    new_mmap->upage=upage;
    new_mmap->length=mmap_length;
    list_push_back(&thread_current()->mmap_list,&new_mmap->elem);
    
    lock_release(&mmap_lock);
    return new_mmap->md;

}
void munmap(int md)
{
    lock_acquire(&mmap_lock);
    struct list_elem *e;
    struct mmap_entry* mmap_temp;
    struct mmap_entry* mmap=NULL;
    if(list_empty(&thread_current()->mmap_list))
    {
        lock_release(&mmap_lock);
        return;
    }
    for(e=list_begin(&thread_current()->mmap_list);e!=list_end(&thread_current()->mmap_list);e=list_next(e))
    {
        mmap_temp=list_entry(e,struct mmap_entry,elem);
        if(mmap_temp->md==md)
        {
            mmap=mmap_temp;
            break;
        }
    }
    if(mmap==NULL)
    {
        lock_release(&mmap_lock);
        return;
    }
    list_remove(e);

    int offset=0;
    int old_offset=file_tell(mmap->file);
    int mmap_left=mmap->length;
    uint8_t *upage_cur=mmap->upage;

    lock_acquire(&file_rw);
    while(mmap_left > 0)
    {
        check_address((void*)upage_cur);
        file_write_at(mmap->file, upage_cur, mmap_left < PGSIZE ? mmap_left : PGSIZE, offset);
        mmap_left-=PGSIZE;
        upage_cur+=PGSIZE;
        offset+=PGSIZE;
    }
    file_seek(mmap->file,old_offset);
    lock_release(&file_rw);

    free(mmap);
    lock_release(&mmap_lock);
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
  int f_size;
  struct thread *t=thread_current();
  struct thread *child;
  bool rt;
  unsigned uint_f;
  struct file* new_file;
  struct file_data* new_file_data;
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
          if(tid!=TID_ERROR)
          {
              child=search_thread(tid);          
              sema_down(&(child->sync.exec));
              if(child->sync.exit_status==-1) 
              {
                  sema_up(&(child->sync.exec));
                  f->eax=-1;
              } 
              else 
              {
                  f->eax=tid;
              }
          }
          else
              f->eax=-1;
          //use and modify process_execute
          break;
      case SYS_WAIT: //Project 1
          get_argument(esp,arg,1);
          check_address(arg[0]);
          exit_status=process_wait(*(int*)arg[0]);
          f->eax=exit_status;
          break;
      case SYS_CREATE:
          get_argument(esp,arg,2);
          for(i=0;i<2;i++)
              check_address(arg[i]);
          check_address(*(char**)arg[0]);
          lock_acquire(&file_rw);
          rt=filesys_create(*(char**)arg[0],*(int32_t*)arg[1]);
          lock_release(&file_rw);
          f->eax=rt;
          break;
      case SYS_REMOVE: 
          get_argument(esp,arg,1);
          check_address(arg[0]);
          check_address(*(char**)arg[0]);
          lock_acquire(&file_rw);
          rt=filesys_remove(*(char**)arg[0]);
          lock_release(&file_rw);
          f->eax=rt;
          break;
      case SYS_OPEN:
          get_argument(esp,arg,1);
          check_address(arg[0]);
          check_address(*(char**)arg[0]);
          lock_acquire(&file_rw);
          new_file=filesys_open(*(char**)arg[0]);
          lock_release(&file_rw);
          if(new_file==NULL)
          {
              f->eax=-1;
              break;
          }

          new_file_data=calloc(1,sizeof(struct file_data));
          if(new_file_data==NULL)
          {
              f->eax=-1;
              break;
          }
          lock_acquire(&(t->sync.fd_lock));

          new_file_data->fd=t->sync.fd_gen;
          new_file_data->file=new_file;
          list_push_back(&(t->sync.file_list),&(new_file_data->elem));
          (t->sync.fd_gen)++;

          lock_release(&(t->sync.fd_lock));
          f->eax=new_file_data->fd;
          break;
      case SYS_FILESIZE:
          get_argument(esp,arg,1);
          check_address(arg[0]);
          new_file=search_file(*(int*)arg[0]);
          if(new_file==NULL)
          {
              f->eax=-1;
              break;
          }
          f_size=file_length(new_file);
          f->eax=f_size;
          break;
      case SYS_READ: //Project 1
          get_argument(esp,arg,3);
          for(i=0;i<3;i++) 
              check_address(arg[i]);
          check_address(*(void**)arg[1]);
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
          else
          {
              lock_acquire(&(file_rw));
              new_file=search_file(*(int*)arg[0]);
              if(new_file!=NULL)
              {
                  f_size=file_read(new_file,*(void**)arg[1],*(int*)arg[2]);
                  f->eax=f_size;
              }
              else
                  f->eax=-1;
              lock_release(&(file_rw));
          }
          break;
      case SYS_WRITE: //Project 1
          get_argument(esp,arg,3);
          for(i=0;i<3;i++)
              check_address(arg[i]);
          check_address(*(void**)arg[1]);
          if(*(int*)arg[0]==1)
              putbuf(*(char**)(arg[1]),*(int*)(arg[2]));
          else
          {
              lock_acquire(&(file_rw));
              new_file=search_file(*(int*)arg[0]);
              if(new_file!=NULL)
              {
                  f_size=file_write(new_file,*(void**)arg[1],*(int*)arg[2]);
                  f->eax=f_size;
              }
              else
                  f->eax=-1;
              lock_release(&(file_rw));
          }
          break;
      case SYS_SEEK:
          get_argument(esp,arg,2);
          for(i=0;i<2;i++)
              check_address(arg[i]);
          new_file=search_file(*(int*)arg[0]);
          if(new_file!=NULL)
          {
              file_seek(new_file,*(off_t*)arg[1]);
          }
          break;
      case SYS_TELL:
          get_argument(esp,arg,1);
          check_address(arg[0]);
          new_file=search_file(*(int*)arg[0]);
          if(new_file!=NULL)
          {
              uint_f=file_tell(new_file);
              f->eax=uint_f;
          }
          break;
      case SYS_CLOSE:
          check_address(arg[0]);
          lock_acquire(&(file_rw));
          remove_fd(*(int*)arg[0]);
          lock_release(&(file_rw));
          break;
      case SYS_MMAP:
          get_argument(esp,arg,2);
          for(i=0;i<2;i++)
              check_address(arg[i]);
          check_address(*(void**)arg[1]);
          new_file=search_file(*(int*)arg[0]);
          if(new_file!=NULL && *(int*)arg[0] > 1)
          {
              f->eax=mmap(new_file,*(uint8_t**)arg[1]);
          }
          else
              f->eax=-1;
          break;
      case SYS_MUNMAP:
          get_argument(esp,arg,1);
          check_address(arg[0]);
          munmap(*(int*)arg[0]);
          break;
      case PIBONACCI:
          get_argument(esp,arg,1);
          check_address(arg[0]);
          f->eax=pibonacci(*(int*)arg[0]);
          break;
      case SUM_OF_FOUR_INTEGERS:
          get_argument(esp,arg,4);
          for(i=0;i<3;i++)
              check_address(arg[i]);
          f->eax=sum_of_four_integers(*(int*)arg[0],*(int*)arg[1],*(int*)arg[2],*(int*)arg[3]);
          break;
  };
}
