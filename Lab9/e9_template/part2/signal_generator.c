#include <linux/fs.h> // struct file, struct file_operations
#include <linux/init.h> // for __init, see code
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h> // for misc_device_register and struct miscdev
#include <linux/uaccess.h> // for copy_to_user, see code
#include <linux/init.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include "../include/address_map_arm.h"
#include "../include/interrupt_ID.h"

// Declare variables and prototypes needed for a character device driver
static int signal_generator_open (struct inode *, struct file *);
static int signal_generator_release (struct inode *, struct file *);
static ssize_t signal_generator_read (struct file *, char *, size_t, loff_t *);
static ssize_t signal_generator_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations signal_generator_dev_fops = {
    .owner = THIS_MODULE,
    .open = signal_generator_open,
    .release = signal_generator_release
};

#define SUCCESS 0
#define SIGNAL_GENERATOR_DEV_NAME "singal_generator"

static struct miscdevice signal_generator_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = SIGNAL_GENERATOR_DEV_NAME,
    .fops = &signal_generator_dev_fops,
    .mode = 0666
};

static int signal_generator_dev_registered = 0;
void *LW_virtual; // used to access FPGA light-weight bridge
volatile int * interval_timer_ptr;
volatile int * SW_ptr;
volatile int * LEDR_ptr;
volatile int * HEX_ptr;
volatile int * GPIO_ptr;

static int output_signal = 0;

int timer_counter_value[16] = {
    5000000,
    2500000,
    1666666,
    1250000,
    1000000,
    833333,
    714285,
    625000,
    555555,
    500000,
    454545,
    416666,
    384615,
    357142,
    333333,
    312500
};
// 7-seg bit patterns for digits 0-9
char seg7[10] =   {0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110, 
                   0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01100111};


irq_handler_t timer_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
    // Clear the interrupt
    *(interval_timer_ptr) = *(interval_timer_ptr) & 0x2;

    output_signal = output_signal ? 0 : 1;
    *GPIO_ptr = (output_signal & 0x1);

    // read from SW switches from 9->6 bits
    int SW_value = *SW_ptr;
    SW_value = SW_value >> 6;
    *(LEDR_ptr) = SW_value;

    // convert SW value to decimal
    int freq_decimal = 0;
    int tmp_sw = SW_value;
    int MSB_label = 0x1 << 3;
    int i;
    for (i = 0; i < 4; i++) {
        freq_decimal += (tmp_sw & MSB_label);
        MSB_label = MSB_label >> 1;
    }
    freq_decimal = (freq_decimal + 1) * 10;

    // display the SW value on HEX3_HEX0
    *HEX_ptr = (seg7[(freq_decimal / 100) % 10] << 16) | (seg7[(freq_decimal / 10) % 10] << 8) | seg7[freq_decimal % 10];

    // set the interval timer period
    *(interval_timer_ptr) = 0x0; // stop interval timer, clear any pending interrupts
    *(interval_timer_ptr + 2) = timer_counter_value[SW_value] & 0xFFFF;
    *(interval_timer_ptr + 3) = timer_counter_value[SW_value] >> 16;
    *(interval_timer_ptr + 1) = 7; // start the timer and enable interrupts

    
    return (irq_handler_t) IRQ_HANDLED;
}

static int __init start_signal_generator(void) {
    int err = misc_register (&signal_generator_dev);
    if (err < 0) {
        printk (KERN_ERR "misc_register failed\n");
    }

    // generate a virtual address for the FPGA lightweight bridge
    LW_virtual = ioremap_nocache (0xFF200000, 0x00005000);

    if (LW_virtual == 0) { 
        printk (KERN_ERR "Error: ioremap_nocache returned NULL\n");
    } else {
        signal_generator_dev_registered = 1;
    }

    // set the switch, hex, ledr virtual address
    SW_ptr = LW_virtual + SW_BASE;
    LEDR_ptr = LW_virtual + LEDR_BASE;
    HEX_ptr = LW_virtual + HEX3_HEX0_BASE;

    // set the GPIO virtual address and set pin0 as output
    GPIO_ptr = LW_virtual + GPIO0_BASE;
    *(GPIO_ptr + 1) = 0xFFFF;

    // configure the internal timer
    interval_timer_ptr = LW_virtual + TIMER0_BASE;
    *(interval_timer_ptr) = 0x0; // stop interval timer, clear any pending interrupts

    // set the interval timer period for .01 second, counter top is 0xF4240 (100 MHz)
    *(interval_timer_ptr + 2) = 0x4240;
    *(interval_timer_ptr + 3) = 0xF;

    // register the interrupt handler
    err = request_irq (TIMER0_IRQ, (irq_handler_t) timer_irq_handler, IRQF_SHARED, "timer_irq_handler", (void *) (timer_irq_handler));

    // start the timer and enable interrupts
    *(interval_timer_ptr + 1) = 7;

    return 0;
}

static int __init stop_signal_generator(void) {
    *(interval_timer_ptr) = 0x0; // stop interval timer, clear any pending interrupts

    printk("Stopping signal generator\n");
    if (signal_generator_dev_registered) {
        misc_deregister (&signal_generator_dev);
        signal_generator_dev_registered = 0;
    }

    printk("Unmap LW_virtual\n");
    iounmap (LW_virtual);

    printk("Freeing IRQ\n");
    free_irq (TIMER0_IRQ, (void*) timer_irq_handler);
    printk("Freed IRQ\n");

}

static int signal_generator_open(struct inode *inode, struct file *file) {
    return SUCCESS;
}

static int signal_generator_release(struct inode *inode, struct file *file) {
    return 0;
}


module_init (start_signal_generator);
module_exit (stop_signal_generator);