#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sysfs.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include "asgn1_ioctl.h"
#define DEVICE_NAME "asgn1"
#define CLASS_NAME "asgn1_class"
#define BUFFER_SIZE 1024
#define DRIVER_MAJOR 241

static int major_number = DRIVER_MAJOR;
static char device_buffer[BUFFER_SIZE];
static int buffer_len = 0;
static int open_count = 0;
static int read_only = 0;

/* Assignment 01 statistics */
static int write_count = 0;
static int read_count = 0;
static int last_write_size = 0;

/* 0 = Kernel mode, 1 = Echo mode */
static int driver_mode = 1;
static struct class *asgn1_class = NULL;
static struct device *asgn1_device = NULL;

static DEFINE_MUTEX(asgn1_mutex);
static bool asgn1_sysfs_created = false;
/* ===== PROCFS Implementation ===== */
static struct proc_dir_entry *proc_entry;
/* Ham open: mo character device */
/* Hien thi thong tin driver qua procfs */
static int asgn1_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "LAB-02 Driver Statistics\n");
    seq_printf(m, "========================\n");
    seq_printf(m, "Driver name   : %s\n", DEVICE_NAME);
    seq_printf(m, "Major number  : %d\n", major_number);
    seq_printf(m, "Buffer size   : %d bytes\n", BUFFER_SIZE);
    seq_printf(m, "Data length   : %d bytes\n", buffer_len);
    seq_printf(m, "Open count    : %d\n", open_count);

    if (buffer_len > 0)
        seq_printf(m, "Last data     : [%s]\n", device_buffer);

    return 0;
}

static int asgn1_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, asgn1_proc_show, NULL);
}

static const struct proc_ops asgn1_proc_fops = {
    .proc_open    = asgn1_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};
/* ===== SYSFS Implementation ===== */
static ssize_t read_only_show(struct device *dev,
                              struct device_attribute *attr,
                              char *buf)
{
    ssize_t ret;

    mutex_lock(&asgn1_mutex);
    ret = sprintf(buf, "%d\n", read_only);
    mutex_unlock(&asgn1_mutex);

    return ret;
}

static ssize_t read_only_store(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf,
                               size_t count)
{
    int value;

    if (kstrtoint(buf, 10, &value) != 0)
        return -EINVAL;

    if (value != 0 && value != 1)
        return -EINVAL;

    mutex_lock(&asgn1_mutex);
    read_only = value;
    mutex_unlock(&asgn1_mutex);

    return count;
}
static ssize_t buffer_len_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
    return sprintf(buf, "%d\n", buffer_len);
}

static ssize_t open_count_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
    return sprintf(buf, "%d\n", open_count);
}

static ssize_t last_data_show(struct device *dev,
                              struct device_attribute *attr,
                              char *buf)
{
    if (buffer_len > 0)
        return sprintf(buf, "%s\n", device_buffer);

    return sprintf(buf, "(empty)\n");
}
static ssize_t word_count_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
    int i;
    int count = 0;
    bool in_word = false;

    mutex_lock(&asgn1_mutex);

    for (i = 0; i < buffer_len; i++) {
        if (isspace(device_buffer[i])) {
            in_word = false;
        } else if (!in_word) {
            count++;
            in_word = true;
        }
    }

    mutex_unlock(&asgn1_mutex);

    return sprintf(buf, "%d\n", count);
}
static DEVICE_ATTR_RO(buffer_len);
static DEVICE_ATTR_RO(open_count);
static DEVICE_ATTR_RO(last_data);
static DEVICE_ATTR_RO(word_count);
static DEVICE_ATTR_RW(read_only);
static struct attribute *asgn1_attrs[] = {
    &dev_attr_buffer_len.attr,
    &dev_attr_open_count.attr,
    &dev_attr_last_data.attr,
    &dev_attr_word_count.attr,
    &dev_attr_read_only.attr,
    NULL,
};


static struct attribute_group asgn1_attr_group = {
    .attrs = asgn1_attrs,
};
static int asgn1_open(struct inode *inode, struct file *file)
{
    mutex_lock(&asgn1_mutex);

    if (open_count > 0) {
        mutex_unlock(&asgn1_mutex);
        pr_info("asgn1_driver: device is busy\n");
        return -EBUSY;
    }

    open_count = 1;

    mutex_unlock(&asgn1_mutex);

    pr_info("asgn1_driver: device opened\n");
    return 0;
}

/* Ham release: dong character device */
static int asgn1_release(struct inode *inode, struct file *file)
{
    mutex_lock(&asgn1_mutex);

    open_count = 0;

    mutex_unlock(&asgn1_mutex);

    pr_info("asgn1_driver: device closed\n");
    return 0;
}
static ssize_t asgn1_read(struct file *file, char __user *buf,
                         size_t len, loff_t *offset)
{
    mutex_lock(&asgn1_mutex);

    /* Check end of buffer */
    if (*offset >= buffer_len) {
        mutex_unlock(&asgn1_mutex);
        return 0;
    }

    /* Limit read size */
    if (len > buffer_len - *offset)
        len = buffer_len - *offset;

    /* Copy data to user space */
    if (copy_to_user(buf, device_buffer + *offset, len)) {
        mutex_unlock(&asgn1_mutex);
        return -EFAULT;
    }

    /* Update read statistics */
    *offset += len;
    read_count++;

    mutex_unlock(&asgn1_mutex);

    pr_info("asgn1_driver: read %zu bytes\n", len);

    return len;
}



static ssize_t asgn1_write(struct file *file, const char __user *buf,
                          size_t len, loff_t *offset)
{
    char *temp;
    size_t prefix_len = 0;
    size_t data_len;
    size_t max_len;

    mutex_lock(&asgn1_mutex);

    /* Check read-only protection */
    if (read_only == 1) {
        mutex_unlock(&asgn1_mutex);
        return -EPERM;
    }

    /* Kernel mode adds [KERNEL] prefix */
    if (driver_mode == 0)
        prefix_len = strlen("[KERNEL]");

    /* Limit data size to avoid buffer overflow */
    max_len = BUFFER_SIZE - 1 - prefix_len;
    data_len = (len > max_len) ? max_len : len;

    /* Allocate temporary buffer */
    temp = kmalloc(data_len + 1, GFP_KERNEL);
    if (!temp) {
        mutex_unlock(&asgn1_mutex);
        return -ENOMEM;
    }

    /* Copy data from user space */
    if (copy_from_user(temp, buf, data_len)) {
        kfree(temp);
        mutex_unlock(&asgn1_mutex);
        return -EFAULT;
    }

    temp[data_len] = '\0';

    /* Clear old buffer */
    memset(device_buffer, 0, BUFFER_SIZE);

    /* Add prefix in Kernel mode */
    if (driver_mode == 0)
        memcpy(device_buffer, "[KERNEL]", prefix_len);

    /* Copy user data */
    memcpy(device_buffer + prefix_len, temp, data_len);

    buffer_len = prefix_len + data_len;

    /* Update statistics */
    write_count++;
    last_write_size = data_len;

    kfree(temp);
    mutex_unlock(&asgn1_mutex);

    pr_info("asgn1_driver: received %zu bytes\n", data_len);

    return data_len;


}
/* Handle Assignment 01 IOCTL commands */
static long asgn1_ioctl(struct file *file,
                        unsigned int cmd,
                        unsigned long arg)
{
    int mode;
    struct asgn1_stats stats;
    char version[64];

    switch (cmd) {

    case ASGN1_RESET_BUFFER:
        pr_info("asgn1_driver: ioctl RESET_BUFFER\n");

        mutex_lock(&asgn1_mutex);

        memset(device_buffer, 0, BUFFER_SIZE);
        buffer_len = 0;
        write_count = 0;
        read_count = 0;
        last_write_size = 0;

        mutex_unlock(&asgn1_mutex);
        return 0;

    case ASGN1_GET_STATS:
        pr_info("asgn1_driver: ioctl GET_STATS\n");

        mutex_lock(&asgn1_mutex);

        stats.open_count = open_count;
        stats.write_count = write_count;
        stats.read_count = read_count;
        stats.buffer_len = buffer_len;
        stats.last_write_size = last_write_size;

        mutex_unlock(&asgn1_mutex);

        if (copy_to_user((void __user *)arg,
                         &stats, sizeof(stats)))
            return -EFAULT;

        return 0;

    case ASGN1_SET_MODE:
        pr_info("asgn1_driver: ioctl SET_MODE\n");

        if (copy_from_user(&mode,
                           (int __user *)arg,
                           sizeof(mode)))
            return -EFAULT;

        if (mode != 0 && mode != 1)
            return -EINVAL;

        mutex_lock(&asgn1_mutex);
        driver_mode = mode;
        mutex_unlock(&asgn1_mutex);

        pr_info("asgn1_driver: mode changed to %d\n", mode);
        return 0;

    case ASGN1_GET_VERSION:
        pr_info("asgn1_driver: ioctl GET_VERSION\n");

        memset(version, 0, sizeof(version));
        snprintf(version, sizeof(version),
                 "asgn1_driver v1.0");

        if (copy_to_user((void __user *)arg,
                         version, sizeof(version)))
            return -EFAULT;

        return 0;

    default:
        pr_info("asgn1_driver: invalid ioctl command\n");
        return -ENOTTY;
    }
}
static struct file_operations asgn1_fops = {
    .owner = THIS_MODULE,
    .open = asgn1_open,
    .read = asgn1_read,
    .write = asgn1_write,
    .release = asgn1_release,
    .unlocked_ioctl = asgn1_ioctl,
};

/* Ham init: duoc goi khi nap module */
static int __init asgn1_init(void)
{
    int ret;

    pr_info("asgn1_driver: initializing module\n");

    ret = register_chrdev(major_number, DEVICE_NAME, &asgn1_fops);
    if (ret < 0) {
        pr_err("asgn1_driver: register_chrdev failed\n");
        return ret;
    }

    asgn1_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(asgn1_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(asgn1_class);
    }

    asgn1_device = device_create(asgn1_class, NULL,
                                MKDEV(major_number, 0),
                                NULL, DEVICE_NAME);

    if (IS_ERR(asgn1_device)) {
        class_destroy(asgn1_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(asgn1_device);
    }
/* Tao sysfs attribute group */
ret = sysfs_create_group(&asgn1_device->kobj,
                         &asgn1_attr_group);

if (ret) {
    pr_warn("asgn1_driver: sysfs_create_group failed\n");
} else {
    asgn1_sysfs_created = true;
    pr_info("asgn1_driver: sysfs group created\n");
}
    /* Tao /proc/asgn1_info */
proc_entry = proc_create("asgn1_info", 0444, NULL,
                         &asgn1_proc_fops);

if (!proc_entry)
    pr_warn("asgn1_driver: failed to create /proc/asgn1_info\n");
else
    pr_info("asgn1_driver: /proc/asgn1_info created\n");

pr_info("asgn1_driver: loaded, major=%d\n", major_number);
return 0;
}
   /* Ham exit: duoc goi khi go module */
static void __exit asgn1_exit(void)
{
    /* Xoa procfs */
    if (proc_entry)
        proc_remove(proc_entry);

    /* Xoa sysfs */
    if (asgn1_sysfs_created)
        sysfs_remove_group(&asgn1_device->kobj,
                           &asgn1_attr_group);

    /* Huy device va class */
    device_destroy(asgn1_class, MKDEV(major_number, 0));
    class_destroy(asgn1_class);

    /* Huy dang ky character device */
    unregister_chrdev(major_number, DEVICE_NAME);

    pr_info("asgn1_driver: module unloaded\n");
}

module_init(asgn1_init);
module_exit(asgn1_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Lab");
MODULE_DESCRIPTION("Assignment 01 IOCTL Character Device Driver");
MODULE_VERSION("1.0");


