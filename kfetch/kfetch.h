#ifndef CHARDEV_H
#define CHARDEV_H

#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h> 
#include <linux/module.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/utsname.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/vmstat.h>
#include <linux/sysinfo.h>
#include <linux/errno.h>
#include <asm/processor.h>
#include <linux/smp.h>
#include <linux/sched/signal.h>
#include <linux/sched.h>
#include <linux/timekeeping.h>
#include <linux/types.h>

static int kfetch_open(struct inode *, struct file *);
static int kfetch_release(struct inode *, struct file *);
static ssize_t kfetch_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t kfetch_write(struct file *, const char __user *, size_t, loff_t *);

//nr_threads or nr_processes() are not defined for a kernel module
static int count_processes(void) {
    struct task_struct *task;
    int count = 0;

    for_each_process(task) {
        count++;
    }
    return count;
}

static void select_chosen_data(char **str_formats) {
    int j = 0;

    // Move valid strings to begin
    for (int i = 0; i < 7; i++) {
        if (strncmp(str_formats[i], " ", 100) != 0) {
            if (i != j) {
                strncpy(str_formats[j], str_formats[i], 100);
            }
            j++;
        }
    }

    // Set the rest with void strings
    for (; j < 7; j++) {
        strncpy(str_formats[j], " ", 100);
    }
}

#endif