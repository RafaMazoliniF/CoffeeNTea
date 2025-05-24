#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched/signal.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/sched/clock.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fdtable.h>
#include <linux/net.h>
#include <net/sock.h>
#include <linux/socket.h>

#define PROC_NAME "mod2"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Grupo 2");
MODULE_DESCRIPTION("Modulo 2: Sistema de Pontuacao de Comportamento de Processos");

struct proc_cpu_data {
    pid_t pid;
    u64 last_exec_runtime;
    u64 last_time;
    struct list_head list;
};

static LIST_HEAD(proc_cpu_list);
static DEFINE_MUTEX(proc_cpu_mutex);

static struct proc_cpu_data *get_proc_data(pid_t pid) {
    struct proc_cpu_data *entry;

    list_for_each_entry(entry, &proc_cpu_list, list) {
        if (entry->pid == pid)
            return entry;
    }

    entry = kmalloc(sizeof(*entry), GFP_KERNEL);
    if (!entry)
        return NULL;

    entry->pid = pid;
    entry->last_exec_runtime = 0;
    entry->last_time = ktime_get_ns(); 
    list_add(&entry->list, &proc_cpu_list);

    return entry;
}

static int mod2_show(struct seq_file *m, void *v)
{
    struct task_struct *task;
    u64 now_time = sched_clock(); 

    seq_printf(m, "%-8s | %-5s | %-9s | %-9s | %-9s | %-7s | %-5s | %-5s | %s\n", "PID", "'%'CPU", "SYSCALLS", "Input", "Output", "Sockets", "Prio", "Risco", "Score");
    seq_printf(m, "---------+--------+-----------+-----------+-----------+---------+-------+--------+--------\n");

    for_each_process(task) {
        u64 now_exec_runtime = task->se.sum_exec_runtime;
        unsigned long cpu_percent = 0;

        mutex_lock(&proc_cpu_mutex);
        struct proc_cpu_data *pdata = get_proc_data(task->pid);
        if (pdata) {
            if (pdata->last_exec_runtime > 0) { 
                u64 delta_exec = now_exec_runtime - pdata->last_exec_runtime;
                u64 delta_time = now_time - pdata->last_time;

                if (delta_time > 0)
                    cpu_percent = (delta_exec * 1000) / delta_time; // Ta em nanossegundos por isso multiplica por 1000
                    cpu_percent /= 10;
                //seq_printf(m, "PID: %d delta_exec: %llu delta_time: %llu cpu_percent: %lu\n", task->pid, delta_exec, delta_time, cpu_percent);
            }

            pdata->last_exec_runtime = now_exec_runtime;
            pdata->last_time = now_time;
        }
        mutex_unlock(&proc_cpu_mutex);

        unsigned long syscalls = task->nvcsw + task->nivcsw;
        unsigned long long read_bytes = task->ioac.read_bytes;
        unsigned long long write_bytes = task->ioac.write_bytes;
        int num_sockets = 0;
        struct files_struct *files = task->files;
        int prioridade = task->prio;

        if (files) {
            struct fdtable *fdt = files_fdtable(files);
            for (int i = 0; i < fdt->max_fds; i++) {
                struct file *f = fdt->fd[i];

                if (f && S_ISSOCK(file_inode(f)->i_mode)) {
                    num_sockets++;
                }
            }
        }

        unsigned long input = read_bytes / 1000000;
        unsigned long output = write_bytes / 1000000;
        int score = 0;

        if (cpu_percent >= 80) {
            score += 2;
        } else if (cpu_percent > 30) {
            score += 1;
        }

        if (syscalls > 1000) {
            score += 2;
        } else if (syscalls > 100) {
            score += 1;
        }

        if (input >= 10){
            score += 2;
        } else if (input >= 1){
            score += 1;
        }
        if (output >= 10){
            score += 2;
        } else if (output >= 1){
            score += 1;
        }

        if (num_sockets > 10){
            score += 2;
        } else if (num_sockets >= 4){
            score += 1;
        }

        if (prioridade >= 62 && prioridade < 139) {
            score += 1;
        } else if (prioridade >= 42 && prioridade < 62) {
            score += 2;
        } else if (prioridade >= 0 && prioridade < 42) {
            score += 3;
        }

        const char *risco;
        if (score >= 10)
            risco = "Alto";
        else if (score >= 6)
            risco = "Medio";
        else
            risco = "Baixo";

        seq_printf(m, "%-8d | %-5lu | %-10lu | %-9lu | %-9lu | %-7d | %-5d | %-5s | %d\n",
                    task->pid, cpu_percent, syscalls, input, output, num_sockets, prioridade, risco, score);
    }

    return 0;
}

static int mod2_open(struct inode *inode, struct file *file)
{
    return single_open(file, mod2_show, NULL);
}

static const struct proc_ops mod2_fops = {
    .proc_open = mod2_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init mod2_init(void)
{
    proc_create(PROC_NAME, 0, NULL, &mod2_fops);
    printk(KERN_INFO "Modulo 2: iniciado\n");
    return 0;
}

static void __exit mod2_exit(void)
{
    struct proc_cpu_data *entry, *tmp;
    remove_proc_entry(PROC_NAME, NULL);
    mutex_lock(&proc_cpu_mutex);
    list_for_each_entry_safe(entry, tmp, &proc_cpu_list, list) {
        list_del(&entry->list);
        kfree(entry);
    }
    mutex_unlock(&proc_cpu_mutex);
    printk(KERN_INFO "Modulo 2: encerrado\n");
}

module_init(mod2_init);
module_exit(mod2_exit);
