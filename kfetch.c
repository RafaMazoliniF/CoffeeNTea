#include "kfetch.h"

#define DEVICE_NAME "kfetch"  // Dev name as it appears in /proc/devices 
#define MSG_MAX_LEN 2048      // Max length of the message from the device 
#define MASK_MAX_LEN 2048

#define KFETCH_RELEASE   (1 << 0)
#define KFETCH_CPU_MODEL (1 << 1)
#define KFETCH_NUM_CPUS  (1 << 2)
#define KFETCH_MEM       (1 << 3)
#define KFETCH_NUM_PROCS (1 << 4)
#define KFETCH_UPTIME    (1 << 5)
#define KFETCH_FULL_INFO    0

#define BOLD_BLUE     "\x1b[1;34m"  
#define BOLD_GREEN     "\x1b[1;32m"
#define CYAN     "\x1b[36m"
#define YELLOW     "\x1b[38;5;143m"
#define RESET     "\x1b[0m"

static int major; // major number assigned to our device driver 

enum
{
    KFETCH_NOT_USED,
    KFTECH_EXCLUSIVE_OPEN,
};

//Is device open? Used to prevent multiple access to device
static atomic_t already_open = ATOMIC_INIT(KFETCH_NOT_USED);

static char msg[MSG_MAX_LEN + 1]; //The msg the device will give when asked
static int mask = 0;

static struct class *cls;

static struct file_operations kfetch_fops = {
    .read = kfetch_read,
    .write = kfetch_write,
    .open = kfetch_open,
    .release = kfetch_release,
};

static int __init kfetch_init(void)
{
    major = register_chrdev(0, DEVICE_NAME, &kfetch_fops);

    if (major < 0)
    {
        pr_alert("Registering char device failed with %d\n", major);
        return major;
    }

    pr_info("I was assigned major number %d.\n", major);

    cls = class_create(DEVICE_NAME);
    device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);

    pr_info("Device created on /dev/%s\n", DEVICE_NAME);

    return 0;
}

static void __exit kfetch_exit(void)
{
    device_destroy(cls, MKDEV(major, 0));
    class_destroy(cls);

    // Unregister the device 
    unregister_chrdev(major, DEVICE_NAME);
}

#define K(x) ((x) << (PAGE_SHIFT - 10)) // Turn page-sized info to KB (as seen in fs/proc/meminfo)

// Called when a process tries to open the device file: get message string
static int kfetch_open(struct inode *inode, struct file *file)
{

    // Concurrency protection: only opens if is not been used
    if (atomic_cmpxchg(&already_open, KFETCH_NOT_USED, KFTECH_EXCLUSIVE_OPEN))
        return -EBUSY;

    // Hostname + "---" strings
    size_t hostname_len = strnlen("hostname: \n", 20) + strnlen(init_uts_ns.name.nodename, 80);

    char *dashes = kmalloc(hostname_len * sizeof(char) + 1, GFP_KERNEL); //Dynamic alloc for dashes
    if (!dashes) {
        pr_err("Dashes kmalloc failed!\n");
        return -ENOMEM;
    }

    memset(dashes, '-', hostname_len); //Fill with dashes
    dashes[hostname_len] = '\0'; // End of string

    //Mem info
    struct sysinfo mi;
    si_meminfo(&mi);

    //uptime
    struct timespec64 tp;
    ktime_get_boottime_ts64(&tp);
    int uptime = tp.tv_sec;

    char *str_formats[7]; // Holds the formatted strings

    //Mem allocation for all strings
    for (int i = 0; i < 7; i++) {
        str_formats[i] = kmalloc(100 * sizeof(char), GFP_KERNEL);

        if (!str_formats[i]) {
            pr_err("str_formats kmalloc failed!\n");
            return -ENOMEM;
        }
    }

    //Chose only required data
    snprintf(str_formats[0], 100, BOLD_GREEN "Hostname: %s" RESET, init_uts_ns.name.nodename); // Hostname

    if (mask & KFETCH_RELEASE || mask == 0) 
        snprintf(str_formats[1], 100, BOLD_BLUE "Kernel: " YELLOW "%s" RESET, init_uts_ns.name.release); // Kernel
    else snprintf(str_formats[1], 100, " ");
    
    if (mask & KFETCH_CPU_MODEL || mask == 0) 
        snprintf(str_formats[2], 100, BOLD_BLUE "CPU: " YELLOW "%s" RESET, cpu_data(0).x86_model_id); // CPU
    else snprintf(str_formats[2], 100, " ");
    
    if (mask & KFETCH_NUM_CPUS || mask == 0) 
        snprintf(str_formats[3], 100, BOLD_BLUE "CPUs: " YELLOW "%d/%d" RESET, num_online_cpus(), num_possible_cpus()); // CPUs
    else snprintf(str_formats[3], 100, " ");
    
    if (mask & KFETCH_MEM || mask == 0) 
        snprintf(str_formats[4], 100, BOLD_BLUE "Mem: " YELLOW "%lu MB / %lu MB" RESET, K(mi.totalram - mi.freeram) / 1024, K(mi.totalram) / 1024); // Mem
    else snprintf(str_formats[4], 100, " ");
    
    if (mask & KFETCH_NUM_PROCS || mask == 0) 
        snprintf(str_formats[5], 100, BOLD_BLUE "Proc: " YELLOW "%d" RESET, count_processes()); // Proc
    else snprintf(str_formats[5], 100, " ");
    
    if (mask & KFETCH_UPTIME || mask == 0) 
        snprintf(str_formats[6], 100, BOLD_BLUE "Uptime: " YELLOW "%d min" RESET, uptime); // Uptime
    else snprintf(str_formats[6], 100, " ");
    
    //Sorts the output
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

    //The final message
    snprintf(msg, sizeof(msg), art_kfeti,
        str_formats[0],
        dashes,
        str_formats[1],
        str_formats[2],
        str_formats[3],
        str_formats[4],
        str_formats[5],
        str_formats[6]
    );

    //Clean
    kfree(dashes);
    for (int i = 0; i < 7; i++) {
        kfree(str_formats[i]);
    }

    try_module_get(THIS_MODULE);

    return 0;
}

// Called when a process closes the device file. 
static int kfetch_release(struct inode *inode, struct file *file)
{
    // We're now ready for our next caller 
    atomic_set(&already_open, KFETCH_NOT_USED);

    module_put(THIS_MODULE);

    return 0;
}

// Called when a process tries to read the device file
static ssize_t kfetch_read(struct file *filp, 
                           char __user *buffer,
                           size_t length,   
                           loff_t *offset)
{
    // Number of bytes actually written to the buffer 
    int bytes_read = 0;
    const char *msg_ptr = msg;

    if (!*(msg_ptr + *offset))
    {                // we are at the end of message 
        *offset = 0; // reset the offset 
        return 0;    // signify end of file 
    }

    msg_ptr += *offset;

    // Actually put the data into the buffer 
    while (length && *msg_ptr)
    {
        put_user(*(msg_ptr++), buffer++); //copies the data to user space
        length--;
        bytes_read++;
    }

    *offset += bytes_read;

    // Most read functions return the number of bytes put into the buffer. 
    return bytes_read;
}

// Called when a process writes to dev file
static ssize_t kfetch_write(struct file *filp, const char __user *buff,
                            size_t len, loff_t *off)
{   
    char mask_str[10];
    int mask_aux = 0;

    if (len >= MASK_MAX_LEN) return -EINVAL;
    if (copy_from_user(mask_str, buff, len)) return -EFAULT;

    if (kstrtoint(mask_str, 10, &mask_aux)) return -EINVAL;
    if (mask_aux > 63) return -EINVAL; // if mask_aux > 0b111111
    else mask = mask_aux;

    return strnlen(mask_str, 3);
}

module_init(kfetch_init);
module_exit(kfetch_exit);

MODULE_LICENSE("GPL");
