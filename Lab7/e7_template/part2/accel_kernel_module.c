#include <linux/kernel.h>
#include <linux/fs.h>               // struct file, struct file_operations
#include <linux/init.h>             // for __init, see code
#include <linux/module.h>           // for module init and exit macros
#include <linux/miscdevice.h>       // for misc_device_register and struct miscdev
#include <linux/uaccess.h>          // for copy_to_user, see code
#include <asm/io.h>                 // for mmap

#include "../include/address_map_arm.h"
#include "../include/ADXL345.h"

// Declare global variables needed to use the accelerometer
volatile unsigned int * I2C0_ptr; // virtual address for I2C communication
volatile unsigned int * SYSMGR_ptr; // virtual address for System Manager communication

// Declare variables and prototypes needed for a character device driver
static int accel_open (struct inode *, struct file *);
static int accel_release (struct inode *, struct file *);
static ssize_t accel_read (struct file *, char *, size_t, loff_t *);
// static ssize_t accel_write(struct file *, const char *, size_t, loff_t *);

/**  implement your part 2 driver here  **/
static struct file_operations accel_dev_fops = {
    .owner = THIS_MODULE,
    .read = accel_read,
    // .write = accel_write, 
    .open = accel_open,
    .release = accel_release
};

#define SUCCESS 0
#define ACCEL_DEV_NAME "accel"

static struct miscdevice accel_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = ACCEL_DEV_NAME,
    .fops = &accel_dev_fops,
    .mode = 0666
};

static int accel_dev_registered = 0;
static int16_t szData16[3];

static int __init start_accel(void) {
    uint8_t id;

    int err = misc_register (&accel_dev);
    if (err < 0) {
        printk (KERN_ERR "misc_register failed\n");
    }

    I2C0_ptr = ioremap_nocache (I2C0_BASE, I2C0_SPAN);
    SYSMGR_ptr = ioremap_nocache (SYSMGR_BASE, SYSMGR_SPAN);

    if ((I2C0_ptr == NULL) || (SYSMGR_ptr == NULL))
        printk (KERN_ERR "Error: ioremap_nocache returned NULL!\n");

    pass_addrs((unsigned int*) SYSMGR_ptr, (unsigned int*) I2C0_ptr);

    Pinmux_Config();
    I2C0_Init();
    ADXL345_Init();

    ADXL345_IdRead(&id);
    if (id != 0xE5) {
        printk("ADXL345 not detected\n");
    }

    accel_dev_registered = 1;

    return 0;
}

static void __exit stop_accel(void) {
    iounmap (I2C0_ptr);
    iounmap (SYSMGR_ptr);

    /* Remove the device from the kernel */
    if (accel_dev_registered) {
        misc_deregister (&accel_dev);
    }
}

static ssize_t accel_read(struct file *filp, char *buffer, size_t length, loff_t *offset) {
    char msg[19];
    int new_data_ready = 0;

    if (*offset == 19) {
        *offset = 0;
        return 0;
    }

    if (length < 19)
        return -EINVAL;

    new_data_ready = ADXL345_IsDataReady();
    if (new_data_ready) {
        ADXL345_XYZ_Read(szData16);
    }

    sprintf(msg, "%1d %4d %4d %4d %2d", new_data_ready ? 1 : 0, szData16[0], szData16[1], szData16[2], 31);
    if (copy_to_user(buffer, msg, 19)) {
        return -EFAULT;
    }

    *offset = 19;
    return 19;
}

static int accel_open(struct inode *inode, struct file *file){
    return SUCCESS;
}

static int accel_release(struct inode *inode, struct file *file){
    return 0;
}

MODULE_LICENSE("GPL");
module_init (start_accel);
module_exit (stop_accel);
