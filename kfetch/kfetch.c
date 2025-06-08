#include "kfetch.h"

#define DEVICE_NAME "kfetch"  // Device name shown in /proc/devices
#define MSG_MAX_LEN 2048      // Max output message length
#define MASK_MAX_LEN 2048     // Max input mask length

// Bitmask flags for selecting displayed system info
#define KFETCH_RELEASE   (1 << 0)
#define KFETCH_CPU_MODEL (1 << 1)
#define KFETCH_NUM_CPUS  (1 << 2)
#define KFETCH_MEM       (1 << 3)
#define KFETCH_NUM_PROCS (1 << 4)
#define KFETCH_UPTIME    (1 << 5)
#define KFETCH_FULL_INFO 0  // If mask is 0, display all

// ANSI color codes for formatting output
#define BOLD_BLUE     "\x1b[1;34m"  
#define BOLD_GREEN    "\x1b[1;32m"
#define CYAN          "\x1b[36m"
#define YELLOW        "\x1b[38;5;143m"
#define RESET         "\x1b[0m"

static int major; // Major number assigned to the device

// Used to ensure only one process opens the device at a time
enum {
    KFETCH_NOT_USED,
    KFTECH_EXCLUSIVE_OPEN,
};
static atomic_t already_open = ATOMIC_INIT(KFETCH_NOT_USED);

static char msg[MSG_MAX_LEN + 1]; // Final output message
static int mask = 0;              // Bitmask for chosen system info

static struct class *cls;

// File operations for the character device
static struct file_operations kfetch_fops = {
    .read = kfetch_read,
    .write = kfetch_write,
    .open = kfetch_open,
    .release = kfetch_release,
};

// Module init: register character device and create /dev/kfetch
static int __init kfetch_init(void)
{
    major = register_chrdev(0, DEVICE_NAME, &kfetch_fops);
    if (major < 0) {
        pr_alert("Registering char device failed with %d\n", major);
        return major;
    }

    pr_info("I was assigned major number %d.\n", major);

    cls = class_create(DEVICE_NAME);
    device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);
    pr_info("Device created on /dev/%s\n", DEVICE_NAME);

    return 0;
}

// Module exit: cleanup device and class
static void __exit kfetch_exit(void)
{
    device_destroy(cls, MKDEV(major, 0));
    class_destroy(cls);
    unregister_chrdev(major, DEVICE_NAME);
}

// Converts number of pages to kilobytes
#define K(x) ((x) << (PAGE_SHIFT - 10)) 

// Handle device open: initialize message, ensure exclusive access
static int kfetch_open(struct inode *inode, struct file *file)
{
    if (atomic_cmpxchg(&already_open, KFETCH_NOT_USED, KFTECH_EXCLUSIVE_OPEN))
        return -EBUSY;

    // Allocate and fill dynamic dashed line under hostname
    size_t hostname_len = strnlen("hostname: \n", 20) + strnlen(init_uts_ns.name.nodename, 80);
    char *dashes = kmalloc(hostname_len * sizeof(char) + 1, GFP_KERNEL);
    if (!dashes) {
        pr_err("Dashes kmalloc failed!\n");
        return -ENOMEM;
    }
    memset(dashes, '-', hostname_len);
    dashes[hostname_len] = '\0';

    // Gather system info
    struct sysinfo mi;
    si_meminfo(&mi);

    struct timespec64 tp;
    ktime_get_boottime_ts64(&tp);
    int uptime = tp.tv_sec;

    // Allocate buffers for each info line
    char *str_formats[7];
    for (int i = 0; i < 7; i++) {
        str_formats[i] = kmalloc(100 * sizeof(char), GFP_KERNEL);
        if (!str_formats[i]) {
            pr_err("str_formats kmalloc failed!\n");
            return -ENOMEM;
        }
    }

    // Format selected system info
    snprintf(str_formats[0], 100, BOLD_GREEN "Hostname: %s" RESET, init_uts_ns.name.nodename);
    if (mask & KFETCH_RELEASE || mask == 0)
        snprintf(str_formats[1], 100, BOLD_BLUE "Kernel: " YELLOW "%s" RESET, init_uts_ns.name.release);
    else snprintf(str_formats[1], 100, " ");

    if (mask & KFETCH_CPU_MODEL || mask == 0)
        snprintf(str_formats[2], 100, BOLD_BLUE "CPU: " YELLOW "%s" RESET, cpu_data(0).x86_model_id);
    else snprintf(str_formats[2], 100, " ");

    if (mask & KFETCH_NUM_CPUS || mask == 0)
        snprintf(str_formats[3], 100, BOLD_BLUE "CPUs: " YELLOW "%d/%d" RESET, num_online_cpus(), num_possible_cpus());
    else snprintf(str_formats[3], 100, " ");

    if (mask & KFETCH_MEM || mask == 0)
        snprintf(str_formats[4], 100, BOLD_BLUE "Mem: " YELLOW "%lu MB / %lu MB" RESET, K(mi.totalram - mi.freeram) / 1024, K(mi.totalram) / 1024);
    else snprintf(str_formats[4], 100, " ");

    if (mask & KFETCH_NUM_PROCS || mask == 0)
        snprintf(str_formats[5], 100, BOLD_BLUE "Proc: " YELLOW "%d" RESET, count_processes());
    else snprintf(str_formats[5], 100, " ");

    if (mask & KFETCH_UPTIME || mask == 0)
        snprintf(str_formats[6], 100, BOLD_BLUE "Uptime: " YELLOW "%d min" RESET, uptime);
    else snprintf(str_formats[6], 100, " ");

    // Align values under ASCII art
    select_chosen_data(str_formats);

    const char *art_kfeti =
    "\n"
    "         {\n"
    "      {   }\n"
    "       }" CYAN "_" RESET
    "{" RESET " " CYAN "__" RESET "{\n"
    "    " CYAN ".-" RESET "{   }   }" CYAN "-." RESET "\n"
    "   " CYAN "(   " RESET "}     {   " CYAN ")         " RESET "%s\n"
    "   " CYAN "|`-.._____..-'|         " RESET "%s\n"
    "   " CYAN "|             ;--.      " RESET "%s\n"
    "   " CYAN "|            (__  \\     " RESET "%s\n"
    "   " CYAN "|             | )  )    " RESET "%s\n"
    "   " CYAN "|             |/  /     " RESET "%s\n"
    "   " CYAN "|             /  /      " RESET "%s\n"
    "   " CYAN "|            (  /       " RESET "%s\n"
    "   " CYAN "\\             y'\n"
    "   " CYAN " `-.._____..-'" RESET "\n\n";

    // Final message with data injected into ASCII art
    snprintf(msg, sizeof(msg), art_kfeti,
        str_formats[0], dashes,
        str_formats[1], str_formats[2],
        str_formats[3], str_formats[4],
        str_formats[5], str_formats[6]);

    // Free temporary allocations
    kfree(dashes);
    for (int i = 0; i < 7; i++) {
        kfree(str_formats[i]);
    }

    try_module_get(THIS_MODULE);
    return 0;
}

// Handle device release: mark as no longer in use
static int kfetch_release(struct inode *inode, struct file *file)
{
    atomic_set(&already_open, KFETCH_NOT_USED);
    module_put(THIS_MODULE);
    return 0;
}

// Handle read from /dev/kfetch: copy ASCII-art message to user
static ssize_t kfetch_read(struct file *filp, 
                           char __user *buffer,
                           size_t length,   
                           loff_t *offset)
{
    int bytes_read = 0;
    const char *msg_ptr = msg;

    if (!*(msg_ptr + *offset)) {
        *offset = 0;
        return 0;
    }

    msg_ptr += *offset;

    // Copy data to user buffer byte by byte
    while (length && *msg_ptr) {
        put_user(*(msg_ptr++), buffer++);
        length--;
        bytes_read++;
    }

    *offset += bytes_read;
    return bytes_read;
}

// Handle write to /dev/kfetch: set the data mask
static ssize_t kfetch_write(struct file *filp, const char __user *buff,
                            size_t len, loff_t *off)
{   
    char mask_str[10];
    int mask_aux = 0;

    if (len >= MASK_MAX_LEN) return -EINVAL;
    if (copy_from_user(mask_str, buff, len)) return -EFAULT;
    if (kstrtoint(mask_str, 10, &mask_aux)) return -EINVAL;
    if (mask_aux > 63) return -EINVAL; // max valid bitmask: 0b111111

    mask = mask_aux;
    return strnlen(mask_str, 3);
}

module_init(kfetch_init);
module_exit(kfetch_exit);

MODULE_LICENSE("GPL");
