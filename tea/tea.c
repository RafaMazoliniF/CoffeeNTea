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
MODULE_AUTHOR("Group 2");
MODULE_DESCRIPTION("Module 2: Process Behavior Scoring System");

// Structure to store temporal data for each process
struct proc_cpu_data {
    pid_t pid;
    u64 last_exec_runtime; // Accumulated CPU time at the last reading
    u64 last_time;         // Timestamp of the last reading
    struct list_head list; // For chaining in the global list
};

static LIST_HEAD(proc_cpu_list);
static DEFINE_MUTEX(proc_cpu_mutex);

// Searches for or creates a `proc_cpu_data` entry for a given PID.
static struct proc_cpu_data *get_proc_data(pid_t pid) {
    struct proc_cpu_data *entry;

    // Tries to find an existing entry.
    list_for_each_entry(entry, &proc_cpu_list, list) {
        if (entry->pid == pid)
            return entry;
    }

    // If it doesn't exist, creates a new one.
    entry = kmalloc(sizeof(*entry), GFP_KERNEL);
    if (!entry)
        return NULL;

    entry->pid = pid;
    entry->last_exec_runtime = 0;
    entry->last_time = ktime_get_ns();
    list_add(&entry->list, &proc_cpu_list);

    return entry;
}

// Main function that generates content for the /proc/mod2 file.
// Iterates over all processes, collects metrics, and calculates a risk score.
static int mod2_show(struct seq_file *m, void *v)
{
    struct task_struct *task;
    u64 now_time = sched_clock(); // Current scheduler time, for CPU delta calculation

    seq_puts(m, "\033[2J\033[H"); // Clears the terminal screen

    seq_printf(m, BOLD_BLUE "%-8s | %-5s | %-7s | %-9s | %-9s | %-9s | %-7s | %-5s | %-5s | %-5s | %-6s | %s\n" RESET,
               "PID", "%CPU", "Mem(KB)", "Syscalls", "Input(MB)", "Output(MB)", "Sockets", "TCP", "UDP", "Prio", "Risk", "Score");
    seq_printf(m, "---------+-------+---------+-----------+-----------+-------------+---------+-------+-------+-------+--------+-------\n");

    rcu_read_lock(); // Required for safe for_each_process
    for_each_process(task) {
        u64 now_exec_runtime = task->se.sum_exec_runtime;
        unsigned long cpu_percent = 0;

        // CPU usage since the last reading.
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

        // Collection of other metrics: syscalls, I/O, priority.
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

        // Count of sockets opened by the process (TCP and UDP).
        int num_sockets = 0;
        int num_sockets_tcp = 0;
        int num_sockets_udp = 0;
        struct files_struct *files = task->files;
        if (files) {
            spin_lock(&files->file_lock); // Protects access to the file table
            struct fdtable *fdt = files_fdtable(files); // files_fdtable is safe with the lock
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


        unsigned long input = read_bytes / 1048576; // Input in MB (divides by 2^20)
        unsigned long output = write_bytes / 1048576; // Output in MB (divides by 2^20)
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

        // Risk classification (High, Medium, Low)
        const char *risk;
        if (score >= 9) risk = "\033[31mHigh\033[0m";
        else if (score >= 6) risk = "\033[33mMedium\033[0m";
        else risk = "\033[32mLow\033[0m";

        // Prints the process data line.
        seq_printf(m, "%-8d | %-5lu | %-7lu | %-9lu | %-9lu | %-11lu | %-7d | %-5d | %-5d | %-5d | %-6s | %d\n",
                   task->pid, cpu_percent, mem_usage_mb, syscalls, input, output,
                   num_sockets, num_sockets_tcp, num_sockets_udp,
                   prioridade, risk, score);
    }
    rcu_read_unlock();

    return 0;
}

// Defines the open function for the /proc file.
static int mod2_open(struct inode *inode, struct file *file)
{
    return single_open(file, mod2_show, NULL);
}

// Structure that defines the file operations for /proc/mod2.
static const struct proc_ops mod2_fops = {
    .proc_open = mod2_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

// Module initialization function.
static int __init mod2_init(void)
{
    // Creates the /proc/mod2 entry.
    proc_create(PROC_NAME, 0, NULL, &mod2_fops);
    printk(KERN_INFO "Module 2: initialized\n");
    return 0;
}

// Module finalization function.
static void __exit mod2_exit(void)
{
    struct proc_cpu_data *entry, *tmp;
    // Removes the /proc/mod2 entry.
    remove_proc_entry(PROC_NAME, NULL);

    // Frees the memory allocated for `proc_cpu_list`.
    mutex_lock(&proc_cpu_mutex);
    list_for_each_entry_safe(entry, tmp, &proc_cpu_list, list) {
        list_del(&entry->list);
        kfree(entry);
    }
    mutex_unlock(&proc_cpu_mutex);
    printk(KERN_INFO "Module 2: terminated\n");
}

module_init(mod2_init);
module_exit(mod2_exit);