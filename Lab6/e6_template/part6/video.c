#include <linux/fs.h> // struct file, struct file_operations
#include <linux/init.h> // for __init, see code
#include <linux/module.h> // for module init and exit macros
#include <linux/miscdevice.h> // for misc_device_register and struct miscdev
#include <linux/uaccess.h> // for copy_to_user, see code
#include <linux/string.h>
#include <asm/io.h> // for mmap
#include "../include/address_map_arm.h"

// Declare variables and prototypes needed for a character device driver
static int video_open (struct inode *, struct file *);
static int video_release (struct inode *, struct file *);
static ssize_t video_read (struct file *, char *, size_t, loff_t *);
static ssize_t video_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations video_dev_fops = {
    .owner = THIS_MODULE,
    .read = video_read,
    .write = video_write, 
    .open = video_open,
    .release = video_release
};

#define SUCCESS 0
#define VIDEO_DEV_NAME "video"

static struct miscdevice video_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = VIDEO_DEV_NAME,
    .fops = &video_dev_fops,
    .mode = 0666
};

void swap_int(int *x, int *y);
void get_screen_specs(volatile int * pixel_ctrl_ptr);
void get_screen_char_spaces(volatile int * char_buffer_ctrl_ptr);
void clear_screen(void);
void clear_char_buffer(void);
void plot_line(int x0, int x1, int y0, int y1, short int color);
void plot_pixel(int x, int y, short int color);
void plot_box(int x1, int y1, int x2, int y2, short int color);
void plot_char(int, int, char);
void plot_chars(int, int, char*);
void sync_video(void);

static int video_dev_registered = 0;
// Declare global variables needed to use the pixel buffer
void *LW_virtual; // used to access FPGA light-weight bridge
volatile int * pixel_ctrl_ptr; // virtual address of pixel buffer controller
volatile int * char_buffer_ctrl_ptr; // virtual address of character buffer controller
int front_buffer; // used for virtual address of the front buffer
int back_buffer; // used for virtual address of the back buffer
int pixel_buffer; // used for virtual address of pixel buffer
int char_buffer; // used for virtual address of character buffer
int resolution_x, resolution_y; // VGA screen size
int char_resolution_x, char_resolution_y; // VGA char buffer size

/* Code to initialize the video driver */
static int __init start_video(void){
    printk("Starting video driver\n");
    // initialize the dev_t, cdev, and class data structures
    int err = misc_register (&video_dev);
    if (err < 0) {
        printk (KERN_ERR "misc_register failed\n");
    }

    // generate a virtual address for the FPGA lightweight bridge
    LW_virtual = ioremap_nocache (0xFF200000, 0x00005000);

    if (LW_virtual == 0) { 
        printk (KERN_ERR "Error: ioremap_nocache returned NULL\n");
    } else {
        video_dev_registered = 1;
    }

    // Create virtual memory access to the pixel buffer controller
    pixel_ctrl_ptr = (unsigned int *) (LW_virtual + 0x00003020);
    *(pixel_ctrl_ptr + 1) = (0xC8000000); 
    sync_video();
    *(pixel_ctrl_ptr + 1) = (0xC0000000);
 
    get_screen_specs (pixel_ctrl_ptr); // determine X, Y screen size

    char_buffer_ctrl_ptr = (unsigned int *) (LW_virtual + 0x00003030);
    get_screen_char_spaces (char_buffer_ctrl_ptr);

    // Create virtual memory access to the pixel buffer
    // allocate front buffer
    front_buffer = (int) ioremap_nocache (0xC8000000, 0x0003FFFF);
    if (front_buffer == 0)
        printk (KERN_ERR "Error: ioremap_nocache returned NULL\n");
    
    // allocate back buffer
    back_buffer = (int) ioremap_nocache (0xC0000000, 0x0003FFFF);
    if (back_buffer == 0)
        printk (KERN_ERR "Error: ioremap_nocache returned NULL\n");
    pixel_buffer = back_buffer;

    char_buffer = (int) ioremap_nocache (0xC9000000, 0x00001FFF);
    if (char_buffer == 0)
        printk (KERN_ERR "Error: ioremap_nocache returned NULL\n");
    
    /* Erase the pixel buffer */
    clear_screen ();
    return 0;
}

static void __exit stop_video(void){
    clear_screen();

    *(pixel_ctrl_ptr) = (0xC8000000); 
    *(pixel_ctrl_ptr + 1) = (0xC8000000); 

    /* unmap the physical-to-virtual mappings */
    iounmap (LW_virtual);
    iounmap ((void *) pixel_buffer);

    pixel_ctrl_ptr = NULL;
    pixel_buffer = 0;

    /* Remove the device from the kernel */
    if (video_dev_registered) {
        misc_deregister (&video_dev);
    }
}

void get_screen_specs(volatile int * pixel_ctrl_ptr) {
    int screen_size = *(pixel_ctrl_ptr + 2);
    resolution_x = screen_size & 0xFFFF;
    resolution_y = (screen_size >> 16) & 0xFFFF;

    printk("Pixel resolution: %d, %d\n", resolution_x, resolution_y);
}

void get_screen_char_spaces(volatile int * char_buffer_ctrl_ptr) {
    int screen_char_size = *(char_buffer_ctrl_ptr + 2);
    char_resolution_x = screen_char_size & 0xFFFF;
    char_resolution_y = (screen_char_size >> 16) & 0xFFFF;

    printk("Char resolution: %d, %d\n", char_resolution_x, char_resolution_y);
}

void clear_screen(void) {
    int x, y;
    for (x = 0; x < resolution_x; x++){
        for (y = 0; y < resolution_y; y++){
            plot_pixel(x, y, 0);
        }
    }
}

void clear_char_buffer(void) {
    int x, y;
    for (x = 0; x < char_resolution_x; x++){
        for (y = 0; y < char_resolution_y; y++){
            plot_chars(x, y, " ");
        }
    }
}

void plot_line(int x0, int x1, int y0, int y1, short int color) {
    int is_steep = (abs(y1 - y0) > abs(x1 - x0));
    int deltax;
    int deltay;
    int error;
    int y;
    int y_step;
    int x;

    printk("Plotting line of color %x on %d, %d -> %d, %d\n", color, x0, y0, x1, y1);

    if (is_steep) {
        swap_int(&x0, &y0);
        swap_int(&x1, &y1);
    }
    if (x0 > x1) {
        swap_int(&x0, &x1);
        swap_int(&y0, &y1);
    }

    deltax = x1 - x0;
    deltay = abs(y1 - y0);
    error = -(deltax / 2);
    y = y0;

    if (y0 < y1) {
        y_step = 1;
    } else {
        y_step = -1;
    }

    for (x = x0; x <= x1; x++) {
        if (is_steep) {
            plot_pixel(y, x, color);
        } else {
            plot_pixel(x, y, color);
        }
        error = error + deltay;
        if (error >= 0) {
            y = y + y_step;
            error = error - deltax;
        }
    }
}

void swap_int(int* x, int* y) {
    int temp = *x;
    *x = *y;
    *y = temp;
}

void plot_pixel(int x, int y, short int color) {
    // printk("Plotting color %x on %d, %d\n", color, x, y);
    int address = pixel_buffer + (((x & 0x1FF) << 1) | ((y & 0xFF) << 10));
    *(short int*)(address) = color;

    // printk("address: %x\n", address);
}

void plot_box(int x1, int y1, int x2, int y2, short int color) {
    printk("Plotting box %x on (%d, %d) -> (%d, %d)\n", color, x1, y1, x2, y2);
    int x, y;
    for (x = x1; x <= x2; x++) {
        for (y = y1; y <= y2; y++) {
            plot_pixel(x, y, color);
        }
    }
}

void plot_char(int x, int y, char c) {
    int address = char_buffer + (x & 0x7F) + ((y & 0x3F) << 7);
    *(volatile char*)(address) = c;
}

void plot_chars(int x, int y, char* str) {
    printk("Plotting chars %s on %d, %d\n", str, x, y);
    int cur = 0;
    int cur_x = x;
    int cur_y = y;
    while (str[cur] != '\0') {
        plot_char(cur_x, cur_y, str[cur]);
        cur++;
        cur_x++;
        
        if (cur_x >= char_resolution_x) {
            cur_x = 0;
            cur_y++;
        }

        if (cur_y >= char_resolution_y) {
            break;
        }
    }
}

void sync_video (void) {
    volatile int * status_ptr = pixel_ctrl_ptr + 3;
    int buffer_register = *(pixel_ctrl_ptr);

    printk("sync\n");
    *(pixel_ctrl_ptr) = buffer_register | 0x1;

    while ((*(status_ptr) & 0x1) != 0) {
        // busy-wait
        // printk("Waiting for sync\n");
    }

    pixel_buffer = (pixel_buffer == back_buffer) ? front_buffer : back_buffer;
    printk("sync done,changed pixel buffer to: 0x%x\n", pixel_buffer);
}


static int video_open(struct inode *inode, struct file *file){
    return SUCCESS;
}

static int video_release(struct inode *inode, struct file *file){
    return 0;
}

static ssize_t video_read(struct file *filp, char *buffer, size_t length, loff_t *offset){
    char msg[8];

    if (*offset == 8)
        return 0;

    if (length < 8)
        return -EINVAL;

    sprintf(msg, "%3d %3d", resolution_x, resolution_y);
    if (copy_to_user(buffer, msg, 8)) {
        return -EFAULT;
    }

    *offset = 8;
    return 8;
}

static ssize_t video_write(struct file *filp, const char *buffer, size_t length, loff_t *offset){
    char msg[length];
    if (copy_from_user(msg, buffer, length)) {
        return -EFAULT;
    }
    msg[length] = '\0';

    if (strncmp(msg, "clear", 5) == 0){
        clear_screen();
    } else if (strncmp(msg, "pixel", 5) == 0) {
        char *token = msg + 6;
        int x, y, color;
        sscanf(token, "%d,%d 0x%x", &x, &y, &color);
        plot_pixel(x, y, color);

        // *(pixel_ctrl_ptr+3) =  *(pixel_ctrl_ptr+3) | 0x1;
    } else if (strncmp(msg, "line", 4) == 0) {
        char *token = msg + 5;
        int x0, x1, y0, y1, color;
        sscanf(token, "%d,%d %d,%d 0x%x", &x0, &y0, &x1, &y1, &color);
        plot_line(x0, x1, y0, y1, color);
    } else if (strncmp(msg, "sync", 4) == 0) {
        sync_video();
    } else if (strncmp(msg, "box", 3) == 0) {
        char *token = msg + 4;
        int x0, x1, y0, y1, color;
        sscanf(token, "%d,%d %d,%d 0x%x", &x0, &y0, &x1, &y1, &color);
        plot_box(x0, y0, x1, y1, color);
    } else if (strncmp(msg, "erase", 5) == 0) {
        clear_char_buffer();
    } else if (strncmp(msg, "text", 4) == 0) {
        char *token = msg + 5;
        int x, y;
        char c[length];

        sscanf(token, "%d,%d %s", &x, &y, &c);
        plot_chars(x, y, c);
    }

    return length;
}

MODULE_LICENSE("GPL");
module_init (start_video);
module_exit (stop_video);
