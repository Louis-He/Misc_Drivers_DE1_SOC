#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "ADXL345.h"
#include "accel_wrappers.h"

#define video_BYTES 8
#define BOX_OFFSET 2
#define BALCK 0x0000
#define RED 0xF800
#define GREEN 0x07E0
#define YELLOW 0xFFE0
#define BLUE 0x001F
#define MAGENTA 0xF81F
#define CYAN 0x07FF
#define WHITE 0xFFFF
#define ACCEL_ALPHA 0.8

static int colors[] = {WHITE, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN};
static int color_index = 0;

struct ball {
    double x;
    double y;

    double a_x;
    double a_y;
};

/**             your part 3 user code here                   **/
/**  hint: you can call functions from ../accel_wrappers.c   **/
void plot_box(int, int, int, int, short int);
void draw_text(int x, int y, char* str);
void catchSIGINT(int);

static int run = 1;
int screen_x, screen_y;
int video_FD; // file descriptor
char video_buffer[video_BYTES]; // buffer for video char data

int main(void) {
    
    signal(SIGINT, catchSIGINT);

    // Open the character device driver
    if ((video_FD = open("/dev/video", O_RDWR)) == -1) {
        printf("Error opening /dev/video: %s\n", strerror(errno));
        return -1;
    }

    read(video_FD, video_buffer, video_BYTES);
    sscanf(video_buffer, "%d %d", &screen_x, &screen_y);
    
    accel_open();
    printf("Opened accel\n");
    accel_init();
    printf("Initialized accel\n");

    int int_source, x, y, z, mg_per_lsb;
    struct ball b = {screen_x / 2, screen_y / 2, 0, 0};

    while (run)
    {
        accel_read(&int_source, &x, &y, &z, &mg_per_lsb);

        if (int_source & XL345_DATAREADY) {
            printf("Int Source: 0x%x, color: %d\n", int_source, color_index);
            if (int_source & XL345_DOUBLETAP) {
                color_index++;
            } else if (int_source & XL345_SINGLETAP) {
                color_index--;
                if (color_index < 0) {
                    color_index = 7 + color_index;
                }
            }
        }

        double x_g = (double)x * mg_per_lsb / 100;
        double y_g = -(double)y * mg_per_lsb / 100;
        double z_g = (double)z * mg_per_lsb / 100;

        // Update the ball's position
        // TODO: Check the sign of the acceleration values
        b.a_x = ACCEL_ALPHA * b.a_x + (1 - ACCEL_ALPHA) * x_g;
        b.a_y = ACCEL_ALPHA * b.a_y + (1 - ACCEL_ALPHA) * y_g;

        b.x += b.a_x;
        b.y += b.a_y;

        if (b.x < 0 + BOX_OFFSET) {
            b.x = BOX_OFFSET;
        } else if (b.x > screen_x - BOX_OFFSET) {
            b.x = screen_x - BOX_OFFSET;
        }

        if (b.y < 0 + BOX_OFFSET) {
            b.y = BOX_OFFSET;
        } else if (b.y > screen_y - BOX_OFFSET) {
            b.y = screen_y - BOX_OFFSET;
        }

        clean_screen();
        plot_box((int)(b.x) - BOX_OFFSET, (int)(b.y) - BOX_OFFSET, (int)(b.x) + BOX_OFFSET, (int)(b.y) + BOX_OFFSET, colors[color_index % 7]);

        char text[10];
        sprintf(text, "x:%2.3lf\0", x_g);
        draw_text(1, 1, text);
        sprintf(text, "y:%2.3lf\0", y_g);
        draw_text(1, 2, text);
        sprintf(text, "z:%2.3lf\0", z_g);
        draw_text(1, 3, text);

        char command[64];
        sprintf (command, "sync\n");
        write (video_FD, command, strlen(command));

        // usleep(100000);
    }

    accel_close();
    printf("Closed accel\n");
    
    clean_screen();

    char command[64]; // buffer for command data
    sprintf (command, "erase\n"); 
    write (video_FD, command, strlen(command));

    sprintf (command, "sync\n");
    write (video_FD, command, strlen(command));
    close(video_FD);
    printf("Closed video\n");
    return 0;
}

void catchSIGINT(int signum) {
    run = 0;
}

void draw_text(int x, int y, char* str) {
    char command[64];
    sprintf (command, "text %d,%d %s\0", x, y, str);
    write (video_FD, command, strlen(command)+1);
}

void clean_screen() {
    char command[64];
    sprintf (command, "clear\n"); 
    write (video_FD, command, strlen(command));
}

void plot_box(int x1, int y1, int x2, int y2, short int color) {
    char command[64];

    sprintf (command, "box %d,%d %d,%d 0x%x\n", x1, y1, x2, y2, color); 
    write (video_FD, command, strlen(command));
}