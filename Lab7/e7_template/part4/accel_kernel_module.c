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
static ssize_t accel_write(struct file *, const char *, size_t, loff_t *);

/**  implement your part 2 driver here  **/
static struct file_operations accel_dev_fops = {
    .owner = THIS_MODULE,
    .read = accel_read,
    .write = accel_write, 
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

void config_accel_tap(void ) {
    // Config Tap
    ADXL345_REG_WRITE(ADXL345_REG_THRESH_TAP, 48); // 255 / 16 * 3 = 48 
    ADXL345_REG_WRITE(ADXL345_REG_DURATION, 32); // 625us * 32 = 20ms
    ADXL345_REG_WRITE(ADXL345_REG_LATENCY, 16); // 1.25ms * 16 = 20ms
    ADXL345_REG_WRITE(ADXL345_REG_WINDOW, 240); // 1.25ms * 240 = 300ms

    // ADXL345_REG_WRITE(ADXL345_REG_TAP_AXES, 0x0F); // Enable all axes with surpress on
    ADXL345_REG_WRITE(ADXL345_REG_TAP_AXES, 0x07);

    // Interrupt Map
    ADXL345_REG_WRITE(ADXL345_REG_INT_MAP, 0x00); // Map all interrupts to INT1
}

void enable_accel_interrupts(void) {
    // stop measure
    ADXL345_REG_WRITE(ADXL345_REG_POWER_CTL, XL345_STANDBY);
    // Enable Interrupt
    ADXL345_REG_WRITE(ADXL345_REG_INT_ENABLE, XL345_DOUBLETAP | XL345_SINGLETAP | XL345_ACTIVITY | XL345_INACTIVITY);
    // start measure
    ADXL345_REG_WRITE(ADXL345_REG_POWER_CTL, XL345_MEASURE);
}

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
    config_accel_tap();
    ADXL345_Init();
    enable_accel_interrupts();

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
    char msg[21];
    int new_data_ready = 0;
    int is_single_tap = 0;
    int is_double_tap = 0;
    uint8_t int_source = 0;

    if (*offset == 21) {
        *offset = 0;
        return 0;
    }

    if (length < 21)
        return -EINVAL;

    ADXL345_REG_READ(ADXL345_REG_INT_SOURCE, (uint8_t *)&int_source);
    printk("Int Source: 0x%x\n", int_source);

    new_data_ready = int_source & XL345_DATAREADY;
    is_single_tap = int_source & XL345_SINGLETAP;
    is_double_tap = int_source & XL345_DOUBLETAP;

    if (new_data_ready) {
        ADXL345_XYZ_Read(szData16);
    }

    sprintf(msg, "%3d %4d %4d %4d %2d", int_source, szData16[0], szData16[1], szData16[2], 31);
    if (copy_to_user(buffer, msg, 21)) {
        return -EFAULT;
    }

    *offset = 21;
    return 21;
}

static ssize_t accel_write(struct file *filp, const char *buffer, size_t length, loff_t *offset) {
    char msg[length];
    copy_from_user(msg, buffer, length);

    if (length >= 4 && strncmp(msg, "init", 4) == 0) {
        config_accel_tap();
        ADXL345_Init();
        enable_accel_interrupts();
    } else if (length >= 6 && strncmp(msg, "device", 6) == 0) {
        uint8_t id;
        ADXL345_IdRead(&id);
        printk("ADXL345 ID: 0x%x\n", id);
    } else if (length >= 9 && strncmp(msg, "calibrate", 9) == 0) {
        ADXL345_Calibrate();
    } else if (length >= 6 && strncmp(msg, "format", 6) == 0) {
        int resolution;
        int range;
        int range_encoded;
        sscanf(msg, "format %d %d", &resolution, &range);

        if (resolution != 0 && resolution != 1) {
            printk("Invalid resolution value\n");
            return -EINVAL;
        }

        if (range != 2 && range != 4 && range != 8 && range != 16) {
            printk("Invalid range value\n");
            return -EINVAL;
        }

        switch (range)
        {
        case 2:
            range_encoded = XL345_RANGE_2G;
            break;
        case 4:
            range_encoded = XL345_RANGE_4G;
            break;
        case 8:
            range_encoded = XL345_RANGE_8G;
            break;
        case 16:    
            range_encoded = XL345_RANGE_16G;
            break;
        default:
            break;
        }
        ADXL345_REG_WRITE(ADXL345_REG_DATA_FORMAT, (resolution << 3) | range_encoded);
    } else if ((length >= 4) && (strncmp(msg, "rate", 4) == 0)) {
        char rate[8];
        sscanf(msg, "rate %s", &rate);

        if (strcmp(rate, "3200") == 0)
            ADXL345_REG_WRITE(ADXL345_REG_BW_RATE, XL345_RATE_3200);
        else if (strcmp(rate, "1600") == 0)
            ADXL345_REG_WRITE(ADXL345_REG_BW_RATE, XL345_RATE_1600);
        else if (strcmp(rate, "800") == 0)
            ADXL345_REG_WRITE(ADXL345_REG_BW_RATE, XL345_RATE_800);
        else if (strcmp(rate, "400") == 0)
            ADXL345_REG_WRITE(ADXL345_REG_BW_RATE, XL345_RATE_400);
        else if (strcmp(rate, "200") == 0)
            ADXL345_REG_WRITE(ADXL345_REG_BW_RATE, XL345_RATE_200);
        else if (strcmp(rate, "100") == 0)
            ADXL345_REG_WRITE(ADXL345_REG_BW_RATE, XL345_RATE_100);
        else if (strcmp(rate, "50") == 0)
            ADXL345_REG_WRITE(ADXL345_REG_BW_RATE, XL345_RATE_50);
        else if (strcmp(rate, "25") == 0)
            ADXL345_REG_WRITE(ADXL345_REG_BW_RATE, XL345_RATE_25);
        else if (strcmp(rate, "12.5") == 0)
            ADXL345_REG_WRITE(ADXL345_REG_BW_RATE, XL345_RATE_12_5);
        else if (strcmp(rate, "6.25") == 0)
            ADXL345_REG_WRITE(ADXL345_REG_BW_RATE, XL345_RATE_6_25);
        else if (strcmp(rate, "3.13") == 0)
            ADXL345_REG_WRITE(ADXL345_REG_BW_RATE, XL345_RATE_3_13);
        else if (strcmp(rate, "1.56") == 0)
            ADXL345_REG_WRITE(ADXL345_REG_BW_RATE, XL345_RATE_1_56);
        else if (strcmp(rate, "0.78") == 0)
            ADXL345_REG_WRITE(ADXL345_REG_BW_RATE, XL345_RATE_0_78);
        else if (strcmp(rate, "0.39") == 0)
            ADXL345_REG_WRITE(ADXL345_REG_BW_RATE, XL345_RATE_0_39);
        else if (strcmp(rate, "0.20") == 0)
            ADXL345_REG_WRITE(ADXL345_REG_BW_RATE, XL345_RATE_0_20);
        else if (strcmp(rate, "0.10") == 0)
            ADXL345_REG_WRITE(ADXL345_REG_BW_RATE, XL345_RATE_0_10);
        else {
            printk("Invalid rate value\n");
            return -EINVAL;
        }
    }

    return length;
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
