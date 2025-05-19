#include "kfetch.h"

#define DEVICE_NAME "kfetch"  // Dev name as it appears in /proc/devices 
#define MSG_MAX_LEN 2048      // Max length of the message from the device 
#define MASK_MAX_LEN 2048

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

//nr_threads or nr_processes() are not defined for a kernel module
static int count_processes(void) {
    struct task_struct *task;
    int count = 0;

    for_each_process(task) {
        count++;
    }
    return count;
}

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

// Called when a process tries to open the device file: get message string
static int kfetch_open(struct inode *inode, struct file *file)
{

    // Concurrency protection: only opens if is not been used
    if (atomic_cmpxchg(&already_open, KFETCH_NOT_USED, KFTECH_EXCLUSIVE_OPEN))
        return -EBUSY;

    // Hostname + "---" strings
    size_t hostname_len = strnlen("hostname: \n", 20) + strnlen(init_uts_ns.name.nodename, 80);

    char *dashes = kmalloc(hostname_len * sizeof(char), GFP_KERNEL); //Dynamic alloc for dashes
    if (!dashes) {
        pr_err("Kmalloc failed!\n");
        return -ENOMEM;
    }

    memset(dashes, '-', hostname_len - 1); //Fill with dashes
    dashes[hostname_len - 1] = '\0'; // End of string

    //Mem info
    struct sysinfo mi;
    si_meminfo(&mi);

    //uptime
    struct timespec64 tp;
    ktime_get_boottime_ts64(&tp);
    int uptime = tp.tv_sec;

#define K(x) ((x) << (PAGE_SHIFT - 10)) // Turn page-sized info to KB (as seen in fs/proc/meminfo)

    sprintf(msg, 
        "         {\n"
        "      {   }\n"
        "       }_{ __{\n"
        "    .-{   }   }-.\n"
        "   (   }     {   )         Hostname: %s\n"
        "   |`-.._____..-'|         %s\n"
        "   |             ;--.      Kernel: %s\n"
        "   |            (__  \\     CPU: %s\n"
        "   |             | )  )    CPUs: %d/%d\n"
        "   |             |/  /     Mem: %lu MB / %lu MB\n"
        "   |             /  /      Proc: %d\n"
        "   |            (  /       Uptime: %d\n"
        "   \\             y'\n"
        "    `-.._____..-'\n%d",
        init_uts_ns.name.nodename,          //Hostname
        dashes,                             //Div
        init_uts_ns.name.release,           //kernel version
        cpu_data(0).x86_model_id,           //CPU model name
        num_online_cpus(),                  //CPU used
        num_possible_cpus(),                //Total CPU
        K(mi.totalram - mi.freeram) / 1024, //Used RAM in MB
        K(mi.totalram) / 1024,              //Total RAM in MB
        count_processes(),                  //Number of processes
        uptime,                             //Uptime in seconds
        mask
    );


    kfree(dashes);

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

    if (len >= MASK_MAX_LEN) return -EINVAL;
    if (copy_from_user(mask_str, buff, len)) return -EFAULT;

    if (kstrtoint(mask_str, 10, &mask)) return -EINVAL;

    return strnlen(mask_str, 3);
}

module_init(kfetch_init);
module_exit(kfetch_exit);

MODULE_LICENSE("GPL");
