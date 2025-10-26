#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched/signal.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/highmem.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/rcupdate.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vibhu Bhardwaj");
MODULE_DESCRIPTION("memory_manager");

static int pid = -1;
module_param(pid, int, 0444);
MODULE_PARM_DESC(pid, "PID of the process to inspect (int)");

static unsigned long long addr = 0;
module_param(addr, ullong, 0444);
MODULE_PARM_DESC(addr, "Virtual address to translate (unsigned long long)");

static void print_phys(int pid_in, unsigned long long vaddr, unsigned long long phys_addr)
{
	printk(KERN_INFO "[CSE330-Memory-Manager] PID [%d]: virtual address [%llx]  physical address [%llx] swap identifier [NA]\n",
	       pid_in, vaddr, phys_addr);
}

static void print_swap(int pid_in, unsigned long long vaddr, unsigned long long swap_id)
{
	printk(KERN_INFO "[CSE330-Memory-Manager] PID [%d]: virtual address [%llx]  physical address [NA] swap identifier [%llx]\n",
	       pid_in, vaddr, swap_id);
}

static void print_invalid(int pid_in, unsigned long long vaddr)
{
	printk(KERN_INFO "[CSE330-Memory-Manager] PID [%d]: virtual address [%llx]  physical address [NA] swap identifier [NA]\n",
	       pid_in, vaddr);
}

static int __init memory_manager_init(void)
{
	struct task_struct *task;
	struct task_struct *found_task = NULL;
	struct mm_struct *mm;
	unsigned long long vaddr;
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte_ptr;
	pte_t pte_entry;
	bool found = false;

	if (pid < 0)
		return -EINVAL;

	vaddr = addr;

	rcu_read_lock();
	for_each_process(task) {
		if (task->pid == pid) {
			found_task = task;
			found = true;
			get_task_struct(found_task);
			break;
		}
	}
	rcu_read_unlock();

	if (!found || !found_task) {
		print_invalid(pid, vaddr);
		return 0;
	}

	mm = found_task->mm;
	if (!mm) {
		put_task_struct(found_task);
		print_invalid(pid, vaddr);
		return 0;
	}

	pgd = pgd_offset(mm, (unsigned long)vaddr);
	if (!pgd || pgd_none(*pgd) || unlikely(pgd_bad(*pgd))) {
		put_task_struct(found_task);
		print_invalid(pid, vaddr);
		return 0;
	}

	p4d = p4d_offset(pgd, (unsigned long)vaddr);
	if (!p4d || p4d_none(*p4d) || unlikely(p4d_bad(*p4d))) {
		put_task_struct(found_task);
		print_invalid(pid, vaddr);
		return 0;
	}

	pud = pud_offset(p4d, (unsigned long)vaddr);
	if (!pud || pud_none(*pud) || unlikely(pud_bad(*pud))) {
		put_task_struct(found_task);
		print_invalid(pid, vaddr);
		return 0;
	}

	pmd = pmd_offset(pud, (unsigned long)vaddr);
	if (!pmd || pmd_none(*pmd) || unlikely(pmd_bad(*pmd))) {
		put_task_struct(found_task);
		print_invalid(pid, vaddr);
		return 0;
	}

	pte_ptr = pte_offset_kernel(pmd, (unsigned long)vaddr);
	if (!pte_ptr) {
		put_task_struct(found_task);
		print_invalid(pid, vaddr);
		return 0;
	}

	pte_entry = *pte_ptr;
	if (pte_none(pte_entry)) {
		put_task_struct(found_task);
		print_invalid(pid, vaddr);
		return 0;
	}

	if (pte_present(pte_entry)) {
		unsigned long long pfn;
		unsigned long long phys_page_base;
		unsigned long long offset;
		unsigned long long phys_addr;

		pfn = (unsigned long long)pte_pfn(pte_entry);
		phys_page_base = pfn << PAGE_SHIFT;
		offset = vaddr & ~PAGE_MASK;
		phys_addr = phys_page_base | offset;

		put_task_struct(found_task);
		print_phys(pid, vaddr, phys_addr);
		return 0;
	}

	{
		swp_entry_t swp = pte_to_swp_entry(pte_entry);
		if (swp.val) {
			put_task_struct(found_task);
			print_swap(pid, vaddr, (unsigned long long)swp.val);
		} else {
			put_task_struct(found_task);
			print_invalid(pid, vaddr);
		}
	}

	return 0;
}

static void __exit memory_manager_exit(void)
{
}

module_init(memory_manager_init);
module_exit(memory_manager_exit);
