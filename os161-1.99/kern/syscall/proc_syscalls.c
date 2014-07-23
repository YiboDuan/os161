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
extern struct semaphore *proc_list_mutex;
//extern struct semaphore *fork_synch;

struct fork_pack {
    struct trapframe *tf;
    struct addrspace *as;
    struct semaphore *synch;
};


#endif


void sys__exit(int exitcode) {
  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
#if OPT_A2
  p->exitcode = exitcode;
  P(proc_list_mutex);
    if(proc_list[p->parent_pid] != NULL) {
        p->exited = true;
        cv_signal(p->waitcv, NULL);
    } else {
        cv_destroy(p->waitcv);
        proc_list[p->pid] = NULL;
    }
  V(proc_list_mutex);
#endif
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
  *retval = curproc->pid;
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
    int err = 0;
    struct fork_pack *pack = kmalloc(sizeof(struct fork_pack *));
    //create new process
    struct proc *new_proc = proc_create_runprogram(curproc->p_name);
    new_proc->parent_pid = curproc->pid;
    
    // create and copy new address space
    err = as_copy(curproc->p_addrspace,&(pack->as));
    if (err) {
        proc_destroy(new_proc);
        return err;
    }
    
    // create and copy new trap frame
    pack->tf = (struct trapframe *)kmalloc(sizeof(struct trapframe*));
    if(pack->tf == NULL) {
        proc_destroy(new_proc);
        kfree(pack->as);
        return ENOMEM;
    }
    memcpy(pack->tf, tf, sizeof(struct trapframe *));
    
    //pack->synch = sem_create("synch", 0);
    
    
    void (*entry_func)(void*, unsigned long) = &child_entry;
    
    //P(pack->synch);
    //V(pack->synch);
    err = thread_fork(curthread->t_name, new_proc, entry_func, pack, 0);
    struct lock *dummy = lock_create("dummy lock");
    lock_acquire(dummy);
    cv_wait(fork_synch, dummy);
    *retval = new_proc->pid;
    kprintf("WTF\n");
    return 0;
}

void
child_entry(void* arg1, unsigned long arg2) {
   
    struct fork_pack *pack = arg1;
    //P(pack->synch);
    struct trapframe *p_ntf = pack->tf;
    struct trapframe ntf;
    //struct addrspace *as;
    (void)arg2;

    curproc_setas(pack->as);
    as_activate();
    
    memcpy(&ntf, p_ntf, sizeof(struct trapframe));
    ntf.tf_v0 = 0;
    ntf.tf_a3 = 0;
    ntf.tf_epc += 4;
    cv_signal(fork_synch, NULL);
    kprintf("LOL\n");
    mips_usermode(&ntf);
    panic("child thread escaped usermode warp!\n");
}
#endif
