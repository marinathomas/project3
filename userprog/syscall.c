#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "userprog/pagedir.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "devices/shutdown.h"

#define CODE_PHYS_BASE 0x08048000

static void syscall_handler (struct intr_frame *);
int sys_write (int fd, void *buffer, unsigned size);
void sys_exit (int status);
bool is_valid_memory_access(uint32_t *pd, const void *vaddr );

void syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler (struct intr_frame *f UNUSED)
{
  //printf ("system call!\n");
  //thread_exit ();
  
  //Ensure that esp is valid
  struct thread *t = thread_current ();
  
  if(!is_valid_memory_access(t->pagedir, f->esp)){
      sys_exit(-1);
  }
  uint32_t callNo;
  uint32_t *user_esp = f->esp;
  uint32_t arg1, arg2, arg3;
  
  callNo = (uint32_t)(*user_esp);
  //printf("call no %d",callNo);
  
  switch(callNo){

  case SYS_HALT:
    //shut down machine
    shutdown_power_off();
    break;

  case SYS_WRITE: //called to output to a file or STDOUT
    user_esp++;
    if(!is_valid_memory_access(t->pagedir, user_esp)){
      sys_exit(-1);
      break;
    }
    arg1 = (uint32_t)(*user_esp);//fd

    user_esp++;
    if(!is_valid_memory_access(t->pagedir, user_esp)){
      sys_exit(-1);
      break;
    }
    //arg2 = (uint32_t)(*user_esp);
    void* buffer = (void*)(*((int*)user_esp));
    if(buffer == NULL){
      sys_exit(-1);
      break;
    }

    user_esp++;
    if(!is_valid_memory_access(t->pagedir, user_esp)){
      sys_exit(-1);
      break;
    } 
    arg3 = (uint32_t)(*user_esp);
        
    f->eax = sys_write((int)arg1, (char *)buffer, (unsigned )arg3);
    break;
  case SYS_EXIT:
    user_esp++;
    if(!is_valid_memory_access(t->pagedir, user_esp)){
	sys_exit(-1);
	break;
    }
    arg1 = (uint32_t)(*user_esp); //exit status
    sys_exit(arg1);
    break;
  default:
    sys_exit(-1);
  }
  
}

/*
checks for validity of the address
*/
bool is_valid_memory_access(uint32_t *pd, const void *vaddr ){
  if ( vaddr != NULL &&  vaddr < ((void *)LOADER_PHYS_BASE) && vaddr > ((void *)CODE_PHYS_BASE) && pagedir_get_page (pd, vaddr) != NULL){
    return true;
  } else {
    return false;
  }
}

/*
Terminates the current user program, returning status to the kernel. 
If the process's parent waits for it (see below), this is the status that will be returned.
Conventionally, a status of 0 indicates success and nonzero values indicate errors.
*/
void sys_exit (int status){
  struct thread *curthread = thread_current();
  curthread->exit_status = status;
  printf ("%s: exit(%d)\n", curthread->name, status);
  thread_exit(); //cleanup and deallocation and waiting for parent to reap exit status
}


/*
Writes size bytes from buffer to the open file fd. Returns the number of bytes actually written, 
which may be less than size if some bytes could not be written.
Writing past end-of-file would normally extend the file, but file growth is not implemented by the basic file system. 
The expected behavior is to write as many bytes as possible up to end-of-file and return the actual number written, or 0 if no bytes could be written at all.
Fd 1 writes to the console. Your code to write to the console should write all of buffer in one call to putbuf(),
 at least as long as size is not bigger than a few hundred bytes. (It is reasonable to break up larger buffers.)
 Otherwise, lines of text output by different processes may end up interleaved on the console, confusing both human readers and our grading scripts.
**/
int sys_write (int fd, void *buffer, unsigned size){
  if( fd == 1){
    //STDOUT
    putbuf(buffer, size);
    return size;
  }
  return 0;
}
