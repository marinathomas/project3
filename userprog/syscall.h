#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
void sys_exit (int status);
bool is_valid_memory_access(uint32_t *pd, const void *vaddr );

#endif /* userprog/syscall.h */
