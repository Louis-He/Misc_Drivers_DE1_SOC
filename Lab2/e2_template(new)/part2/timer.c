#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include "../include/address_map_arm.h"
#include "../include/interrupt_ID.h"


/** you part 2 kernel code here  **/
/* This is a kernel module that uses interrupts from the KEY port. The interrupt service 
 * routine increments a value. The LSB is displayed as a BCD digit on the display HEX0, 
 * and the value is also displayed as a binary number on the red lights LEDR. */


// 7-seg bit patterns for digits 0-9
char seg7[10] =   {0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110, 
                   0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01100111};

void * LW_virtual;             // used to map physical addresses for the light-weight bridge
volatile int * interval_timer_ptr;
volatile int * KEY_ptr;        // virtual address for the KEY port
volatile int * HEX3_HEX0_ptr; // virtual pointer to HEX displays
volatile int * HEX5_HEX4_ptr; // virtual pointer to HEX displays
volatile int *LEDR_ptr;

static int minute = 0;
static int second = 0;
static int millisecond = 0;

irq_handler_t key_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
    // Update the terminal view
    // printk(KERN_ERR "\e[2J\e[10;10H------------\e[11;10H| %d%d:%d%d:%d%d |\e[12;10H------------\n", minute / 10, minute % 10, second / 10, second % 10, millisecond / 10, millisecond % 10);
    printk(KERN_ERR "%d%d:%d%d:%d%d\n", minute / 10, minute % 10, second / 10, second % 10, millisecond / 10, millisecond % 10);
    // Clear the edgecapture register (clears current interrupt)
    *(KEY_ptr + 3) = 0xF;

    return (irq_handler_t) IRQ_HANDLED;
}


void display_on_HEX(void)
{
    int ms_digit, ls_digit;
    int minute_ms_digit, minute_ls_digit;
    int second_ms_digit, second_ls_digit;
    int millisecond_ms_digit, millisecond_ls_digit;

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


void increment_clock(void)
{
    millisecond++;
    if (millisecond == 100)
    {
        millisecond = 0;
        second++;
        if (second == 60)
        {
            second = 0;
            minute++;
            if (minute == 60)
            {
                minute = 0;
            }
        }
    }
}

irq_handler_t timer_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
    // Clear the interrupt
    *(interval_timer_ptr) = *(interval_timer_ptr) & 0x2;

    increment_clock();
    display_on_HEX();

    return (irq_handler_t) IRQ_HANDLED;
}

static int __init initialize_handler(void)
{
    // register the interrupt handler, and then return
    int ret = 0;

    // generate a virtual address for the FPGA lightweight bridge
    LW_virtual = ioremap_nocache (LW_BRIDGE_BASE, LW_BRIDGE_SPAN);

    // Configure the key interrupts
    KEY_ptr = LW_virtual + KEY_BASE;     // init virtual address for KEY port
    // Clear the PIO edgecapture register (clear any pending interrupt)
    *(KEY_ptr + 3) = 0xF; 
    // Enable IRQ generation for the 4 buttons
    *(KEY_ptr + 2) = 0xF; 

    LEDR_ptr = LW_virtual + LEDR_BASE;

    HEX3_HEX0_ptr = LW_virtual + HEX3_HEX0_BASE;   // init virtual address for HEX port
    *HEX3_HEX0_ptr = seg7[0] << 24 | seg7[0] << 16 | seg7[0] << 8 | seg7[0];   // display 0000
    HEX5_HEX4_ptr = LW_virtual + HEX5_HEX4_BASE;   // init virtual address for HEX port
    *HEX5_HEX4_ptr = seg7[0] << 8 | seg7[0];   // display 00

    ret = request_irq (KEY_IRQ, (irq_handler_t) key_irq_handler, IRQF_SHARED, "key_irq_handler", (void *) (key_irq_handler));
    if (ret) {
        printk(KERN_ERR "Failed to request timer IRQ: %d\n", ret);
        free_irq(KEY_IRQ, (void *) key_irq_handler); // Cleanup previously registered IRQ
        return ret; // Early return on failure
    }


    // Configure the internal timer
    interval_timer_ptr = LW_virtual + TIMER0_BASE; // init virtual address for timer
    *(interval_timer_ptr) = 0x0; // stop interval timer, clear any pending interrupts

    // set the interval timer period for .01 second, counter top is 0xF4240
    *(interval_timer_ptr + 2) = 0x4240;
    *(interval_timer_ptr + 3) = 0xF;

    // register the interrupt handler
    ret |= request_irq (TIMER0_IRQ, (irq_handler_t) timer_irq_handler, IRQF_SHARED,
        "timer_irq_handler", (void *) (timer_irq_handler));

    // start the timer and enable interrupts
    *(interval_timer_ptr + 1) = 7; 

    return ret;
}

static void __exit cleanup_handler(void)
{
   *LEDR_ptr = 0; // Turn off LEDs and 7-seg display
   *HEX3_HEX0_ptr = 0;
   *HEX5_HEX4_ptr = 0;
   iounmap (LW_virtual);
   free_irq (KEY_IRQ, (void*) key_irq_handler); // De-register irq handler
   free_irq (TIMER0_IRQ, (void*) timer_irq_handler);
}

module_init (initialize_handler);
module_exit (cleanup_handler);

