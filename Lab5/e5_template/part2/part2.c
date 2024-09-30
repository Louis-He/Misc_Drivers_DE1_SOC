#include <stdio.h>
#include <stdlib.h>
#include "defines.h"

/**  your part 2 user code here  **/
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

int main(void)
{
    int i;
    printf ("\e[2J"); // clear the screen
    printf ("\e[?25l"); // hide the cursor

    plot_pixel (1, 1, CYAN, 'X');
    plot_pixel (80, 24, CYAN, 'X');
    for (i = 8; i < 18; ++i) {
        plot_pixel (40, i + 12, YELLOW, '*');
    }
    draw_line(0, 140, 0, 37, RED, '+');

    draw_line(0, 80, 0, 37, GREEN, '+');
    draw_line(0, 140, 0, 20, BLUE, '+');

    draw_line(0, 140, 37, 0, MAGENTA, '+');

    getchar (); // wait for user to press return
    printf ("\e[2J"); // clear the screen
    printf ("\e[%2dm", WHITE); // reset foreground color
    printf ("\e[%d;%dH", 1, 1); // move cursor to upper left
    printf ("\e[?25h"); // show the cursor
    fflush (stdout);

    return 0;
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