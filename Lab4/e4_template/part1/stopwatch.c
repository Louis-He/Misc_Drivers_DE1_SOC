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
static int device_open (struct inode *, struct file *);
static int device_release (struct inode *, struct file *);
static ssize_t device_read (struct file *, char *, size_t, loff_t *);

static struct file_operations stopwatch_dev_fops = {
    .owner = THIS_MODULE,
    .read = device_read,
    .open = device_open,
    .release = device_release
};

#define SUCCESS 0

#define STOPWATCH_DEV_NAME "STOPWATCH"

void * LW_virtual;             // used to map physical addresses for the light-weight bridge
volatile int* TIMER0_ptr;

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

irq_handler_t timer_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
    // printk(KERN_INFO "Timer Interrupt called\n");
    // Clear the interrupt
    *(TIMER0_ptr) = *(TIMER0_ptr) & 0x2;

    decrement_clock();

    return (irq_handler_t) IRQ_HANDLED;
}


static int __init start_stopwatch_dev(void)
{
    // generate a virtual address for the FPGA lightweight bridge
    LW_virtual = ioremap_nocache (LW_BRIDGE_BASE, LW_BRIDGE_SPAN);
    
    int err = misc_register (&stopwatch_dev);
    if (err < 0) {
        printk (KERN_ERR "/dev/%s: misc_register() failed\n", STOPWATCH_DEV_NAME);
    }
    else {
        printk (KERN_INFO "/dev/%s driver registered\n", STOPWATCH_DEV_NAME);
        stopwatch_dev_registered = 1;

        TIMER0_ptr = LW_virtual + TIMER0_BASE;
    }

        // register the interrupt handler
    err |= request_irq (TIMER0_IRQ, (irq_handler_t) timer_irq_handler, IRQF_SHARED,
        "timer_irq_handler", (void *) (timer_irq_handler));

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

MODULE_LICENSE("GPL");
module_init (start_stopwatch_dev);
module_exit (stop_stopwatch_dev);