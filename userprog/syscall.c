#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "filesys/filesys.h"
#include "userprog/pagedir.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
//#include "process.h"
#include "devices/shutdown.h"

#define CODE_PHYS_BASE 0x08048000

static void syscall_handler (struct intr_frame *);
bool sys_create ( char *file, unsigned initial_size);
bool sys_remove ( char *file);
int sys_open ( char *file);
void sys_close( int fd);
int sys_filesize( int fd);
void sys_exit (int status);
int sys_write (int fd, void *buffer, unsigned size);
int sys_read (int fd, void *buffer, unsigned size);
bool is_valid_memory_access(uint32_t *pd, const void *vaddr );
static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);

void syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
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
  // if( (callNo != SYS_HALT) && (callNo != SYS_OPEN) && (callNo != SYS_REMOVE) && (callNo != SYS_EXIT) && callNo != SYS_CLOSE && CALL ){
  if(callNo == SYS_CREATE || callNo == SYS_WRITE || callNo == SYS_READ){
    user_esp++;
    if(!is_valid_memory_access(t->pagedir, user_esp)){
      sys_exit(-1);
    }
    arg2 = (uint32_t)(*user_esp);
  }

  //Below if statement is for sys calls that needs atleast 3 arguements
  // ie filter out SYS_HALT, SYS_OPEN, SYS_REMOVE, SYS_EXIT
  //And use it only for
  //callNo == SYS_WRITE, SYS_READ
  // if( callNo != SYS_HALT && callNo != SYS_OPEN && callNo != SYS_REMOVE && callNo != SYS_EXIT && callNo != SYS_CREATE ){
  if(callNo == SYS_WRITE || callNo == SYS_READ){
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
    
  case SYS_CREATE:
    if((char *)arg1 == NULL){
      sys_exit(-1);
      break;
    }
    f->eax =  sys_create ((char *)arg1, (unsigned)arg2);    
    break;

  case SYS_OPEN:
    if((char *)arg1 == NULL){
      sys_exit(-1);
      break;
    }
    f->eax =  sys_open ((char *)arg1);
    break;

  case SYS_READ:
    if(arg2 == NULL){
      sys_exit(-1);
      break;
    }

    if(arg3 !=0 && !put_user((uint8_t *)arg2,'a')){
      sys_exit(-1);
      break;
    }

    f->eax = sys_read((int)arg1, (char *)arg2, (unsigned )arg3);
    break;
   
  case SYS_REMOVE:
       
    f->eax =  sys_remove ((char *)arg1);
    break;

  case SYS_CLOSE:
       
    sys_close (arg1);
    break;

  case SYS_FILESIZE:
       
    f->eax = sys_filesize(arg1);
    break;
   
  case SYS_WRITE: //called to output to a file or STDOUT

     if(get_user((uint8_t *)arg2) == -1){
      sys_exit(-1);
      break;
     }
      
    f->eax = sys_write((int)arg1, (char *)arg2, (unsigned )arg3);
    break;
    
  case SYS_EXIT:
    //uint32_t status = (uint32_t)(arg3); //exit status
    sys_exit(arg1);
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
    return true;
  } else {
    return false;
  }
}


/*Creates a new file called file initially initial_size bytes in size. Returns true if successful, false otherwise.
 Creating a new file does not open it: opening the new file is a separate operation which would require a open system call. */
bool sys_create (char *file, unsigned initial_size){
  bool status = filesys_create ( file, initial_size);
  return status;
}

/*  Deletes the file called file. Returns true if successful, false otherwise. A file may be removed regardless of whether it is open or closed,
    and removing an open file does not close it. See Removing an Open File, for details.
 */
bool sys_remove ( char *file){
  bool status = filesys_remove ( file);
  return status;
}


/* Opens the file called file. Returns a nonnegative integer handle called a "file descriptor" (fd), or -1 if the file could not be opened.
      File descriptors numbered 0 and 1 are reserved for the console: fd 0 (STDIN_FILENO) is standard input, fd 1 (STDOUT_FILENO) is standard output. The open system call will never return either of these file descriptors, which are valid as system call arguments only as explicitly described below.

      Each process has an independent set of file descriptors. File descriptors are not inherited by child processes.

      When a single file is opened more than once, whether by a single process or different processes, each open returns a new file descriptor. Different file descriptors for a single file are closed independently in separate calls to close and they do not share a file position.
*/
int sys_open ( char *name){
  struct file * new_file = filesys_open (name);
  struct thread *curthread = thread_current();

  int cur_fd = -1;
  if( (struct file *)new_file != NULL){
    cur_fd = curthread->nextFd;
    curthread->nextFd = cur_fd +1;
    curthread->fd_table[cur_fd] = *new_file;
  }
  return cur_fd;
}

/*  Reads size bytes from the file open as fd into buffer. Returns the number of bytes actually read (0 at end of file), or -1 if the file could not be read (due to a condition other than end of file). Fd 0 reads from the keyboard using input_getc().*/

int sys_read (int fd, void *buffer, unsigned size){
  if(fd >2){
    struct thread *curthread = thread_current();
    struct file  thisFile = curthread->fd_table[fd];
    return file_read((struct file *)(&thisFile), buffer, size);
  }
  return NULL;
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

/* Closes file descriptor fd. Exiting or terminating a process implicitly closes all its open file descriptors, as if by calling this function for each one.*/
void sys_close (int fd){
  if (fd > 2){
   struct thread *curthread = thread_current();
   struct file  thisFile = curthread->fd_table[fd];
   file_close ((struct file *)&thisFile);
   //curthread->fd_table[fd] = NULL;
  }
  
}

/* Returns the size, in bytes, of the file open as fd.*/
int sys_filesize (int fd){
  if (fd > 2){
    struct thread *curthread = thread_current();
    struct file  thisFile = curthread->fd_table[fd];
    return file_length ((struct file *)&thisFile);
    //curthread->fd_table[fd] = NULL;
  }
  return 0;
}
 
  
