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
MODULE_AUTHOR("CSE330");
MODULE_DESCRIPTION("memory_manager");
MODULE_VERSION("0.1");

static int pid = -1;
static unsigned long long addr = 0;

module_param(pid, int, 0444);
MODULE_PARM_DESC(pid, "PID");
module_param(addr, ullong, 0444);
MODULE_PARM_DESC(addr, "virtual address");

static void print_result(int pid_val, unsigned long long vaddr, int phys_ok, unsigned long long phys, int swap_ok, unsigned long long swapid)
{
    if (phys_ok)
        printk(KERN_INFO "[CSE330-Memory-Manager] PID [%d]: virtual address [%llx]  physical address [%llx] swap identifier [NA]\n", pid_val, vaddr, phys);
    else if (swap_ok)
        printk(KERN_INFO "[CSE330-Memory-Manager] PID [%d]: virtual address [%llx]  physical address [NA] swap identifier [%llx]\n", pid_val, vaddr, swapid);
    else
        printk(KERN_INFO "[CSE330-Memory-Manager] PID [%d]: virtual address [%llx]  physical address [NA] swap identifier [NA]\n", pid_val, vaddr);
}

static int __init memory_manager_init(void)
{
    struct pid *p_struct;
    struct task_struct *task;
    struct mm_struct *mm;
    if (pid < 0)
        return -EINVAL;
    p_struct = find_get_pid(pid);
    if (!p_struct)
        return -ESRCH;
    task = pid_task(p_struct, PIDTYPE_PID);
    put_pid(p_struct);
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
            goto out;
        p4d = p4d_offset(pgd, address);
        if (p4d_none(*p4d) || unlikely(p4d_bad(*p4d)))
            goto out;
        pud = pud_offset(p4d, address);
        if (pud_none(*pud) || unlikely(pud_bad(*pud)))
            goto out;
        pmd = pmd_offset(pud, address);
        if (pmd_none(*pmd) || unlikely(pmd_bad(*pmd)))
            goto out;
        pte = pte_offset_kernel(pmd, address);
        if (!pte) {
            print_result(pid, addr, 0, 0, 0, 0);
            goto out;
        }
        if (pte_present(*pte)) {
            unsigned long pfn = pte_pfn(*pte);
            unsigned long phys_hi = (pfn << PAGE_SHIFT);
            unsigned long offset = address & (PAGE_SIZE - 1);
            unsigned long long phys_addr = (unsigned long long)phys_hi | (unsigned long long)offset;
            print_result(pid, addr, 1, phys_addr, 0, 0);
            goto out;
        } else {
            swp_entry_t swp = pte_to_swp_entry(*pte);
            if (swp.val) {
                unsigned long long swapid = (unsigned long long)swp.val;
                print_result(pid, addr, 0, 0, 1, swapid);
                goto out;
            } else {
                print_result(pid, addr, 0, 0, 0, 0);
                goto out;
            }
        }
    }
out:
    up_read(&mm->mmap_sem);
    mmput(mm);
    return 0;
}

static void __exit memory_manager_exit(void)
{
}

module_init(memory_manager_init);
module_exit(memory_manager_exit);
