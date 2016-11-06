#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#define COMMAND_LINE 128

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);
/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  struct thread *child;
  struct thread *cur=thread_current();
  tid_t tid;
  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);
  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (file_name, PRI_DEFAULT, start_process, fn_copy);
  
  if(tid!=TID_ERROR)
  {
      cur->sync.exec_file=NULL;
      child=search_thread(tid);
      cur=thread_current();
      child->sync.parent=thread_current()->tid;
      list_push_back(&(cur->sync.child_list),&(child->sync.elem));
      //thread push (child)
  }
  if (tid == TID_ERROR)
    palloc_free_page (fn_copy); 
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  struct thread* parent;
  struct thread* cur=thread_current();
  bool success;
  parent=search_thread(thread_current()->sync.parent);
  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_name, &if_.eip, &if_.esp);
  /* If load failed, quit. */
  palloc_free_page (file_name);
  if (!success)
  {
      cur->sync.exit_status=-1;
      sema_up(&(cur->sync.exec));
      thread_exit ();
  }
  else
  {
      sema_up(&(cur->sync.exec));
  }
  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid) 
{
    struct thread* child;
    struct thread* cur;
    int exit;
    cur=thread_current();
    child=search_thread(child_tid);
    int old_level = intr_disable();
  
    //1. check if tid is really thread_current()'s child
    if(search_thread(child_tid)==NULL)
    {
        exit=-1;
        intr_set_level(old_level);
    }
    else if(is_child(child_tid)==NULL)
    {
        exit=-1;
        intr_set_level(old_level);
    }
    else
    {
        intr_set_level(old_level);
        sema_down(&(child->sync.wait));//parent should wait until child exit(lock here)
        exit=child->sync.exit_status;
        if(!list_empty(&(cur->sync.child_list)))
            list_remove(&(child->sync.elem));//if not empty
        sema_up(&(child->sync.exit));//child should wait until parent takes it's exit_status(unlock here)
    }

    return exit;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  struct thread* parent;
  uint32_t *pd;
  char* token;
  char* save_ptr;
  enum intr_level old_level;
  struct list_elem *e;
  struct list_elem *pop;
  struct thread *undead;
  struct file_data* cur_file;
  parent=search_thread(cur->sync.parent);
  sema_up(&(cur->sync.wait));//parent should wait until child exit(unlock here)
  token=strtok_r(cur->name," \t\n",&save_ptr);
  printf("%s: exit(%d)\n",token,cur->sync.exit_status);
  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
/*  old_level = intr_disable ();
  if(!list_empty(&(cur->sync.file_list)))
  {
      e=list_begin(&(cur->sync.file_list));
      while(e!=NULL)
      {
          e=list_next(e);
          pop=list_pop_front(&cur->sync.file_list);
          
          cur_file=list_entry(e,struct file_data,elem);
          if(cur_file->file!=NULL)
          //    file_close(cur_file->file);
          free(cur_file);
      }
  }
  intr_set_level (old_level);*/
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }

  old_level = intr_disable ();
  if(!list_empty(&(cur->sync.child_list)))
  {
      for(e=list_begin(&(cur->sync.child_list));e!=list_end(&(cur->sync.child_list));e=list_next(e))
      {
          undead=list_entry(e,struct thread,sync.elem);
          sema_up(&(undead->sync.exit));
          list_remove(e);
      }
  }
  intr_set_level (old_level);

  sema_down(&(cur->sync.exit));//child should wait until parent takes it's exit_status(lock here)
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofset;
  bool success = false;
  int i;
  char *token,*save_ptr;//for strtok_r;
  int *argv[20];//argv pointer sets
  int argc;//argument counter
  char fn_copy[COMMAND_LINE];
  //char test_name[128]="/bin/ls -l fo bar";
  int word_align=0;
  char fn_exe[COMMAND_LINE];
  char temp[COMMAND_LINE];
  int arglen=0;
  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL)
    goto done;
  process_activate ();
  /* Open executable file.*/
  strlcpy(temp,file_name,strlen(file_name)+1);
  token=strtok_r(temp," \t\n",&save_ptr);
  strlcpy(fn_exe,token,strlen(token)+1);
  file = filesys_open (fn_exe);
  /*Add file to cur thread exec_file*/
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }
  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }
  /* Read program headers.*/ 
  file_ofset = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofset < 0 || file_ofset > file_length (file))
        goto done;
      file_seek (file, file_ofset);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofset += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }
  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /*Argument Parsing and add arguments to STACK*/
  strlcpy(fn_copy,file_name,strlen(file_name)+1);
  //strlcpy(fn_copy,test_name,strlen(test_name)+1);
  argc=0;
  token=NULL;
  save_ptr=NULL;
  word_align=0;
  //printf("PHYS_BASE: %d\n",(int)esp);//FOR DEBUG: DON'T FORGET TO ERASE
  for(token=strtok_r(fn_copy," \n\t",&save_ptr);token!=NULL;token=strtok_r(NULL," \n\t",&save_ptr))
  {
      arglen=strlen(token)+1;
      *esp=(void*)((uintptr_t)*esp-arglen);//move ESP
      word_align+=arglen;
      strlcpy((char*)*esp,token,arglen);
      //printf("ESP: %d argv[]: %s\n",(int)*esp,(char*)*esp);//FOR DEBUG: DON'T FORGET TO ERASE
      argv[argc]=(int*)*esp;
      argc++;
  }//(type: char*)
  argv[argc]=NULL;//argv[argc]==NULL which is C standard
  if(word_align%4!=0)
  {
      for(i=0;i<(4-(word_align%4));i++)
      {
          *esp=(void*)((uintptr_t)*esp-1);//move ESP
          *(uint8_t*)*esp=0;//word_align PUSH
          //printf("ESP: %d word_align: %u\n",(int)*esp,*(uint8_t*)*esp);//FOR DEBUG: DON'T FORGET TO ERASE
      }
  }
  /*In STACK, arguments are not in right-to-left order. While pushing argv[], consider the order(right to left)*/
  for(i=argc;i>=0;i--)
  {
      *esp=(void*)((uintptr_t)*esp-4);//move ESP
      *(int**)*esp=argv[i];
      //printf("ESP: %d argv[%d]: %d\n",(int)*esp,i,*(int*)*esp);//FOR DEBUG: DON'T FORGET TO ERASE
  }//argv[] PUSH(type:char *)
  *esp=(void*)((uintptr_t)*esp-4);//move ESP
  *(char***)*esp=(char**)((int)*esp+4);//argv PUSH(type: char**)
  //printf("ESP: %d argv: %d\n",(int)*esp,*(int*)*esp);//FOR DEBUG: DON'T FORGET TO ERASE
  *esp=(void*)((uintptr_t)*esp-4);//move ESP
  *(int*)*esp=argc;//argc PUSH(type: int)
  //printf("ESP: %d argc: %d\n",(int)*esp,*(int*)*esp);//FOR DEBUG: DON'T FORGET TO ERASE
  *esp=(void*)((uintptr_t)*esp-4);//move ESP
  //printf("ESP: %d\n",(int)*esp);//FOR DEBUG: DON'T FORGET TO ERASE
  *(void**)*esp=(void(*)())0;//fake return address PUSH which is C standard
  //hex_dump((int)*esp,*esp,64,true);//FOR DEBUG: DON'T FORGET TO ERASE
  //printf("\n-------------------------------------------\n");//FOR DEBUG: DON'T FORGET TO ERASE

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  file_close (file);
  return success;
}
/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *knpage = palloc_get_page (PAL_USER);
      if (knpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, knpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (knpage);
          return false; 
        }
      memset (knpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, knpage, writable)) 
        {
          palloc_free_page (knpage);
          return false; 
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE;
      else
        palloc_free_page (kpage);
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *th = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (th->pagedir, upage) == NULL
          && pagedir_set_page (th->pagedir, upage, kpage, writable));
}
