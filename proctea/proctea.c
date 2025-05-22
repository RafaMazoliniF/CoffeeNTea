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

// Função para exibir as estatisticas e determinar o risco de um processo 
static int mod2_show(struct seq_file *m, void *v)
{
    struct task_struct *task;

seq_printf(m, "%-8s | %-5s | %-9s | %-9s | %-9s | %-7s | %s\n", "PID", "'%'CPU", "SYSCALLS", "Input", "Output", "Sockets", "Risco");
seq_printf(m, "---------+-----+-----------+-----------+-----------+----+--------\n");

    for_each_process(task) {
        unsigned long cpu_usage = task->se.sum_exec_runtime;  
        unsigned long syscalls = task->nvcsw + task->nivcsw; 
        u64 atual_time = sched_clock();
        unsigned long long read_bytes = task->ioac.read_bytes;
        unsigned long long write_bytes = task->ioac.write_bytes;
        int num_sockets = 0;
        struct files_struct *files = task->files;

        if (files) {
            struct fdtable *fdt = files_fdtable(files);
            for (int i = 0; i < fdt->max_fds; i++) {
                struct file *f = fdt->fd[i];

                if (f && S_ISSOCK(file_inode(f)->i_mode)) {
                    num_sockets++;
                }

            }
        }

        

        unsigned long cpu_percent = (cpu_usage * 1000) / atual_time;
        unsigned long input = read_bytes / 1000000;
        unsigned long output = write_bytes / 1000000;
        int score = 0;

        // Considerando que cpu > 80% 
        if (cpu_percent >= 80) {
            score += 2;
        } else if (cpu_percent < 80 && cpu_percent > 30){
            score += 1;
        }
        
        // Se syscalls > 1000/s 
        if (syscalls > 1000) {
            score += 2;
        } else if (syscalls <= 1000 && syscalls > 100) {
            score += 1;
        }

        // I/O > 10MB/s
        if (input >= 10){
            score += 2;
        } else if (input < 10 && input >= 1){
            score += 1;
        }
        if (output >= 10){
            score += 2;
        } else if (output < 10 && output >= 1){ 
            score += 1;
        }
    
        // numero de sockets abertos > 10
        if (num_sockets > 10){
            score += 2;
        } else if (num_sockets >= 4 || num_sockets <= 10)
        {
            score += 1;
        }
        

        const char *risco;
        if (score >= 10)
            risco = "Alto";
        else if (score >= 6)
            risco = "Medio";
        else
            risco = "Baixo";

        seq_printf(m, "%-8d | %-5lu | %-9lu | %-9lu | %-9lu | %-7d | %s\n", task->pid, cpu_percent, syscalls, input, output, num_sockets, risco);

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
    remove_proc_entry(PROC_NAME, NULL);
    printk(KERN_INFO "Modulo 2: encerrado\n");
}

module_init(mod2_init);
module_exit(mod2_exit);
