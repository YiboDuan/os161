#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <synch.h>
#include <copyinout.h>
#include <mips/trapframe.h>
#include <spl.h>
#include "opt-A2.h"

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */
#if OPT_A2
extern struct proc **proc_list;

struct fork_pack {
    struct trapframe *tf;
    struct addrspace *as;
    struct lock *lock;
    struct semaphore *synch;
};
#endif


void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  *retval = 1;
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}
#if OPT_A2

int
sys_fork(struct trapframe *tf, pid_t *retval) {
    int err;
    struct fork_pack *pack = kmalloc(sizeof(struct fork_pack *));
    //create new process
    struct proc *new_proc = proc_create_runprogram(curproc->p_name);
    new_proc->parent_pid = curproc->pid;
    
    // create and copy new address space
    err = as_copy(curproc_getas(),&(pack->as));
    if (err) {
        proc_destroy(new_proc);
        return err;
    }
    pack->synch = sem_create("fork synch", 0);
    // create and copy new trap frame
    pack->tf = kmalloc(sizeof(struct trapframe*));
    if(pack->tf == NULL) {
        proc_destroy(new_proc);
        kfree(pack->as);
        return ENOMEM;
    }
    memcpy(pack->tf,tf,sizeof(struct trapframe*));    
    pack->lock = lock_create("fork lock");
    
    void (*entry_func)(void*, unsigned long) = &child_entry;
    lock_acquire(pack->lock);
    err = thread_fork("child thread", new_proc, entry_func, pack, 0);
    lock_release(pack->lock);
    *retval = new_proc->pid;
    spllower(IPL_HIGH, IPL_NONE);
    P(pack->synch);
    return 0;
}

void
child_entry(void* arg1, unsigned long arg2) {
    struct fork_pack *pack = arg1;
    lock_acquire(pack->lock);
    (void)arg2;
    curproc->p_addrspace = pack->as;
    as_activate();
    struct trapframe *p_ntf = kmalloc(sizeof(struct trapframe*));
    memcpy(p_ntf, pack->tf, sizeof(struct trapframe*));
    struct trapframe ntf = *p_ntf;
    
    ntf.tf_v0 = 0;
    ntf.tf_a3 = 0;
    ntf.tf_epc += 4;
    
    lock_release(pack->lock);
    V(pack->synch);
    mips_usermode(&ntf);
    panic("child thread escaped usermode warp!\n");
}
#endif
