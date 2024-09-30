#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>

/**  your part 3 user code here  **/
#define BALCK 30
#define RED 31
#define GREEN 32
#define YELLOW 33
#define BLUE 34
#define MAGENTA 35
#define CYAN 36
#define WHITE 37

void swap(int* x, int* y);
void plot_pixel(int, int, char, char);
void draw_line(int x0, int x1, int y0, int y1, char color, char c);
void clean_screen(void);
void catchSIGINT(int signum);

int run = 1;

int main(void)
{
    int i;
    int y = 0;
    int direction = 1;
    printf ("\e[2J"); // clear the screen
    printf ("\e[?25l"); // hide the cursor

    signal(SIGINT, catchSIGINT);

    while (run) {
        clean_screen();
        draw_line(0, 140, y, y, GREEN, '+');
        nanosleep((const struct timespec[]){{0, 100000000L}}, NULL);

        y += direction;
        if (y == 40) {
            direction = -1;
        } else if (y == 0) {
            direction = 1;
        }
    }

    return 0;
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

void catchSIGINT(int signum) {
    printf ("\e[2J"); // clear the screen
    printf ("\e[%2dm", WHITE); // reset foreground color
    printf ("\e[%d;%dH", 1, 1); // move cursor to upper left
    printf ("\e[?25h"); // show the cursor
    fflush (stdout);
    run = 0;
}