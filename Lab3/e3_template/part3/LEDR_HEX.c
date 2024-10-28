#include <linux/fs.h>               // struct file, struct file_operations
#include <linux/init.h>             // for __init, see code
#include <linux/module.h>           // for module init and exit macros
#include <linux/miscdevice.h>       // for misc_device_register and struct miscdev
#include <linux/uaccess.h>          // for copy_to_user, see code
#include <asm/io.h>                 // for mmap
#include "../include/address_map_arm_vm.h"

// 7-seg bit patterns for digits 0-9
char seg7[10] =   {0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110, 
                   0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01100111};
                   
/**  your part 3 kernel code here  **/
static int device_open (struct inode *, struct file *);
static int device_release (struct inode *, struct file *);
static ssize_t device_read (struct file *, char *, size_t, loff_t *);

static ssize_t ledr_write(struct file *, const char *, size_t, loff_t *);
static ssize_t hex_write(struct file *, const char *, size_t, loff_t *);


static struct file_operations ledr_dev_fops = {
    .owner = THIS_MODULE,
    .read = device_read,
    .write = ledr_write,
    .open = device_open,
    .release = device_release
};

static struct file_operations hex_dev_fops = {
    .owner = THIS_MODULE,
    .read = device_read,
    .write = hex_write,
    .open = device_open,
    .release = device_release
};

#define SUCCESS 0

#define LEDR_DEV_NAME "LEDR"
#define HEX_DEV_NAME "HEX"

void * LW_virtual;             // used to map physical addresses for the light-weight bridge
volatile int* LEDR_ptr;
volatile int* HEX3_HEX0_ptr;
volatile int* HEX5_HEX4_ptr;

static struct miscdevice ledr_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = LEDR_DEV_NAME,
    .fops = &ledr_dev_fops,
    .mode = 0666
};

static struct miscdevice hex_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = HEX_DEV_NAME,
    .fops = &hex_dev_fops,
    .mode = 0666
};

static int ledr_dev_registered = 0;
static int hex_dev_registered = 0;

static int __init start_ledr_hexdev(void)
{
    // generate a virtual address for the FPGA lightweight bridge
    LW_virtual = ioremap_nocache (LW_BRIDGE_BASE, LW_BRIDGE_SPAN);
    
    int err = misc_register (&ledr_dev);
    if (err < 0) {
        printk (KERN_ERR "/dev/%s: misc_register() failed\n", LEDR_DEV_NAME);
    }
    else {
        printk (KERN_INFO "/dev/%s driver registered\n", LEDR_DEV_NAME);
        ledr_dev_registered = 1;

        LEDR_ptr = LW_virtual + LEDR_BASE;
        *LEDR_ptr = 0;
    }

    err = misc_register (&hex_dev);
    if (err < 0) {
        printk (KERN_ERR "/dev/%s: misc_register() failed\n", HEX_DEV_NAME);
    } else {
        printk (KERN_INFO "/dev/%s driver registered\n", HEX_DEV_NAME);
        hex_dev_registered = 1;
        HEX3_HEX0_ptr = LW_virtual + HEX3_HEX0_BASE;
        HEX5_HEX4_ptr = LW_virtual + HEX5_HEX4_BASE;
        *HEX3_HEX0_ptr = 0;
        *HEX5_HEX4_ptr = 0;
    }

    return err;
}

static void __exit stop_ledr_hexdev(void)
{
    if (ledr_dev_registered) {
        misc_deregister (&ledr_dev);
        printk (KERN_INFO "/dev/%s driver de-registered\n", LEDR_DEV_NAME);

        *LEDR_ptr = 0;
        LEDR_ptr = NULL;
    }

    if (hex_dev_registered) {
        misc_deregister (&hex_dev);
        printk (KERN_INFO "/dev/%s driver de-registered\n", HEX_DEV_NAME);

        *HEX3_HEX0_ptr = 0;
        *HEX5_HEX4_ptr = 0;
        HEX3_HEX0_ptr = NULL;
        HEX5_HEX4_ptr = NULL;
    }

    iounmap (LW_virtual);
}

static int device_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "Opened device: %s\n", LEDR_DEV_NAME);
    return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "Closed device: %s\n", LEDR_DEV_NAME);
    return 0;
}

static ssize_t device_read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
    return 0;
}

static ssize_t ledr_write(struct file *filp, const char *buffer, size_t length, loff_t *offset)
{
    unsigned char kbuf [10];
    memset(kbuf, 0, 10);
    if (length > 10) {
        printk(KERN_ERR "Invalid input\n");
        return -EINVAL;
    }

    copy_from_user(kbuf, buffer, length);
    printk(KERN_INFO "Writing to LEDR copy from user: %s\n", kbuf);

    unsigned long value;
    if (kstrtoul(kbuf, 16, &value) == 0) {
        printk(KERN_INFO "Writing to LEDR: %lx\n", value);
        *LEDR_ptr = (int)value;
    } else {
        printk(KERN_ERR "Invalid input\n");
        return -EINVAL;
    }

    return length;
}

static ssize_t hex_write(struct file *filp, const char *buffer, size_t length, loff_t *offset)
{
    unsigned char kbuf [10];
    memset(kbuf, 0, 10);
    if (length > 10) {
        printk(KERN_ERR "Invalid input\n");
        return -EINVAL;
    }

    copy_from_user(kbuf, buffer, length);
    printk(KERN_INFO "Writing to HEX copy from user: %s\n", kbuf);

    unsigned long value;
    if (kstrtoul(kbuf, 10, &value) == 0) {
        printk(KERN_INFO "Writing to HEX: %lx\n", value);

        int hex3_hex0 = seg7[value % 10] | (seg7[(value / 10) % 10] << 8) | 
                        (seg7[(value / 100) % 10] << 16) | (seg7[(value / 1000) % 10] << 24);
        int hex5_hex4 = seg7[(value / 10000) % 10] | (seg7[(value / 100000) % 10] << 8);

        *HEX3_HEX0_ptr = hex3_hex0;
        *HEX5_HEX4_ptr = hex5_hex4;
    } else {
        printk(KERN_ERR "Invalid input\n");
        return -EINVAL;
    }
    
    return length;
}

MODULE_LICENSE("GPL");

module_init (start_ledr_hexdev);
module_exit (stop_ledr_hexdev);