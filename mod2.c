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
#include <linux/in.h>

#define PROC_NAME "mod2"
#define BOLD_BLUE     "\x1b[1;34m"
#define RESET     "\x1b[0m"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Grupo 2");
MODULE_DESCRIPTION("Modulo 2: Sistema de Pontuacao de Comportamento de Processos");

// Estrutura para armazenar dados temporais de cada processo
struct proc_cpu_data {
    pid_t pid;
    u64 last_exec_runtime; // Tempo de CPU acumulado na última leitura
    u64 last_time;         // Timestamp da última leitura
    struct list_head list; // Para encadear na lista global
};

static LIST_HEAD(proc_cpu_list);
static DEFINE_MUTEX(proc_cpu_mutex);

// Busca ou cria uma entrada `proc_cpu_data` para um dado PID.
static struct proc_cpu_data *get_proc_data(pid_t pid) {
    struct proc_cpu_data *entry;

    // Tenta encontrar uma entrada existente.
    list_for_each_entry(entry, &proc_cpu_list, list) {
        if (entry->pid == pid)
            return entry;
    }

    // Se não existir, cria uma nova.
    entry = kmalloc(sizeof(*entry), GFP_KERNEL);
    if (!entry)
        return NULL;

    entry->pid = pid;
    entry->last_exec_runtime = 0;
    entry->last_time = ktime_get_ns();
    list_add(&entry->list, &proc_cpu_list);

    return entry;
}

// Função principal que gera o conteúdo para o arquivo /proc/mod2.
// Itera sobre todos os processos, coleta métricas e calcula uma pontuação de risco.
static int mod2_show(struct seq_file *m, void *v)
{
    struct task_struct *task;
    u64 now_time = sched_clock(); // Tempo atual do scheduler, para cálculo de delta de CPU

    seq_puts(m, "\033[2J\033[H"); // Limpa a tela do terminal

    seq_printf(m, BOLD_BLUE "%-8s | %-5s | %-7s | %-9s | %-9s | %-9s | %-7s | %-5s | %-5s | %-5s | %-6s | %s\n" RESET,
               "PID", "%CPU", "Mem(KB)", "Syscalls", "Input(MB)", "Output(MB)", "Sockets", "TCP", "UDP", "Prio", "Risco", "Score");
    seq_printf(m, "---------+-------+---------+-----------+-----------+-------------+---------+-------+-------+-------+--------+-------\n");

    rcu_read_lock(); // Necessário para for_each_process de forma segura
    for_each_process(task) {
        u64 now_exec_runtime = task->se.sum_exec_runtime;
        unsigned long cpu_percent = 0;

        // Uso de CPU desde a última leitura.
        mutex_lock(&proc_cpu_mutex);
        struct proc_cpu_data *pdata = get_proc_data(task->pid);
        if (pdata) {
            if (pdata->last_exec_runtime > 0) {
                u64 delta_exec = now_exec_runtime - pdata->last_exec_runtime;
                u64 delta_time = now_time - pdata->last_time;

                if (delta_time > 0)
                    cpu_percent = (delta_exec * 100) / delta_time;
            }
            pdata->last_exec_runtime = now_exec_runtime;
            pdata->last_time = now_time;
        }
        mutex_unlock(&proc_cpu_mutex);

        // Coleta de outras métricas: syscalls, I/O, prioridade.
        unsigned long syscalls = task->nvcsw + task->nivcsw;
        unsigned long long read_bytes = task->ioac.read_bytes;
        unsigned long long write_bytes = task->ioac.write_bytes;
        int prioridade = task->prio;

        // Get memory usage (RSS in MB)
        unsigned long mem_usage_mb = 0;
        if (task->mm) { 
            mem_usage_mb = get_mm_rss(task->mm) * (PAGE_SIZE / 1024);
            mem_usage_mb /= 1024;
        }

        // Contagem de sockets abertos pelo processo (TCP e UDP).
        int num_sockets = 0;
        int num_sockets_tcp = 0;
        int num_sockets_udp = 0;
        struct files_struct *files = task->files;
        if (files) {
            spin_lock(&files->file_lock); // Protege o acesso à tabela de arquivos
            struct fdtable *fdt = files_fdtable(files); // files_fdtable é seguro com o lock
            if (fdt) {
                for (int i = 0; i < fdt->max_fds; i++) {
                    struct file *f = fdt->fd[i];
                    if (f && S_ISSOCK(file_inode(f)->i_mode)) {
                        struct socket *sock = SOCKET_I(file_inode(f));
                        if (sock && sock->sk) {
                            if (sock->sk->sk_protocol == IPPROTO_TCP) num_sockets_tcp++;
                            else if (sock->sk->sk_protocol == IPPROTO_UDP) num_sockets_udp++;
                        }
                        num_sockets++;
                    }
                }
            }
            spin_unlock(&files->file_lock);
        }


        unsigned long input = read_bytes / 1048576; // Input em MB (divide by 2^20)
        unsigned long output = write_bytes / 1048576; // Output em MB (divide by 2^20)
        int score = 0;

        // Scores.
        if (cpu_percent >= 80) score += 2; 
        else if (cpu_percent > 30) score += 1;

        if (syscalls > 1000) score += 2; 
        else if (syscalls > 100) score += 1;

        if (input >= 10) score += 2; 
        else if (input >= 1) score += 1;

        if (output >= 10) score += 2;
        else if (output >= 1) score += 1;

        if (num_sockets > 10) score += 2; 
        else if (num_sockets >= 4) score += 1;

        if (prioridade >= 62 && prioridade < 139) score += 1;
        else if (prioridade >= 42 && prioridade < 62) score += 2;
        else if (prioridade >= 0 && prioridade < 42) score += 3;

        if (mem_usage_mb >= 100) score += 2;
        else if (mem_usage_mb < 100 && mem_usage_mb >= 10) score += 1;

        // Classificação do risco (Alto, Medio, Baixo)
        const char *risco;
        if (score >= 9) risco = "\033[31mAlto\033[0m";
        else if (score >= 6) risco = "\033[33mMedio\033[0m";
        else risco = "\033[32mBaixo\033[0m";

        // Imprime a linha de dados do processo.
        seq_printf(m, "%-8d | %-5lu | %-7lu | %-9lu | %-9lu | %-11lu | %-7d | %-5d | %-5d | %-5d | %-6s | %d\n",
                   task->pid, cpu_percent, mem_usage_mb, syscalls, input, output,
                   num_sockets, num_sockets_tcp, num_sockets_udp,
                   prioridade, risco, score);
    }
    rcu_read_unlock();

    return 0;
}

// Define a função de abertura para o arquivo /proc.
static int mod2_open(struct inode *inode, struct file *file)
{
    return single_open(file, mod2_show, NULL);
}

// Estrutura que define as operações de arquivo para /proc/mod2.
static const struct proc_ops mod2_fops = {
    .proc_open = mod2_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

// Função de inicialização do módulo.
static int __init mod2_init(void)
{
    // Cria a entrada /proc/mod2.
    proc_create(PROC_NAME, 0, NULL, &mod2_fops);
    printk(KERN_INFO "Modulo 2: iniciado\n");
    return 0;
}

// Função de finalização do módulo.
static void __exit mod2_exit(void)
{
    struct proc_cpu_data *entry, *tmp;
    // Remove a entrada /proc/mod2.
    remove_proc_entry(PROC_NAME, NULL);

    // Libera a memória alocada para `proc_cpu_list`.
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