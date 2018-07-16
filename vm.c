#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <curthread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/spl.h>
#include <machine/tlb.h>
#include <vfs.h>
#include <uio.h>
#include <vnode.h>
#include <sfs.h>
#include <bitmap.h>
/*
 * Dumb MIPS-only "VM system" that is intended to only be just barely
 * enough to struggle off the ground. You should replace all of this
 * code while doing the VM assignment. In fact, starting in that
 * assignment, this file is not included in your kernel!
 */

/* under dumbvm, always have 48k of user stack */
#define DUMBVM_STACKPAGES    12

int coremapstatus=0; 
int victim=0,evicti=0;
struct vnode *swap;
struct bitmap *disk_map;

void swap_bootstrap()
{	char *n=kstrdup("lhd0raw:");
	        
	//kprintf("\nswap bootstraping");


	int err=vfs_open(n,O_RDWR,&swap);
	
	if(err)
		kprintf("\nError in swap file open");	

	disk_map = bitmap_create(1200);

}

void swap_inout(struct addrspace *as,paddr_t pa, vaddr_t sa,enum uio_rw mode){
        int result,i=0;
        struct iovec iov;
        struct uio ku;
        vaddr_t va=PADDR_TO_KVADDR(pa);
	
	(void)sa;

        mk_kuio(&ku, &iov, va,PAGE_SIZE, mode);        

        if(mode==UIO_READ)
        {        
		result=VOP_READ(swap,&ku);
        }
        else
        {       for (i = 0; i < array_getnum(as->pt); i++){

			pagetable *p = array_getguy(coremapt[i]->as->pt, i);

               		if(coremapt[i]->paddr==p->paddr)
                        {       p->valid=0;
                                p->paddr=0;
                                break;
                        }
                        

                }


                result=VOP_WRITE(swap, &ku);

        }


	if (result) {
		panic("VOP_ops ERROR:%d",result);
        }                

        i=TLB_Probe(va,pa);        

        TLB_Write(TLBHI_INVALID(i), TLBLO_INVALID(),i);
}

void evictpage(){
	int i=evicti;
	
	while(coremapt[i]->used!=1)
	{	i+=coremapt[i]->blocklen;
		i=i%coremap_size; 
	}
		
	evicti=i+1%coremap_size;
	
	kprintf("\ncame to swap");
	
	
}

int pagecount(paddr_t addr){
	int i=0;
	for(i=0;i<coremap_size;i++)
	{
		if(coremapt[i]->paddr==addr)
			return coremapt[i]->blocklen;
	}

}

void
vm_bootstrap(void)
{	int i=0;
	u_int32_t firstpaddr = 0,lastpaddr = 0;

	// swap_bootstrap();

	ram_getsize(&firstpaddr, &lastpaddr);

	coremap_size = (lastpaddr-firstpaddr)/PAGE_SIZE;
	coremapt = kmalloc(sizeof(struct coremap*) * coremap_size);


	if (coremapt == NULL) {
		panic("\ncoremap table: unable to create\n");
	}
	
	
	for (i = 0; i < coremap_size; i++) {
		struct coremap *entry = kmalloc(sizeof(struct coremap));
		entry->paddr = firstpaddr + (i * PAGE_SIZE);
		entry->used = 0;
		entry->as = NULL;
		entry->blocklen=-1;
		coremapt[i] = entry;
	}
	
        //kprintf("\n1st size last: %d %d %d\n", firstpaddr, coremap_size,lastpaddr);     
	
	// Get used ram
	ram_getsize(&firstpaddr, &lastpaddr);

	// Fill up the core map with the ram used
	for(i = 0; coremapt[i]->paddr < firstpaddr; i++) {
		coremapt[i]->used = 1;
	}
	

	coremapstatus=1;
	swap_bootstrap();

	//kprintf("init coremap: %d %d %d\n", firstpaddr, coremap_size,lastpaddr);


}

paddr_t
getppages(unsigned long npages)
{
	int spl;
	spl = splhigh();
	if (coremapstatus == 0)
		return ram_stealmem(npages);

	int i, j;
	unsigned int count=0;
	
//	kprintf("\n getting pages-  %d\n",npages);
	
	for (i = 0; i < coremap_size; i++) {
		if (coremapt[i]->used) {
			count = 0;
		} 
		else {
			count++;
		}
		if (count == npages) {
			coremapt[i - npages + 1]->blocklen = npages;
			for (j = i - npages + 1; j <= i; j++) {
				coremapt[j]->used = 1;
			}
			splx(spl);
			return coremapt[i - npages + 1]->paddr;
		}
	}
	
	// call for swap space if no physical memory
	i=evicti;
	evictpage();
	getppages(npages);
	splx(spl);
	return 0;
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t 
alloc_kpages(int npages)
{
	paddr_t pa;
	pa = getppages(npages);
	if (pa==0) {
		return 0;
	}
	return PADDR_TO_KVADDR(pa);
}

void 
free_kpages(vaddr_t addr)
{
	int i, j;
	
	paddr_t paddr=KVADDR_TO_PADDR(addr);
	
	for (i = 0; coremapt[i]->paddr != paddr; i++);
	
	assert(coremapt[i]->blocklen != -1);

	// kprintf("%d  - core-%d",paddr,coremapt[i]->paddr);
	for (j = 0; j < coremapt[i]->blocklen; j++) {
		coremapt[j]->used = 0;
	}
	
	coremapt[i]->blocklen = -1;

}

int tlb_victim(){ 
	int v;

	v= victim % NUM_TLB;
	victim++;
	return v; 
} 

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	u_int32_t ehi, elo;	
	int spl;
	paddr_t paddr;
	int i;	
	spl = splhigh();
	struct addrspace *as; 
	faultaddress &= PAGE_FRAME;

//	if(curthread->t_vmspace == NULL){
//		return EFAULT;
//	}

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);  

        switch (faulttype) {
            case VM_FAULT_READONLY:
                /* We always create pages read-write, so we can't get this */
                panic("dumbvm: got VM_FAULT_READONLY\n");
            case VM_FAULT_READ:
            case VM_FAULT_WRITE:
                break;         
            default:
                splx(spl);
                return EINVAL;
        }
	as = curthread->t_vmspace;


 	if (as == NULL) {
                /*
                 * No address space set up. This is probably a kernel
                 * fault early in boot. Return EFAULT so as to panic
                 * instead of getting into an infinite faulting loop.
                 */
                return EFAULT;
        }

        /* Assert that the address space has been set up properly. */
        /*assert(as->as_vbase1 != 0);
        assert(as->as_pbase1 != 0);
        assert(as->as_npages1 != 0);
        assert(as->as_vbase2 != 0);
        assert(as->as_pbase2 != 0);
        assert(as->as_npages2 != 0);
        assert(as->as_stackpbase != 0);
        assert((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
        assert((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
        assert((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
        assert((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
        assert((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);

        */

 	struct pagetable * e;
        for (i = 0; i < array_getnum(as->pt); i++)
        {
                e = (struct pagetable *) array_getguy(as->pt, i); 

                if (e->vaddr == faultaddress)
                {        
                        if(e->valid == 0)
                        {

                                paddr = getppages(1);
                                if (paddr == 0)
                                {
                                        return ENOMEM;
                                }
                                e->paddr = paddr;
                                e->valid = 1;

                                if (as->as_pbase1 == 0 && faultaddress >= as->as_vbase1 && faultaddress < as->as_vbase1 + as->as_npages1 * PAGE_SIZE)
                                {
                                        as->as_pbase1 = paddr;
                                }
                                if (as->as_pbase2 == 0 && faultaddress >= as->as_vbase2 && faultaddress < as->as_vbase2 + as->as_npages2 * PAGE_SIZE)
                                {
                                        as->as_pbase2 = paddr;
                                }
                                if (as->as_stackpbase == 0 && faultaddress >= USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE && faultaddress < USERSTACK)
                                {
 					as->as_stackpbase = paddr;
                                }
                                //DEBUG(DB_VM, "VM: Allocated 0x%x at physical address 0x%x\n", faultaddress, paddr);
                        }
                        else
                        {

                                paddr = e->paddr;
                        }
                        break;
                }
        }


        ehi = faultaddress;
        elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
 	if (TLB_Probe(ehi, elo) >= 0)                 
        {
                i = TLB_Probe(ehi, elo);         
        }
        else
        {
                i = tlb_victim();
        }

	//DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
        TLB_Write(ehi, elo, i);  
        splx(spl);
        return 0;                

}


