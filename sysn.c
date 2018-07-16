#include <types.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <synch.h>
#include <lib.h>
#include <addrspace.h>
#include <thread.h>
#include <curthread.h>
#include <vm.h>
#include <vfs.h>
#include <test.h>
#include <syscall.h>
#include <machine/trapframe.h>
#include <proc.h>


int sys_getpid(int* ret)
{     
    *ret = curthread->pid;    

        return 0;
}

struct fork_info{

	struct semaphore *sem;
	struct thread *parent;
	struct trapframe *tf;
	pid_t child_pid;

};

/*
void createchild(void * data1, unsigned long unused) {
	(void)unused;
	struct fork_info * info = data1;
	struct addrspace * new_as;
	struct trapframe new_tf;

	if (as_copy(info->parent->t_vmspace, &new_as)) {
		info->child_pid = ENOMEM; 
		V(info->sem);
		thread_exit();
	}
    
	curthread->t_vmspace = new_as;
	as_activate(new_as);

	memcpy(&new_tf, info->tf, sizeof(struct trapframe)); 
	
	new_tf.tf_v0 = 0;
	new_tf.tf_a3 = 0;
	new_tf.tf_epc += 4; 


	struct proc *new_process = add_process(curthread->t_proc);
	if (new_process == NULL) {
		info->child_pid = EAGAIN; 
		V(info->sem);
        kprintf("second step");
		thread_exit();
	}
    

	info->child_pid = new_process->pid;
	new_process->parent = info->parent->t_proc;


	curthread->t_proc = new_process;



	V(info->sem);

	mips_usermode(&new_tf);


	panic("start_child returned\n");
}
*/

int sys_fork(struct trapframe* tf, int* ret)
{
	int error=0;
    	struct thread *child = NULL;

    
 
	 struct trapframe *parent_tf_copy = kmalloc(sizeof(struct trapframe));
   
    
    	if (parent_tf_copy == NULL) {
        	kfree(parent_tf_copy);
        	return ENOMEM;
    	}
//    	int s;
//    	s = splhigh();
    	memcpy(parent_tf_copy,tf,sizeof(struct trapframe));
//    	splx(s);
    
	child =kmalloc(sizeof(struct thread));
	
	struct proc *new_process = add_process(curthread->t_proc);  // get  pid for new process
    
	child->ppid = new_process->parent->pid;

        child->pid = new_process->pid;
	
	child->t_proc=new_process;
	
	// kprintf("\nchild %ld parent %ld ",child->pid,child->ppid);
	

 //   	error = thread_fork(curthread->t_name, (void*)parent_tf_copy, 0, md_forkentry, &child);
  
        error = thread_fork(curthread->t_name, parent_tf_copy, 0,md_forkentry, &child);


  
    	*ret = child->pid; //return child pid
    
    	return error;
}
int sys_waitpid(pid_t pid, int *status, int options, int *retval)
{
    (void) options;
    int stat, result;
    
    /*index into process table*/
    struct proc* p = get_process(pid);
    if(p == NULL)
    {
        *retval = -1;
        return EINVAL;
    }
    
    /*acquire the exit lock*/
    lock_acquire(p->exitlock);
    pid_t my_pid = curthread->pid;
    
    /*
 *      * Copy user supplied status pointer using copyin
 *           */
   
     result = copyin((userptr_t)status,&stat,sizeof(status));
    
    //kprintf("check 3\n");

    if(result && curthread->ppid != 0)
    {
        lock_release(p->exitlock);
        *retval = -1;
        return result;
    }
    
    if(!pid_exists(pid) || pid <= my_pid)
    {
        lock_release(p->exitlock);
        *retval = -1;
        return EINVAL;
    }
    
    /*kprintf("check 5\n");
 *     status is not valid 
    */
    
	if(status == NULL)
    {
        lock_release(p->exitlock);
        *retval = -1;
        return EINVAL;
    }
    
    /*kprintf("check 6\n");
 
    if(options == 1)
    {
        *retval = 0;
        lock_release(p->exitlock);
        return 0;
    }
    
    kprintf("check 7\n");
 *     we can't handle other options*/
    if(options > 1)
    {
        *retval = -1;
        lock_release(p->exitlock);
        return EINVAL;
    }
    
    /*kprintf("checkpoint8\n");
 *     wait for the child to report exit status*/
    cv_wait(p->exitcv, p->exitlock);
    
    *status = p->exit_code;
    
    /*kprintf("checkpoint8\n");*/
    *retval = pid;
    
    lock_release(p->exitlock);
    
    /*free the pid slot*/
    remove_process(pid);
    return 0;
}


#if OPT_DUMBVM
int sys_sbrk(int size, int *retval)
{
    *retval=-1;
    return 1;
}
 
        
  
int sys__exit(int code)
{
 
   /* pid_t pid = curthread->pid;
    struct process *parent = NULL;
    struct process* p = get_process(pid);
    if(p != NULL)
    {
        lock_acquire(p->exitlock);
        p->exited = 1;
        p->exitcode = code;
        cv_broadcast(p->exitcv, p->exitlock);
        lock_release(p->exitlock);
    }*/
    
    /*kprintf("\n Thread exitted %d\n", curthread->pid);*/
    thread_exit();
    
    panic("I shouldn't be here in sys__exit!");
    
    return 0;
}

#endif
