#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include "defines.h"
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
/**  your part 4 user code here  **/
volatile sig_atomic_t stop; // used to exit the program cleanly
int num_points = 5;
struct point points [100];

void catchSIGINT(int);
void draw_figure(int draw_lines);
void swap(int* x, int* y);
void plot_pixel(int, int, char, char);
void draw_line(int x0, int x1, int y0, int y1, char color, char c);
void clean_screen(void);
void init_points();
void init_point(int idx);
void update_points();
struct point generate_random_point();
struct timespec animation_time; // used to control the animation timing
/*******************************************************************************
* This program draws Xs and lines on the screen, in an animated fashion
******************************************************************************/
int main(void)
{
    srand(time(NULL));

    // catch SIGINT from ^C, instead of having it abruptly close this program
    signal(SIGINT, catchSIGINT);
    // // set random initial position,
    printf ("\e[2J"); // clear the screen
    printf ("\e[?25l"); // hide the cursor
    fflush (stdout);
    // initialize the animation speed
    animation_time.tv_sec = 0;
    animation_time.tv_nsec = 200000000;

    init_points();
    while (!stop) {
        char key_value[2];
        int key;
        int key_fd = open("/dev/KEY", (O_RDWR | O_SYNC));
        if (key_fd == -1) {
            printf("Error opening /dev/KEY: %s\n", strerror(errno));
            return -1;
        }
        read(key_fd, key_value, 2);
        close(key_fd);

        if (key_value[0] >= '0' && key_value[0] <= '9') {
            key = key_value[0] - '0';
        } else if (key_value[0] >= 'A' && key_value[0] <= 'F') {
            key = key_value[0] - 'A' + 10;
        } else if (key_value[0] >= 'a' && key_value[0] <= 'f') {
            key = key_value[0] - 'a' + 10;
        } else {
            printf("Invalid key value: %c\n", key_value[0]);
            continue;
        }

        if ((key & 0x1) != 0) {
            // animation_time set to be halved
            animation_time.tv_nsec /= 2;
        } else if ((key & 0x2) != 0) {
            // animation_time set to be doubled
            animation_time.tv_nsec *= 2;
        } else if ((key & 0x4) != 0) {
            // Add a point
            num_points++;
            init_point(num_points - 1);
        } else if ((key & 0x8) != 0) {
            // Remove a point
            if (num_points > 1) {
                num_points--;
            }
        }

        int sw_fd = open("/dev/SW", (O_RDWR | O_SYNC));
        char sw_value[4];
        int sw_int = 0;
        if (sw_fd == -1) {
            printf("Error opening /dev/SW: %s\n", strerror(errno));
            return -1;
        }
        read(sw_fd, sw_value, 4);
        close(sw_fd);
        int i = 0;
        for (i = 0; i < 3; i++) {
            if (sw_value[i] >= '0' && sw_value[i] <= '9') {
                sw_int = (sw_int << 4) | (sw_value[i] - '0');
            } else if (sw_value[i] >= 'A' && sw_value[i] <= 'F') {
                sw_int = (sw_int << 4) | (sw_value[i] - 'A' + 10);
            } else if (sw_value[i] >= 'a' && sw_value[i] <= 'f') {
                sw_int = (sw_int << 4) | (sw_value[i] - 'a' + 10);
            }
        }

        draw_figure(sw_int == 0);
        update_points();
        nanosleep (&animation_time, NULL); // hide the cursor
        // 0.2 seconds
        // draw the animation
        // wait for timer
    }
    printf ("\e[2J"); // clear the screen
    printf ("\e[%2dm", WHITE); // reset foreground color
    printf ("\e[%d;%dH", 1, 1); // move cursor to upper left
    printf ("\e[?25h"); // show the cursor
    fflush (stdout);
    return 0;
}
/* Function to allow clean exit of the program */
void catchSIGINT(int signum) {
    stop = 1;
}

void init_points() {
    int i = 0;
    for (i = 0; i < num_points; i++) {
        struct point p = generate_random_point();
        points[i] = p;
    }
}

void init_point(int idx) {
    points[idx] = generate_random_point();
}

void draw_figure(int draw_lines) {
    int i = 0;
    clean_screen();

    if (draw_lines) {
        for (i = 0; i < num_points - 1; i++) {
            struct point p1 = points[i];
            struct point p2 = points[(i + 1) % num_points];
            draw_line(p1.x, p2.x, p1.y, p2.y, WHITE, '+');
        }
    }

    for (i = 0; i < num_points; i++) {
        struct point p = points[i];
        plot_pixel(p.x, p.y, CYAN, 'X');
    }

}

void update_points() {
    int i = 0;
    for (i = 0; i < num_points; i++) {
        struct point p = points[i];
        p.x += p.direction_x;
        p.y += p.direction_y;
        if (p.x == 0 || p.x == X_MAX) {
            p.direction_x *= -1;
        }
        if (p.y == 0 || p.y == Y_MAX) {
            p.direction_y *= -1;
        }
        points[i] = p;
    }
}


void clean_screen() {
    printf ("\e[2J"); // clear the screen
    fflush (stdout); 
}

void plot_pixel(int x, int y, char color, char c) {
    printf ("\e[%2dm\e[%d;%dH%c", color, y, x, c);
    fflush (stdout);
}

void draw_line(int x0, int x1, int y0, int y1, char color, char c) {
    int is_steep = (abs(y1 - y0) > abs(x1 - x0));
    if (is_steep) {
        swap(&x0, &y0);
        swap(&x1, &y1);
    }
    if (x0 > x1) {
        swap(&x0, &x1);
        swap(&y0, &y1);
    }

    int deltax = x1 - x0;
    int deltay = abs(y1 - y0);
    double error = -(deltax / 2);
    int y = y0;
    int y_step;
    if (y0 < y1) {
        y_step = 1;
    } else {
        y_step = -1;
    }

    int x;
    for (x = x0; x <= x1; x++) {
        if (is_steep) {
            plot_pixel(y, x, color, c);
        } else {
            plot_pixel(x, y, color, c);
        }
        error = error + deltay;
        if (error >= 0) {
            y = y + y_step;
            error = error - deltax;
        }
    }
}

void swap(int* x, int* y) {
    int temp = *x;
    *x = *y;
    *y = temp;
}

struct point generate_random_point() {
    struct point p;
    p.x = rand() % X_MAX;
    p.y = rand() % Y_MAX;
    p.direction_x = rand() % 2 == 0 ? -1 : 1;
    p.direction_y = rand() % 2 == 0 ? -1 : 1;
    return p;
}