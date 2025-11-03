#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched/signal.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/swap.h>
#include <linux/swapops.h>

MODULE_LICENSE("GPL");

static int pid = -1;
module_param(pid, int, 0444);

static unsigned long long addr = 0;
module_param(addr, ullong, 0444);

static int __init memory_manager_init(void)
{
	struct task_struct *task;
	struct mm_struct *mm;
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	unsigned long vaddr;

	for_each_process(task) {
		if (task->pid == pid) {
			mm = task->mm;
			if (!mm)
				goto invalid;

			vaddr = (unsigned long)addr;

			pgd = pgd_offset(mm, vaddr);
			if (pgd_none(*pgd) || unlikely(pgd_bad(*pgd)))
				goto invalid;

			p4d = p4d_offset(pgd, vaddr);
			if (p4d_none(*p4d) || unlikely(p4d_bad(*p4d)))
				goto invalid;

			pud = pud_offset(p4d, vaddr);
			if (pud_none(*pud) || unlikely(pud_bad(*pud)))
				goto invalid;

			pmd = pmd_offset(pud, vaddr);
			if (pmd_none(*pmd) || unlikely(pmd_bad(*pmd)))
				goto invalid;

			pte = pte_offset_kernel(pmd, vaddr);
			if (!pte)
				goto invalid;

			if (pte_none(*pte)) {
				printk(KERN_INFO "[CSE330-Memory-Manager] PID [%d]: virtual address [%llx]  physical address [NA] swap identifier [NA]\n",
				       pid, addr);
			} else if (pte_present(*pte)) {
				unsigned long long phys_addr = ((unsigned long long)pte_pfn(*pte) << PAGE_SHIFT) | (addr & ~PAGE_MASK);
				printk(KERN_INFO "[CSE330-Memory-Manager] PID [%d]: virtual address [%llx]  physical address [%llx] swap identifier [NA]\n",
				       pid, addr, phys_addr);
			} else {
				swp_entry_t entry = pte_to_swp_entry(*pte);
				printk(KERN_INFO "[CSE330-Memory-Manager] PID [%d]: virtual address [%llx]  physical address [NA] swap identifier [%llx]\n",
				       pid, addr, (unsigned long long)entry.val);
			}
			return 0;
		}
	}

invalid:
	printk(KERN_INFO "[CSE330-Memory-Manager] PID [%d]: virtual address [%llx]  physical address [NA] swap identifier [NA]\n",
	       pid, addr);
	return 0;
}

static void __exit memory_manager_exit(void)
{
}

module_init(memory_manager_init);
module_exit(memory_manager_exit);
