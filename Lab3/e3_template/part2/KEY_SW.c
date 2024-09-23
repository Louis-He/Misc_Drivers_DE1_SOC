#include <linux/fs.h>               // struct file, struct file_operations
#include <linux/init.h>             // for __init, see code
#include <linux/module.h>           // for module init and exit macros
#include <linux/miscdevice.h>       // for misc_device_register and struct miscdev
#include <linux/uaccess.h>          // for copy_to_user, see code
#include <asm/io.h>                 // for mmap
#include "../include/address_map_arm_vm.h"

static int device_open (struct inode *, struct file *);
static int device_release (struct inode *, struct file *);
static ssize_t device_read (struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

static ssize_t device_sw_read(struct file *, char *, size_t, loff_t *);

static struct file_operations key_dev_fops = {
    .owner = THIS_MODULE,
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release
};

static struct file_operations sw_dev_fops = {
    .owner = THIS_MODULE,
    .read = device_sw_read,
    .write = device_write,
    .open = device_open,
    .release = device_release
};


#define SUCCESS 0
#define KEY_DEV_NAME "KEY"
#define SW_DEV_NAME "SW"

void * LW_virtual;             // used to map physical addresses for the light-weight bridge
volatile int* KEY_ptr;
volatile int* SW_ptr;

static struct miscdevice key_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = KEY_DEV_NAME,
    .fops = &key_dev_fops,
    .mode = 0666
};
static struct miscdevice sw_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = SW_DEV_NAME,
    .fops = &sw_dev_fops,
    .mode = 0666
};
static int key_dev_registered = 0;
static int sw_dev_registered = 0;

static int __init start_key_swdev(void)
{
    // generate a virtual address for the FPGA lightweight bridge
    LW_virtual = ioremap_nocache (LW_BRIDGE_BASE, LW_BRIDGE_SPAN);
    
    int err = misc_register (&key_dev);
    if (err < 0) {
        printk (KERN_ERR "/dev/%s: misc_register() failed\n", KEY_DEV_NAME);
    }
    else {
        printk (KERN_INFO "/dev/%s driver registered\n", KEY_DEV_NAME);
        key_dev_registered = 1;

        KEY_ptr = LW_virtual + KEY_BASE;

        *(KEY_ptr + 3) = 0xF; 
    }

    err = misc_register (&sw_dev);
    if (err < 0) {
        printk (KERN_ERR "/dev/%s: misc_register() failed\n", SW_DEV_NAME);
    } else {
        printk (KERN_INFO "/dev/%s driver registered\n", SW_DEV_NAME);
        sw_dev_registered = 1;
        SW_ptr = LW_virtual + SW_BASE;
    }

    return err;
}

static void __exit stop_key_swdev(void)
{
    iounmap (LW_virtual);
    if (key_dev_registered) {
        misc_deregister (&key_dev);
        printk (KERN_INFO "/dev/%s driver de-registered\n", KEY_DEV_NAME);

        KEY_ptr = NULL;
    }

    if (sw_dev_registered) {
        misc_deregister (&sw_dev);
        printk (KERN_INFO "/dev/%s driver de-registered\n", SW_DEV_NAME);

        SW_ptr = NULL;
    }
}

static int device_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "Opened device: %s\n", KEY_DEV_NAME);
    return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "Closed device: %s\n", KEY_DEV_NAME);
    return 0;
}

static ssize_t device_read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
    if (*offset == 2) {
        return 0;
    }

    printk(KERN_INFO "Reading from device: %s\n", KEY_DEV_NAME);
    char key_value[2];

    key_value[0] = *(KEY_ptr + 3);
    printk(KERN_INFO "Key value: %d\n", key_value[0]);

    // clear edge register
    *(KEY_ptr + 3) = key_value[0]; 

    if (key_value[0] < 10) {
        key_value[0] += '0';
    } else {
        key_value[0] = 'A' + key_value[0] - 10;
    }

    key_value[1] = '\n';

    if (copy_to_user (buffer, key_value, 2) != 0)
    {
        printk(KERN_ERR "Error: copy_to_user unsuccessful\n");
    }

    printk(KERN_INFO "Key value: %c\n", key_value[0]);
    *offset = 2;
    return 2;
}

static ssize_t device_sw_read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
    if (*offset == 4) {
        return 0;
    }

    printk(KERN_INFO "Reading from device: %s\n", SW_DEV_NAME);
    char sw_value[4];
    memset(sw_value, 0, 4);

    int sw_read_value = *SW_ptr;
    sprintf(sw_value, "%x", sw_read_value);

    sw_value[3] = '\n';

    if (copy_to_user (buffer, sw_value, 4) != 0)
    {
        printk(KERN_ERR "Error: copy_to_user unsuccessful\n");
    }

    printk(KERN_INFO "Switch value: %x\n", sw_read_value);
    *offset = 4;
    return 4;
}

/* Called when a process writes to key_swdev. Stores the data received into key_swdev_msg, and 
 * returns the number of bytes stored. */
static ssize_t device_write(struct file *filp, const char *buffer, size_t length, loff_t *offset)
{
    size_t bytes;

    bytes = 0;

    return bytes;
}


MODULE_LICENSE("GPL");
module_init (start_key_swdev);
module_exit (stop_key_swdev);
