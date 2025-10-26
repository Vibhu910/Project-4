/*
 * memory_manager.c
 *
 * Kernel module that walks a process' page tables for a given PID and virtual
 * address, and reports the physical address if present, or the swap id if the
 * page is swapped out, or reports NA otherwise.
 *
 * Requirements from the assignment:
 *  - module name: memory_manager
 *  - module parameters: named "pid" (int) and "addr" (unsigned long long)
 *  - follow exact printk templates for outputs
 *
 * NOTE: This implementation follows the instructions provided (no semaphores / locks).
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/highmem.h>
#include <linux/swap.h>      /* include in this order */
#include <linux/swapops.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CSE330");
MODULE_DESCRIPTION("memory_manager");

/* Module parameters (names MUST be exactly 'pid' and 'addr') */
static int pid = -1;
module_param(pid, int, 0444);
MODULE_PARM_DESC(pid, "PID of the process to inspect");

static unsigned long long addr = 0;
module_param(addr, ullong, 0444);
MODULE_PARM_DESC(addr, "Virtual address to translate (unsigned long long)");

/* Helper to print exactly in required template when physical present */
static void print_phys(int pid_in, unsigned long long vaddr, unsigned long phys_addr)
{
	printk(KERN_INFO "[CSE330-Memory-Manager] PID [%d]: virtual address [%llx]  physical address [%lx] swap identifier [NA]\n",
	       pid_in, vaddr, phys_addr);
}

/* Helper to print exactly in required template when in swap */
static void print_swap(int pid_in, unsigned long long vaddr, unsigned long swap_id)
{
	printk(KERN_INFO "[CSE330-Memory-Manager] PID [%d]: virtual address [%llx]  physical address [NA] swap identifier [%lx]\n",
	       pid_in, vaddr, swap_id);
}

/* Helper to print exactly in required template when invalid / not available */
static void print_invalid(int pid_in, unsigned long long vaddr)
{
	printk(KERN_INFO "[CSE330-Memory-Manager] PID [%d]: virtual address [%llx]  physical address [NA] swap identifier [NA]\n",
	       pid_in, vaddr);
}

static int __init memory_manager_init(void)
{
	struct pid *pid_struct;
	struct task_struct *task;
	struct mm_struct *mm;
	unsigned long address;
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte_ptr;
	pte_t pte_entry;

	/* Validate input params */
	if (pid < 0) {
		pr_err("[CSE330-Memory-Manager] invalid pid parameter (must be non-negative)\n");
		return -EINVAL;
	}
	/* addr parameter may be 0; still attempt translation */

	/* find task from pid */
	pid_struct = find_get_pid(pid);
	if (!pid_struct) {
		/* can't resolve pid -> invalid */
		print_invalid(pid, addr);
		return 0;
	}
	task = pid_task(pid_struct, PIDTYPE_PID);
	put_pid(pid_struct);

	if (!task) {
		print_invalid(pid, addr);
		return 0;
	}

	/* get mm_struct */
	mm = task->mm;
	if (!mm) {
		/* kernel thread or mm not present */
		print_invalid(pid, addr);
		return 0;
	}

	/* perform page table walk */
	address = (unsigned long)addr;

	/* PGD */
	pgd = pgd_offset(mm, address);
	if (!pgd || pgd_none(*pgd) || unlikely(pgd_bad(*pgd))) {
		print_invalid(pid, addr);
		return 0;
	}

	/* P4D */
	p4d = p4d_offset(pgd, address);
	if (!p4d || p4d_none(*p4d) || unlikely(p4d_bad(*p4d))) {
		print_invalid(pid, addr);
		return 0;
	}

	/* PUD */
	pud = pud_offset(p4d, address);
	if (!pud || pud_none(*pud) || unlikely(pud_bad(*pud))) {
		print_invalid(pid, addr);
		return 0;
	}

	/* PMD */
	pmd = pmd_offset(pud, address);
	if (!pmd || pmd_none(*pmd) || unlikely(pmd_bad(*pmd))) {
		print_invalid(pid, addr);
		return 0;
	}

	/* PTE */
	pte_ptr = pte_offset_kernel(pmd, address);
	if (!pte_ptr) {
		print_invalid(pid, addr);
		return 0;
	}

	/* copy the pte entry */
	pte_entry = *pte_ptr;

	/* if pte has no mapping (treat as invalid) */
	/* Note: pte_none() checks for empty entry */
	if (pte_none(pte_entry)) {
		print_invalid(pid, addr);
		return 0;
	}

	/* If present in physical memory */
	if (pte_present(pte_entry)) {
		unsigned long pfn;
		unsigned long phys_page_base;
		unsigned long offset;
		unsigned long phys_addr;

		pfn = pte_pfn(pte_entry);
		phys_page_base = (pfn << PAGE_SHIFT);
		offset = address & (~PAGE_MASK);
		phys_addr = phys_page_base | offset;

		print_phys(pid, addr, phys_addr);
		return 0;
	}

	/* Not present: attempt to obtain swap entry */
	{
		swp_entry_t swp = pte_to_swp_entry(pte_entry);
		/* If this is a swap entry, print swap id (entry.val). Otherwise, treat as invalid */
		/* On many architectures pte_present==0 and pte is a swap entry */
		if (swp.val) {
			print_swap(pid, addr, (unsigned long)swp.val);
		} else {
			/* swap entry val == 0 might mean special; treat as invalid as per spec */
			print_invalid(pid, addr);
		}
	}

	return 0;
}

static void __exit memory_manager_exit(void)
{
	/* nothing special to clean up */
}

module_init(memory_manager_init);
module_exit(memory_manager_exit);
