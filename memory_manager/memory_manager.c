#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched/signal.h>
#include <linux/pid.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/moduleparam.h>
#include <linux/swap.h>
#include <linux/swapops.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CSE330 Memory Manager Example");
MODULE_DESCRIPTION("Walk page tables for a given pid and virtual address");
MODULE_VERSION("0.1");

static int pid = -1;
static unsigned long long addr = 0;

module_param(pid, int, 0444);
MODULE_PARM_DESC(pid, "PID of the process");
module_param(addr, ullong, 0444);
MODULE_PARM_DESC(addr, "Virtual address (unsigned long long)");

static void print_result_present(int pid_val, unsigned long long vaddr,
                                 unsigned long phys_valid, unsigned long long phys,
                                 unsigned long swap_valid, unsigned long long swapid)
{
    if (phys_valid) {
        printk(KERN_INFO "[CSE330-Memory-Manager] PID [%d]: virtual address [%llx]  physical address [%llx] swap identifier [NA]\n",
               pid_val, vaddr, phys);
    } else if (swap_valid) {
        printk(KERN_INFO "[CSE330-Memory-Manager] PID [%d]: virtual address [%llx]  physical address [NA] swap identifier [%llx]\n",
               pid_val, vaddr, swapid);
    } else {
        printk(KERN_INFO "[CSE330-Memory-Manager] PID [%d]: virtual address [%llx]  physical address [NA] swap identifier [NA]\n",
               pid_val, vaddr);
    }
}

static int __init memory_manager_init(void)
{
    struct pid *pid_struct = NULL;
    struct task_struct *task = NULL;
    struct mm_struct *mm = NULL;

    if (pid < 0)
        return -EINVAL;

    pid_struct = find_get_pid(pid);
    if (!pid_struct)
        return -ESRCH;

    task = pid_task(pid_struct, PIDTYPE_PID);
    put_pid(pid_struct);
    if (!task)
        return -ESRCH;

    mm = get_task_mm(task);
    if (!mm)
        return -EINVAL;

    down_read(&mm->mmap_sem);

    {
        unsigned long address = (unsigned long)addr;
        pgd_t *pgd;
        p4d_t *p4d;
        pud_t *pud;
        pmd_t *pmd;
        pte_t *pte;

        pgd = pgd_offset(mm, address);
        if (pgd_none(*pgd) || unlikely(pgd_bad(*pgd)))
            goto out_unlock;

        p4d = p4d_offset(pgd, address);
        if (p4d_none(*p4d) || unlikely(p4d_bad(*p4d)))
            goto out_unlock;

        pud = pud_offset(p4d, address);
        if (pud_none(*pud) || unlikely(pud_bad(*pud)))
            goto out_unlock;

        pmd = pmd_offset(pud, address);
        if (pmd_none(*pmd) || unlikely(pmd_bad(*pmd)))
            goto out_unlock;

        pte = pte_offset_kernel(pmd, address);
        if (!pte) {
            print_result_present(pid, addr, 0, 0, 0, 0);
            goto out_unlock;
        }

        if (pte_present(*pte)) {
            unsigned long pfn = pte_pfn(*pte);
            unsigned long phys_hi = (pfn << PAGE_SHIFT);
            unsigned long offset = address & (PAGE_SIZE - 1);
            unsigned long long phys_addr = (unsigned long long)phys_hi | (unsigned long long)offset;
            print_result_present(pid, addr, 1, phys_addr, 0, 0);
            goto out_unlock;
        } else {
            swp_entry_t swp = pte_to_swp_entry(*pte);
            if (swp.val) {
                unsigned long long swapid = (unsigned long long)swp.val;
                print_result_present(pid, addr, 0, 0, 1, swapid);
                goto out_unlock;
            } else {
                print_result_present(pid, addr, 0, 0, 0, 0);
                goto out_unlock;
            }
        }
    }

out_unlock:
    up_read(&mm->mmap_sem);
    mmput(mm);
    return 0;
}

static void __exit memory_manager_exit(void)
{
}

module_init(memory_manager_init);
module_exit(memory_manager_exit);

