
#include <linux/ioctl.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/version.h>

#define DEVICE_NAME "sms_sensor"
#define CLASS_NAME  "sms_class"
#define PROC_NAME   "sms_stats"
#define SMS_MAGIC       'S'
#define SMS_SET_INTERVAL _IOW(SMS_MAGIC, 1, unsigned int)
#define DEFAULT_INTERVAL_MS 100

static int major_number;
static struct class *sms_class;
static struct device *sms_device;

static DEFINE_MUTEX(sensor_mutex);

static unsigned int read_count;
static int last_temperature;
static int last_humidity;

static unsigned int interval_ms = DEFAULT_INTERVAL_MS;
static unsigned long start_time;
static unsigned long last_read_jiffies;
/* Tao du lieu cam bien ngau nhien */
/* Tao du lieu cam bien ngau nhien */
static void generate_sensor_data(void)
{
    u32 random_temp;
    u32 random_humidity;

    random_temp = get_random_u32();
    random_humidity = get_random_u32();

    /* Nhiet do: 15.00 - 45.00 C */
    last_temperature = 1500 + (random_temp % 3001);

    /* Do am: 30.00 - 90.00 % */
    last_humidity = 3000 + (random_humidity % 6001);
}

/* Doc du lieu sensor tu userspace */
static ssize_t sms_read(struct file *file,
                        char __user *user_buffer,
                        size_t count,
                        loff_t *offset)
{
    char buffer[128];
    int len;
    u64 timestamp;
    unsigned long remaining;

    if (*offset > 0)
        return 0;

    mutex_lock(&sensor_mutex);

    /* Cho den khi du khoang thoi gian toi thieu */
    if (last_read_jiffies != 0) {
        remaining = msecs_to_jiffies(interval_ms) -
                    min(msecs_to_jiffies(interval_ms),
                        jiffies - last_read_jiffies);

        mutex_unlock(&sensor_mutex);

        if (remaining > 0)
            msleep(jiffies_to_msecs(remaining));

        mutex_lock(&sensor_mutex);
    }

    generate_sensor_data();
    last_read_jiffies = jiffies;
    timestamp = ktime_to_ms(ktime_get());
    read_count++;

    len = snprintf(buffer, sizeof(buffer),
                   "%llu,%d.%02d,%d.%02d\n",
                   (unsigned long long)timestamp,
                   last_temperature / 100,
                   last_temperature % 100,
                   last_humidity / 100,
                   last_humidity % 100);

    mutex_unlock(&sensor_mutex);

    if (count < len)
        return -EINVAL;

    if (copy_to_user(user_buffer, buffer, len))
        return -EFAULT;

    *offset += len;
    return len;
}

/* Cau hinh khoang thoi gian doc sensor */
static long sms_ioctl(struct file *file,
                      unsigned int cmd,
                      unsigned long arg)
{
    unsigned int new_interval;

    if (cmd != SMS_SET_INTERVAL)
        return -EINVAL;

    if (copy_from_user(&new_interval,
                       (unsigned int __user *)arg,
                       sizeof(new_interval)))
        return -EFAULT;

    if (new_interval < 1 || new_interval > 60000)
        return -EINVAL;

    mutex_lock(&sensor_mutex);
    interval_ms = new_interval;
    mutex_unlock(&sensor_mutex);

    return 0;
}
/* Hien thi thong ke sensor qua /proc */
static int sms_stats_show(struct seq_file *m, void *v)
{
    unsigned long uptime;

    mutex_lock(&sensor_mutex);

    uptime = jiffies_to_msecs(jiffies - start_time) / 1000;

    seq_printf(m, "Sensor Monitoring Statistics\n");
    seq_printf(m, "Read count       : %u\n", read_count);
    seq_printf(m, "Last temperature : %d.%02d C\n",
               last_temperature / 100,
               last_temperature % 100);
    seq_printf(m, "Last humidity    : %d.%02d %%\n",
               last_humidity / 100,
               last_humidity % 100);
    seq_printf(m, "Uptime           : %lu seconds\n", uptime);

    mutex_unlock(&sensor_mutex);

    return 0;
}

static int sms_stats_open(struct inode *inode, struct file *file)
{
    return single_open(file, sms_stats_show, NULL);
}

	static const struct proc_ops sms_proc_ops = {
    .proc_open    = sms_stats_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};
/* Cac thao tac cua character device */
static const struct file_operations sms_fops = {
    .owner          = THIS_MODULE,
    .read           = sms_read,
    .unlocked_ioctl = sms_ioctl,
};

/* Khoi tao sensor driver */
static int __init sms_init(void)
{
    major_number = register_chrdev(0, DEVICE_NAME, &sms_fops);

    if (major_number < 0) {
        pr_err("sms_sensor: failed to register character device\n");
        return major_number;
    }

    sms_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(sms_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(sms_class);
    }

    sms_device = device_create(sms_class, NULL,
                               MKDEV(major_number, 0),
                               NULL, DEVICE_NAME);
    if (IS_ERR(sms_device)) {
        class_destroy(sms_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(sms_device);
    }

    if (!proc_create(PROC_NAME, 0444, NULL, &sms_proc_ops)) {
        device_destroy(sms_class, MKDEV(major_number, 0));
        class_destroy(sms_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return -ENOMEM;
    }

    start_time = jiffies;

    pr_info("sms_sensor: driver initialized, major=%d\n",
            major_number);

    return 0;
}

/* Ket thuc sensor driver */
static void __exit sms_exit(void)
{
    remove_proc_entry(PROC_NAME, NULL);

    device_destroy(sms_class, MKDEV(major_number, 0));
    class_destroy(sms_class);
    unregister_chrdev(major_number, DEVICE_NAME);

    pr_info("sms_sensor: driver removed\n");
}

module_init(sms_init);
module_exit(sms_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Student");
MODULE_DESCRIPTION("Sensor Monitoring System Driver");

