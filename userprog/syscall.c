#include <stdio.h>
#include <syscall-nr.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "process.h"
#include "devices/shutdown.h"

#define CODE_PHYS_BASE 0x08048000

static struct lock file_lock;


static void syscall_handler (struct intr_frame *);
bool sys_create ( char *file, unsigned initial_size);
void sys_seek (int fd, unsigned position);
bool sys_remove ( char *file);
int sys_open ( char *file);
void sys_close( int fd);
int sys_filesize( int fd);
void sys_exit (int status);
int sys_wait (tid_t pid);
unsigned sys_tell (int fd);
tid_t sys_exec (const char *cmd_line);
int sys_write (int fd, void *buffer, unsigned size);
int sys_read (int fd, void *buffer, unsigned size);
static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);

void syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&file_lock);
}

static void syscall_handler (struct intr_frame *f UNUSED)
{
  //Ensure that esp is valid
  struct thread *t = thread_current ();

  if(!is_valid_memory_access(t->pagedir, f->esp)){
    sys_exit(-1);
  }
  uint32_t callNo;
  uint32_t *user_esp = f->esp;
  uint32_t arg1, arg2, arg3;

  callNo = (uint32_t)(*user_esp);

  // Only SYS_HALT needs no arguements
  if(callNo != SYS_HALT){
    user_esp++;
    if(!is_valid_memory_access(t->pagedir, user_esp)){
      sys_exit(-1);
    }
    
    arg1 = (uint32_t)(*user_esp);
  }

  //Below if is for sys calls that needs atleast 2 arguments
  if(callNo == SYS_CREATE || callNo == SYS_WRITE || callNo == SYS_READ || callNo == SYS_SEEK){
    user_esp++;
    if(!is_valid_memory_access(t->pagedir, user_esp)){
      sys_exit(-1);
    }
    arg2 = (uint32_t)(*user_esp);
  }

  //Below if statement is for sys calls that needs atleast 3 arguements
  if(callNo == SYS_WRITE || callNo == SYS_READ){
    user_esp++;
    if(!is_valid_memory_access(t->pagedir, user_esp)){
      sys_exit(-1);
    }
    
    arg3 = (uint32_t)(*user_esp);
  }
  
  switch(callNo){

  case SYS_HALT:
    shutdown_power_off();
    break;
    
  case SYS_CREATE:
    if((char *)arg1 == NULL){
      sys_exit(-1);
      break;
    }
    if(!is_valid_memory_access(t->pagedir, (void *)arg1)){
      sys_exit(-1);
    }
    
    f->eax =  sys_create ((char *)arg1, (unsigned)arg2);    
    break;

  case SYS_OPEN:
    if((char *)arg1 == NULL){
      sys_exit(-1);
      break;
    }
    
      if(!is_valid_memory_access(t->pagedir, (void *)arg1)){
      sys_exit(-1);
    }
    f->eax =  sys_open ((char *)arg1);
    break;

  case SYS_READ:
    if((char *)arg2 == NULL){
      sys_exit(-1);
      break;
    }

    if(!is_valid_memory_access(t->pagedir, (void *)arg2)){
      sys_exit(-1);
      break;
    }

    if((int)arg1 >= t->nextFd){
      sys_exit(-1);
    }

    f->eax = sys_read((int)arg1, (char *)arg2, (unsigned )arg3);
    break;
   
  case SYS_REMOVE:
       
    f->eax =  sys_remove ((char *)arg1);
    break;

  case SYS_CLOSE:

    if((int)arg1 >= t->nextFd){
      sys_exit(-1);
    }

    sys_close ((int)arg1);
    break;

  case SYS_FILESIZE:
       
    if((int)arg1 >= t->nextFd){
      sys_exit(-1);
    }

    f->eax = sys_filesize((int)arg1);
    break;
   
  case SYS_WRITE: //called to output to a file or STDOUT

    if(get_user((uint8_t *)arg2) == -1){
      sys_exit(-1);
      break;
     }
    if((int)arg1 >= t->nextFd){
      sys_exit(-1);
      break;
    }
    
    f->eax = sys_write((int)arg1, (char *)arg2, (unsigned )arg3);
    break;

  case SYS_TELL:
    f->eax = sys_tell ((int)arg1);
    break;

  case SYS_SEEK:
    sys_seek ((int)arg1, arg2);
    break;
    
  case SYS_EXIT:
    sys_exit(arg1);
    break;
    
  case SYS_EXEC:
    if((char *)arg1 == NULL){
      sys_exit(-1);
      break;
    }

      if(!is_valid_memory_access(t->pagedir, (void *)arg1)){
      sys_exit(-1);
    }
  
    f->eax = sys_exec((char *)arg1);
    break;

  case SYS_WAIT:
    f->eax = sys_wait((tid_t)arg1);
    break;

  default:
    sys_exit(-1);
  }
  
}

/* Reads a byte at user virtual address UADDR.
UADDR must be below PHYS_BASE.
Returns the byte value if successful, -1 if a segfault
occurred. */
static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}
/* Writes BYTE to user address UDST.
UDST must be below PHYS_BASE.
Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}

/*
checks for validity of the address
*/
bool is_valid_memory_access(uint32_t *pd, const void *vaddr ){
  if ( vaddr != NULL &&  vaddr < ((void *)LOADER_PHYS_BASE) && vaddr > ((void *)CODE_PHYS_BASE) && pagedir_get_page (pd, vaddr) != NULL){
    // if ( vaddr != NULL &&  vaddr < ((void *)LOADER_PHYS_BASE) && pagedir_get_page (pd, vaddr) != NULL){
    return true;
  } else {
    return false;
  }
}


/*Creates a new file called file initially initial_size bytes in size. Returns true if successful, false otherwise.
 Creating a new file does not open it: opening the new file is a separate operation which would require a open system call. */
bool sys_create (char *file, unsigned initial_size){
  lock_acquire (&file_lock);
  bool status = filesys_create ( file, initial_size);
  lock_release (&file_lock);
  return status;
}

/*  Deletes the file called file. Returns true if successful, false otherwise. A file may be removed regardless of whether it is open or closed,
    and removing an open file does not close it. See Removing an Open File, for details.
 */
bool sys_remove ( char *file){
  lock_acquire (&file_lock);
  bool status = filesys_remove ( file);
  lock_release (&file_lock);
  return status;
}


/* Opens the file called file. Returns a nonnegative integer handle called a "file descriptor" (fd), or -1 if the file could not be opened.
      File descriptors numbered 0 and 1 are reserved for the console: fd 0 (STDIN_FILENO) is standard input, fd 1 (STDOUT_FILENO) is standard output. The open system call will never return either of these file descriptors, which are valid as system call arguments only as explicitly described below.

      Each process has an independent set of file descriptors. File descriptors are not inherited by child processes.

      When a single file is opened more than once, whether by a single process or different processes, each open returns a new file descriptor. Different file descriptors for a single file are closed independently in separate calls to close and they do not share a file position.
*/
int sys_open ( char *name){
  lock_acquire (&file_lock);
  struct file *new_file = filesys_open (name);
  struct thread *curthread = thread_current();

  int cur_fd = -1;
  if(new_file != NULL){
    cur_fd = curthread->nextFd;
    curthread->nextFd = cur_fd +1;
    curthread->fd_table[cur_fd] = new_file;
    //curthread->curFile = new_file;
  }
  lock_release (&file_lock);
  return cur_fd;
}

/*  Reads size bytes from the file open as fd into buffer. Returns the number of bytes actually read (0 at end of file), or -1 if the file could not be read (due to a condition other than end of file). Fd 0 reads from the keyboard using input_getc().*/

int sys_read (int fd, void *buffer, unsigned size){
  int readVal = 0;
  if(fd > STDOUT_FILENO){
    struct thread *curthread = thread_current();
    struct file  *thisFile = curthread->fd_table[fd];
    lock_acquire (&file_lock);
    readVal =  file_read(thisFile, buffer, size);
    lock_release (&file_lock);
  }
  return readVal;
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
  if( fd == STDOUT_FILENO){
    //STDOUT
    putbuf(buffer, size);
    return size;
  }
  int retSize = 0;
  if(fd > STDOUT_FILENO){
    struct thread *curthread = thread_current();
    struct file  *wfile = curthread->fd_table[fd];
    lock_acquire (&file_lock);
    retSize = file_write (wfile, buffer, size);
    lock_release (&file_lock);
  }
  return retSize;
}

/* Closes file descriptor fd. Exiting or terminating a process implicitly closes all its open file descriptors, as if by calling this function for each one.*/
void sys_close (int fd){
  if (fd > STDOUT_FILENO){
    struct thread *curthread = thread_current();
    struct file  *fp = curthread->fd_table[fd];
    if( (struct file *)fp != NULL){
      lock_acquire (&file_lock);
      file_close(fp);
      lock_release (&file_lock);
      curthread->fd_table[fd] = NULL;
    }
  }
  
}

/* Returns the size, in bytes, of the file open as fd.*/
int sys_filesize (int fd){
  int fileSize = 0;
  if (fd > STDOUT_FILENO){
    struct thread *curthread = thread_current();
    struct file  *fp = curthread->fd_table[fd];
    lock_acquire (&file_lock);
    fileSize =  file_length (fp);
    lock_release (&file_lock);
  }
  return fileSize;
}
 
   
/* Runs the executable whose name is given in cmd_line, passing any given arguments, and returns the new process's program id (pid).
 Must return pid -1, which otherwise should not be a valid pid, if the program cannot load or run for any reason.
 Thus, the parent process cannot return from the exec until it knows whether the child process successfully loaded its executable. 
 You must use appropriate synchronization to ensure this.*/
tid_t sys_exec (const char *cmd_line){
  return process_execute (cmd_line);
}


/*Changes the next byte to be read or written in open file fd to position, expressed in bytes from the beginning of the file. (Thus, a position of 0 is the file's start.)
  A seek past the current end of a file is not an error. A later read obtains 0 bytes, indicating end of file. A later write extends the file, filling any unwritten gap with zeros. (However, in Pintos files have a fixed length until project 4 is complete, so writes past end of file will return an error.) These semantics are implemented in the file system and do not require any special effort in system call implementation.*/
void sys_seek (int fd, unsigned position){
  if (fd > STDOUT_FILENO){
    struct thread *curthread = thread_current();
    struct file  *fp = curthread->fd_table[fd];
    lock_acquire (&file_lock);
    file_seek(fp, position);
    lock_release (&file_lock);
  }

}

/*Returns the position of the next byte to be read or written in open file fd, expressed in bytes from the beginning of the file.*/
unsigned sys_tell (int fd){
  uint32_t postn = -1;
  if (fd > STDOUT_FILENO){
    struct thread *curthread = thread_current();
    struct file  *fp = curthread->fd_table[fd];
    lock_acquire (&file_lock);
    postn =  file_tell (fp);
    lock_release (&file_lock);
  }
  return postn;  
}


