#include <stdio.h>
#include <syscall-nr.h>
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "process.h"

#define CODE_PHYS_BASE 0x08048000

static void syscall_handler (struct intr_frame *);
int sys_write (int fd, void *buffer, unsigned size);
tid_t sys_exec (const char *cmd_line);
int sys_wait (tid_t pid);
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

  // callNo == SYS_OPEN || callNo == SYS_REMOVE || callNo == SYS_EXIT || SYS_CREATE || SYS_WRITE
  if(callNo != SYS_HALT){
    user_esp++;
    if(!is_valid_memory_access(t->pagedir, user_esp)){
      sys_exit(-1);
    }

    arg1 = (uint32_t)(*user_esp);
  }

  //Below if is for sys calls that needs atleast 2 arguments
  //callNo == SYS_CREATE || callNo == SYS_WRITE
  // i.e filter out calls that needs only one arguments
  if( callNo != SYS_HALT && callNo != SYS_OPEN && callNo != SYS_REMOVE && callNo != SYS_EXIT && callNo != SYS_EXEC && callNo != SYS_WAIT){

    user_esp++;
    if(!is_valid_memory_access(t->pagedir, user_esp)){
      sys_exit(-1);
    }
    arg2 = (uint32_t)(*user_esp);
  }

  //Below if statement is for sys calls that needs atleast 3 arguements
  // ie filter out SYS_HALT, SYS_OPEN, SYS_REMOVE, SYS_EXIT
  //And use it only for
  //callNo == SYS_WRITE
  if( callNo != SYS_HALT && callNo != SYS_OPEN && callNo != SYS_REMOVE && callNo != SYS_EXIT && callNo != SYS_EXEC && callNo != SYS_WAIT && callNo != SYS_CREATE ){
    user_esp++;
    if(!is_valid_memory_access(t->pagedir, user_esp)){
      sys_exit(-1);
    }

    arg3 = (uint32_t)(*user_esp);
  }

  switch(callNo){

  case SYS_HALT:
    //shut down machine
    shutdown_power_off();
    break;

  case SYS_EXEC:
    f->eax = sys_exec((char *)arg1);
    break;

  case SYS_WAIT:
    f->eax = sys_wait((tid_t)arg1);
    break;

  case SYS_WRITE: //called to output to a file or STDOUT

    f->eax = sys_write((int)arg1, (char *)arg2, (unsigned )arg3);
    break;

  case SYS_EXIT:
    
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

/*  Waits for a child process pid and retrieves the child's exit status.
If pid is still alive, waits until it terminates. Then, returns the status that pid passed to exit. If pid did not call exit(), but was terminated by the kernel (e.g. killed due to an exception), wait(pid) must return -1. It is perfectly legal for a parent process to wait for child processes that have already terminated by the time the parent calls wait, but the kernel must still allow the parent to retrieve its child's exit status, or learn that the child was terminated by the kernel.

  wait must fail and return -1 immediately if any of the following conditions is true:

  pid does not refer to a direct child of the calling process. pid is a direct child of the calling process if and only if the calling process received pid as a return value from a successful call to exec.
  Note that children are not inherited: if A spawns child B and B spawns child process C, then A cannot wait for C, even if B is dead. A call to wait(C) by process A must fail. Similarly, orphaned processes are not assigned to a new parent if their parent process exits before they do.

																															   The process that calls wait has already called wait on pid. That is, a process may wait for any given child at most once.
																 Processes may spawn any number of children, wait for them in any order, and may even exit without having waited for some or all of their children. Your design should consider all the ways in which waits can occur. All of a process's resources, including its struct thread, must be freed whether its parent ever waits for it or not, and regardless of whether the child exits before or after its parent.

You must ensure that Pintos does not terminate until the initial process exits. The supplied Pintos code tries to do this by calling process_wait() (in userprog/process.c) from main() (in threads/init.c). We suggest that you implement process_wait() according to the comment at the top of the function and then implement the wait system call in terms of process_wait().

Implementing this system call requires considerably more work than any of the rest.
*/
int sys_wait (tid_t pid){
  return process_wait(pid);
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

/* Runs the executable whose name is given in cmd_line, passing any given arguments, and returns the new process's program id (pid).
 Must return pid -1, which otherwise should not be a valid pid, if the program cannot load or run for any reason.
 Thus, the parent process cannot return from the exec until it knows whether the child process successfully loaded its executable. 
 You must use appropriate synchronization to ensure this.*/
tid_t sys_exec (const char *cmd_line){
  return process_execute (cmd_line);
}


