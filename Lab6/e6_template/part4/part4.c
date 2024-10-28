#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#define NUMBER_OF_POINTS 3 
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

struct point {
    int x;
    int y;

    int direction_x;
    int direction_y;
};

int screen_x, screen_y;
int video_FD; // file descriptor
char video_buffer[video_BYTES]; // buffer for video char data

/**  your part 4 user code here  **/
volatile sig_atomic_t stop; // used to exit the program cleanly
void catchSIGINT(int);
void draw_figure();
void swap(int* x, int* y);
void plot_pixel(int, int, char);
void plot_box(int, int, int, int, short int);
void draw_line(int x0, int x1, int y0, int y1, short int color);
void clean_screen(void);
void init_points();
void update_points();
struct point generate_random_point();
struct timespec animation_time; // used to control the animation timing
struct point points [NUMBER_OF_POINTS];

int main(void)
{
    char command[64]; // buffer for command data

    // Open the character device driver
    if ((video_FD = open("/dev/video", O_RDWR)) == -1) {
        printf("Error opening /dev/video: %s\n", strerror(errno));
        return -1;
    }

    read(video_FD, video_buffer, video_BYTES);
    sscanf(video_buffer, "%d %d", &screen_x, &screen_y);

    srand(time(NULL));
    // catch SIGINT from ^C, instead of having it abruptly close this program
    signal(SIGINT, catchSIGINT);
    // initialize the animation speed
    animation_time.tv_sec = 0;
    animation_time.tv_nsec = 200000000;

    init_points();
    while (!stop) {
        draw_figure();
        update_points();
    }

    clean_screen();
    sprintf (command, "sync\n");
    write (video_FD, command, strlen(command));
    
    close(video_FD);
    return 0;
}

/* Function to allow clean exit of the program */
void catchSIGINT(int signum) {
    stop = 1;
}

void init_points() {
    int i = 0;
    for (i = 0; i < NUMBER_OF_POINTS; i++) {
        struct point p = generate_random_point();
        points[i] = p;
    }
}

void draw_figure() {
    int i = 0;
    clean_screen();

    for (i = 0; i < NUMBER_OF_POINTS - 1; i++) {
        struct point p1 = points[i];
        struct point p2 = points[(i + 1) % NUMBER_OF_POINTS];

        draw_line(p1.x, p2.x, p1.y, p2.y, WHITE);
    }

    for (i = 0; i < NUMBER_OF_POINTS; i++) {
        struct point p = points[i];
        plot_box(p.x - BOX_OFFSET, p.y - BOX_OFFSET, p.x + BOX_OFFSET, p.y + BOX_OFFSET, WHITE);
    }

    char command[64];
    sprintf (command, "sync\n");
    write (video_FD, command, strlen(command));
}

void update_points() {
    int i = 0;
    for (i = 0; i < NUMBER_OF_POINTS; i++) {
        struct point p = points[i];
        p.x += p.direction_x;
        p.y += p.direction_y;
        if (p.x == BOX_OFFSET || p.x == screen_x - BOX_OFFSET - 1) {
            p.direction_x *= -1;
        }
        if (p.y == BOX_OFFSET || p.y == screen_y - BOX_OFFSET - 1) {
            p.direction_y *= -1;
        }
        points[i] = p;
    }
}

void clean_screen() {
    // printf ("\e[2J"); // clear the screen
    // fflush (stdout); 
    char command[64];
    sprintf (command, "clear\n"); 
    write (video_FD, command, strlen(command));
}

void plot_pixel(int x, int y, char color) {
    char command[64];
    sprintf (command, "pixel %d,%d 0x%x\n", x, y, color); 
    write (video_FD, command, strlen(command));
}

void plot_box(int x1, int y1, int x2, int y2, short int color) {
    char command[64];

    sprintf (command, "box %d,%d %d,%d 0x%x\n", x1, y1, x2, y2, color); 
    write (video_FD, command, strlen(command));
}

void draw_line(int x0, int x1, int y0, int y1, short int color) {
    char command[64];
    sprintf (command, "line %d,%d %d,%d 0x%x\n", x0, y0, x1, y1, color); 
    write (video_FD, command, strlen(command));
}

void swap(int* x, int* y) {
    int temp = *x;
    *x = *y;
    *y = temp;
}

struct point generate_random_point() {
    struct point p;
    p.x = rand() % (screen_x - 2 * BOX_OFFSET) + BOX_OFFSET;
    p.y = rand() % (screen_y - 2 * BOX_OFFSET) + BOX_OFFSET;
    p.direction_x = rand() % 2 == 0 ? -1 : 1;
    p.direction_y = rand() % 2 == 0 ? -1 : 1;
    return p;
}