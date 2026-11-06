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
#define DEVICE_NAME "lab2"
#define CLASS_NAME "lab2_class"
#define BUFFER_SIZE 1024
#define DRIVER_MAJOR 240

static int major_number = DRIVER_MAJOR;
static char device_buffer[BUFFER_SIZE];
static int buffer_len = 0;
static int open_count = 0;
static int read_only = 0;
static struct class *lab2_class = NULL;
static struct device *lab2_device = NULL;

static DEFINE_MUTEX(lab2_mutex);
static bool lab2_sysfs_created = false;
/* ===== PROCFS Implementation ===== */
static struct proc_dir_entry *proc_entry;
/* Ham open: mo character device */
/* Hien thi thong tin driver qua procfs */
static int lab2_proc_show(struct seq_file *m, void *v)
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

static int lab2_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, lab2_proc_show, NULL);
}

static const struct proc_ops lab2_proc_fops = {
    .proc_open    = lab2_proc_open,
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

    mutex_lock(&lab2_mutex);
    ret = sprintf(buf, "%d\n", read_only);
    mutex_unlock(&lab2_mutex);

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

    mutex_lock(&lab2_mutex);
    read_only = value;
    mutex_unlock(&lab2_mutex);

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

    mutex_lock(&lab2_mutex);

    for (i = 0; i < buffer_len; i++) {
        if (isspace(device_buffer[i])) {
            in_word = false;
        } else if (!in_word) {
            count++;
            in_word = true;
        }
    }

    mutex_unlock(&lab2_mutex);

    return sprintf(buf, "%d\n", count);
}
static DEVICE_ATTR_RO(buffer_len);
static DEVICE_ATTR_RO(open_count);
static DEVICE_ATTR_RO(last_data);
static DEVICE_ATTR_RO(word_count);
static DEVICE_ATTR_RW(read_only);
static struct attribute *lab2_attrs[] = {
    &dev_attr_buffer_len.attr,
    &dev_attr_open_count.attr,
    &dev_attr_last_data.attr,
    &dev_attr_word_count.attr,
    &dev_attr_read_only.attr,
    NULL,
};


static struct attribute_group lab2_attr_group = {
    .attrs = lab2_attrs,
};
static int lab2_open(struct inode *inode, struct file *file)
{
    mutex_lock(&lab2_mutex);

    if (open_count > 0) {
        mutex_unlock(&lab2_mutex);
        pr_info("lab2_driver: device is busy\n");
        return -EBUSY;
    }

    open_count = 1;

    mutex_unlock(&lab2_mutex);

    pr_info("lab2_driver: device opened\n");
    return 0;
}

/* Ham release: dong character device */
static int lab2_release(struct inode *inode, struct file *file)
{
    mutex_lock(&lab2_mutex);

    open_count = 0;

    mutex_unlock(&lab2_mutex);

    pr_info("lab2_driver: device closed\n");
    return 0;
}

/* Ham read: doc du lieu tu kernel sang userspace */
static ssize_t lab2_read(struct file *file, char __user *buf,
                         size_t len, loff_t *offset)
{
    if (*offset >= buffer_len)
        return 0;

    if (len > buffer_len - *offset)
        len = buffer_len - *offset;

    if (copy_to_user(buf, device_buffer + *offset, len))
        return -EFAULT;

    *offset += len;

    pr_info("lab2_driver: read %zu bytes\n", len);
    return len;
}

/* Ham write: ghi du lieu tu userspace vao kernel */
static ssize_t lab2_write(struct file *file, const char __user *buf,
                          size_t len, loff_t *offset)
{
    if (len > BUFFER_SIZE - 1)
        len = BUFFER_SIZE - 1;

    mutex_lock(&lab2_mutex);

    /* Check read-only protection */
    if (read_only == 1) {
        mutex_unlock(&lab2_mutex);
        return -EPERM;
    }

    if (copy_from_user(device_buffer, buf, len)) {
        mutex_unlock(&lab2_mutex);
        return -EFAULT;
    }

    buffer_len = len;
    device_buffer[buffer_len] = '\0';

    mutex_unlock(&lab2_mutex);

    pr_info("lab2_driver: received %zu bytes\n", len);
    return len;
}

/* Dang ky cac thao tac cua device */
static struct file_operations lab2_fops = {
    .owner = THIS_MODULE,
    .open = lab2_open,
    .read = lab2_read,
    .write = lab2_write,
    .release = lab2_release,
};

/* Ham init: duoc goi khi nap module */
static int __init lab2_init(void)
{
    int ret;

    pr_info("lab2_driver: initializing module\n");

    ret = register_chrdev(major_number, DEVICE_NAME, &lab2_fops);
    if (ret < 0) {
        pr_err("lab2_driver: register_chrdev failed\n");
        return ret;
    }

    lab2_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(lab2_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(lab2_class);
    }

    lab2_device = device_create(lab2_class, NULL,
                                MKDEV(major_number, 0),
                                NULL, DEVICE_NAME);

    if (IS_ERR(lab2_device)) {
        class_destroy(lab2_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(lab2_device);
    }
/* Tao sysfs attribute group */
ret = sysfs_create_group(&lab2_device->kobj,
                         &lab2_attr_group);

if (ret) {
    pr_warn("lab2_driver: sysfs_create_group failed\n");
} else {
    lab2_sysfs_created = true;
    pr_info("lab2_driver: sysfs group created\n");
}
    /* Tao /proc/lab2_info */
proc_entry = proc_create("lab2_info", 0444, NULL,
                         &lab2_proc_fops);

if (!proc_entry)
    pr_warn("lab2_driver: failed to create /proc/lab2_info\n");
else
    pr_info("lab2_driver: /proc/lab2_info created\n");

pr_info("lab2_driver: loaded, major=%d\n", major_number);
return 0;
}
   /* Ham exit: duoc goi khi go module */
static void __exit lab2_exit(void)
{
    /* Xoa procfs */
    if (proc_entry)
        proc_remove(proc_entry);

    /* Xoa sysfs */
    if (lab2_sysfs_created)
        sysfs_remove_group(&lab2_device->kobj,
                           &lab2_attr_group);

    /* Huy device va class */
    device_destroy(lab2_class, MKDEV(major_number, 0));
    class_destroy(lab2_class);

    /* Huy dang ky character device */
    unregister_chrdev(major_number, DEVICE_NAME);

    pr_info("lab2_driver: module unloaded\n");
}

module_init(lab2_init);
module_exit(lab2_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Lab");
MODULE_DESCRIPTION("LAB-02 Character Device Driver");
MODULE_VERSION("1.0");


