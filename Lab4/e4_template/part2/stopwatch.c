#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>               // struct file, struct file_operations
#include <linux/init.h>             // for __init, see code
#include <linux/module.h>           // for module init and exit macros
#include <linux/miscdevice.h>       // for misc_device_register and struct miscdev
#include <linux/uaccess.h>          // for copy_to_user, see code
#include <asm/io.h>                 // for mmap
#include "../include/address_map_arm.h"
#include "../include/interrupt_ID.h"


/**  your part 1 kernel code here  **/
// 7-seg bit patterns for digits 0-9
char seg7[10] =   {0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110, 
                   0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01100111};


static int device_open (struct inode *, struct file *);
static int device_release (struct inode *, struct file *);
static ssize_t device_read (struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations stopwatch_dev_fops = {
    .owner = THIS_MODULE,
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release
};

#define SUCCESS 0

#define STOPWATCH_DEV_NAME "stopwatch"

void * LW_virtual;             // used to map physical addresses for the light-weight bridge
volatile int* TIMER0_ptr;
volatile int * HEX3_HEX0_ptr; // virtual pointer to HEX displays
volatile int * HEX5_HEX4_ptr; // virtual pointer to HEX displays

static struct miscdevice stopwatch_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = STOPWATCH_DEV_NAME,
    .fops = &stopwatch_dev_fops,
    .mode = 0666
};

static int stopwatch_dev_registered = 0;

int minute = 59;
int second = 59;
int millisecond = 99;
int run = 0;
int disp = 0;

void decrement_clock(void)
{
    millisecond--;
    if (millisecond == -1)
    {
        millisecond = 99;
        second--;
        if (second == -1)
        {
            second = 59;
            minute--;
            if (minute == -1)
            {
                minute = 0;
                second = 0;
                millisecond = 0;
            }
        }
    }
}

void display_on_HEX(void)
{
    int ms_digit, ls_digit;
    int minute_ms_digit, minute_ls_digit;
    int second_ms_digit, second_ls_digit;
    int millisecond_ms_digit, millisecond_ls_digit;

    // printk(KERN_INFO "Displaying on HEX: %02d:%02d:%02d\n", minute, second, millisecond);

    // Calculate digits for minute
    minute_ms_digit = minute / 10;
    minute_ls_digit = minute % 10;

    // Calculate digits for second
    second_ms_digit = second / 10;
    second_ls_digit = second % 10;

    // Calculate digits for millisecond
    millisecond_ms_digit = millisecond / 10;
    millisecond_ls_digit = millisecond % 10;

    // Set the HEX display for milliseconds and seconds
    *HEX3_HEX0_ptr = (seg7[second_ms_digit] << 24) | (seg7[second_ls_digit] << 16) |
                     (seg7[millisecond_ms_digit] << 8) | seg7[millisecond_ls_digit];

    // Set the HEX display for minutes
    *HEX5_HEX4_ptr = (seg7[minute_ms_digit] << 8) | seg7[minute_ls_digit];
}

irq_handler_t timer_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
    // printk(KERN_INFO "Timer Interrupt called\n");
    // Clear the interrupt
    *(TIMER0_ptr) = *(TIMER0_ptr) & 0x2;

    decrement_clock();

    if (disp) {
        display_on_HEX();
    }

    return (irq_handler_t) IRQ_HANDLED;
}

static int __init start_stopwatch_dev(void)
{
    // generate a virtual address for the FPGA lightweight bridge
    LW_virtual = ioremap_nocache (LW_BRIDGE_BASE, LW_BRIDGE_SPAN);
    
    int err = misc_register (&stopwatch_dev);
    if (err < 0) {
        printk (KERN_ERR "/dev/%s: misc_register() failed\n", STOPWATCH_DEV_NAME);
    } else {
        printk (KERN_INFO "/dev/%s driver registered\n", STOPWATCH_DEV_NAME);
        stopwatch_dev_registered = 1;

        TIMER0_ptr = LW_virtual + TIMER0_BASE;
        HEX3_HEX0_ptr = LW_virtual + HEX3_HEX0_BASE;
        HEX5_HEX4_ptr = LW_virtual + HEX5_HEX4_BASE;
    }

    *(TIMER0_ptr) = 0x0; // stop interval timer, clear any pending interrupts
    // set the interval timer period for .01 second, counter top is 0xF4240
    *(TIMER0_ptr + 2) = 0x4240;
    *(TIMER0_ptr + 3) = 0xF;

    // register the interrupt handler
    err |= request_irq (TIMER0_IRQ, (irq_handler_t) timer_irq_handler, IRQF_SHARED,
        "timer_irq_handler", (void *) (timer_irq_handler));

    // Enable interrupts and start the timer
    *(TIMER0_ptr + 1) = 3 | (1 << 2);
    *(HEX3_HEX0_ptr) = 0x00000000;    
    *(HEX5_HEX4_ptr) = 0x00000000;

    return err;
}

static int __exit stop_stopwatch_dev(void)
{
    iounmap (LW_virtual);
    free_irq (TIMER0_IRQ, (void*) timer_irq_handler);
    if (stopwatch_dev_registered) {
        misc_deregister (&stopwatch_dev);
        printk (KERN_INFO "/dev/%s driver de-registered\n", STOPWATCH_DEV_NAME);

        TIMER0_ptr = NULL;
        HEX3_HEX0_ptr = NULL;
        HEX5_HEX4_ptr = NULL;
    }

    return 0;
}

static int device_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "Opened device: %s\n", STOPWATCH_DEV_NAME);
    return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "Closed device: %s\n", STOPWATCH_DEV_NAME);
    return 0;
}

static ssize_t device_read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
    if (length < 10)
    {
        return -EINVAL;
    }

    if (*offset == 10)
    {
        return 0;
    }

    char time[10];
    sprintf(time, "%02d:%02d:%02d", minute, second, millisecond);
    copy_to_user(buffer, time, 10);
    *offset = 10;
    return 10;
}

static ssize_t device_write(struct file *filp, const char *buffer, size_t length, loff_t *offset)
{
    size_t bytes;
    char command[10];

    printk(KERN_INFO "Received command length %d: %s\n", length, buffer);

    if (length > 10)
    {
        return -EINVAL;
    }
    copy_from_user(command, buffer, length);

    if (strncmp(command, "run", 3) == 0) {
        run = 1;
        *(TIMER0_ptr + 1) = 0x7; // start timer, enable CONT and interrupts
        printk(KERN_INFO "Started stopwatch\n");
    } else if (strncmp(command, "stop", 4) == 0) {
        run = 0;
        *(TIMER0_ptr + 1) = 0x8; // stop the timer and everything
        printk(KERN_INFO "Stopped stopwatch\n");
    } else if (strncmp(command, "disp", 4) == 0) {
        disp = 1;
        display_on_HEX();
    } else if (strncmp(command, "nodisp", 6) == 0) {
        disp = 0;
        *(HEX3_HEX0_ptr) = 0x00000000;
        *(HEX5_HEX4_ptr) = 0x00000000;
    } else {
        // set time
        printk(KERN_INFO "Setting time to %s\n", command);
        sscanf(command, "%d:%d:%d", &minute, &second, &millisecond);

        if (second > 59) {
            second = 59;
        }
        if (millisecond > 99) {
            millisecond = 99;
        }

        if (disp) {
            display_on_HEX();
        }
        printk(KERN_INFO "Set time to %02d:%02d:%02d\n", minute, second, millisecond);
    }

    bytes = length;
    return bytes;
}

MODULE_LICENSE("GPL");
module_init (start_stopwatch_dev);
module_exit (stop_stopwatch_dev);