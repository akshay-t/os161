#include <proc.h>
#include <kern/errno.h>
#include <lib.h>
#include <types.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <thread.h>
#include <curthread.h>
#include <test.h>
#include <syscall.h>
#include <machine/trapframe.h>
#include <vnode.h>
#include "opt-synchprobs.h"



void proctable_bootstrap()
{	int i;
	proctable_lock = lock_create("proctable lock");
	for (i=1;i<100;i++)
		proct[i]= kmalloc(sizeof(struct proc*));
}

struct proc * add_process(struct proc *parent)
{
	
/*
	for (i=1;i<100;i++)
	{	if(proct[i]==NULL)
			pid = i;
		break;

	}
	if(pid ==0)
		return NULL;
*/	
	
	struct proc *newp=kmalloc(sizeof(struct proc));
	
	if (parent->pid==0)
	{	newp->pid =1;

          	newp->parent= NULL;

	}
	else
	{
		newp->pid =parent->pid + 1;
	
		newp->parent= parent;
	
	}
	
	newp->status = 1;
	

	proct[newp->pid] = newp;

	return newp;
	

}

int remove_process(pid_t pid)
{
	if(!(pid>=1 && pid < 100) || proct[pid]==NULL)
		return EINVAL;

	struct proc *victim = proct[pid];
	kfree(victim);

	proct[pid]=NULL;
	return 0;
}

int pid_exists(pid_t pid)
{
        if(pid>=1 && pid<100 && proct[pid]!=NULL)
        	return 1;
        else 
		return 0;
}
struct proc* get_process(pid_t pid)
{
       return proct[pid];
}

