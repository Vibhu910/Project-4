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
#include <linux/rcupdate.h>

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
	printk(KERN_INFO "[CSE330-Memory-Manager] PID [%d]: virtual address [%llx]  physical address [%llx] swap identifier [NA]\n",
	       pid_in, (unsigned long long)vaddr, (unsigned long long)phys_addr);
}

static void print_swap(int pid_in, unsigned long vaddr, unsigned long swap_id)
{
	printk(KERN_INFO "[CSE330-Memory-Manager] PID [%d]: virtual address [%llx]  physical address [NA] swap identifier [%llx]\n",
	       pid_in, (unsigned long long)vaddr, (unsigned long long)swap_id);
}

static void print_invalid(int pid_in, unsigned long vaddr)
{
	printk(KERN_INFO "[CSE330-Memory-Manager] PID [%d]: virtual address [%llx]  physical address [NA] swap identifier [NA]\n",
	       pid_in, (unsigned long long)vaddr);
}

static int __init memory_manager_init(void)
{
	struct task_struct *task;
	struct task_struct *found_task = NULL;
	struct mm_struct *mm;
	unsigned long address;
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte_ptr;
	pte_t pte_entry;
	bool found = false;

	if (pid < 0) {
		return -EINVAL;
	}

	address = (unsigned long)addr;

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
		print_invalid(pid, address);
		return 0;
	}

	mm = found_task->mm;
	if (!mm) {
		put_task_struct(found_task);
		print_invalid(pid, address);
		return 0;
	}

	pgd = pgd_offset(mm, address);
	if (!pgd || pgd_none(*pgd) || unlikely(pgd_bad(*pgd))) {
		put_task_struct(found_task);
		print_invalid(pid, address);
		return 0;
	}

	p4d = p4d_offset(pgd, address);
	if (!p4d || p4d_none(*p4d) || unlikely(p4d_bad(*p4d))) {
		put_task_struct(found_task);
		print_invalid(pid, address);
		return 0;
	}

	pud = pud_offset(p4d, address);
	if (!pud || pud_none(*pud) || unlikely(pud_bad(*pud))) {
		put_ta_
