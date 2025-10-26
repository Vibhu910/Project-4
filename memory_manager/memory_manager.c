#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/highmem.h>
#include <linux/swap.h>
#include <linux/swapops.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CSE330");
MODULE_DESCRIPTION("memory_manager");

static int pid = -1;
module_param(pid, int, 0444);
MODULE_PARM_DESC(pid, "PID of the process to inspect (int)");

static unsigned long addr = 0;
module_param(addr, ulong, 0444);
MODULE_PARM_DESC(addr, "Virtual address to translate (unsigned long)");

static void print_phys(int pid_in, unsigned long vaddr, unsigned long phys_addr)
{
	printk(KERN_INFO "[CSE330-Memory-Manager] PID [%d]: virtual address [%lx]  physical address [%lx] swap identifier [NA]\n",
	       pid_in, vaddr, phys_addr);
}

static void print_swap(int pid_in, unsigned long vaddr, unsigned long swap_id)
{
	printk(KERN_INFO "[CSE330-Memory-Manager] PID [%d]: virtual address [%lx]  physical address [NA] swap identifier [%lx]\n",
	       pid_in, vaddr, swap_id);
}

static void print_invalid(int pid_in, unsigned long vaddr)
{
	printk(KERN_INFO "[CSE330-Memory-Manager] PID [%d]: virtual address [%lx]  physical address [NA] swap identifier [NA]\n",
	       pid_in, vaddr);
}

static int __init memory_manager_init(void)
{
	struct task_struct *task;
	struct mm_struct *mm;
	unsigned long address;
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte_ptr;
	pte_t pte_entry;
	bool found = false;

	pr_info("[CSE330-Memory-Manager] module loaded: pid=%d addr=%lx\n", pid, addr);

	if (pid < 0) {
		pr_err("[CSE330-Memory-Manager] invalid pid parameter (must be non-negative)\n");
		return -EINVAL;
	}

	address = (unsigned long)addr;

	for_each_process(task) {
		if (task->pid == pid) {
			found = true;
			break;
		}
	}

	if (!found || !task) {
		print_invalid(pid, address);
		return 0;
	}

	mm = task->mm;
	if (!mm) {
		print_invalid(pid, address);
		return 0;
	}

	pgd = pgd_offset(mm, address);
	if (!pgd || pgd_none(*pgd) || unlikely(pgd_bad(*pgd))) {
		print_invalid(pid, address);
		return 0;
	}

	p4d = p4d_offset(pgd, address);
	if (!p4d || p4d_none(*p4d) || unlikely(p4d_bad(*p4d))) {
		print_invalid(pid, address);
		return 0;
	}

	pud = pud_offset(p4d, address);
	if (!pud || pud_none(*pud) || unlikely(pud_bad(*pud))) {
		print_invalid(pid, address);
		return 0;
	}

	pmd = pmd_offset(pud, address);
	if (!pmd || pmd_none(*pmd) || unlikely(pmd_bad(*pmd))) {
		print_invalid(pid, address);
		return 0;
	}

	pte_ptr = pte_offset_kernel(pmd, address);
	if (!pte_ptr) {
		print_invalid(pid, address);
		return 0;
	}

	pte_entry = *pte_ptr;
	if (pte_none(pte_entry)) {
		print_invalid(pid, address);
		return 0;
	}

	if (pte_present(pte_entry)) {
		unsigned long pfn;
		unsigned long phys_page_base;
		unsigned long offset;
		unsigned long phys_addr;
		pfn = pte_pfn(pte_entry);
		phys_page_base = (pfn << PAGE_SHIFT);
		offset = address & ~PAGE_MASK;
		phys_addr = phys_page_base | offset;
		print_phys(pid, address, phys_addr);
		return 0;
	}

	{
		swp_entry_t swp = pte_to_swp_entry(pte_entry);
		if (swp.val) {
			print_swap(pid, address, (unsigned long)swp.val);
		} else {
			print_invalid(pid, address);
		}
	}

	return 0;
}

static void __exit memory_manager_exit(void)
{
	pr_info("[CSE330-Memory-Manager] module unloaded\n");
}

module_init(memory_manager_init);
module_exit(memory_manager_exit);
