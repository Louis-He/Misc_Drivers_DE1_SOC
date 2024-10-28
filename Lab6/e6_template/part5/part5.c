#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

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
    double x;
    double y;

    int direction_x;
    int direction_y;
};

int screen_x, screen_y;
int video_FD; // file descriptor
char video_buffer[video_BYTES]; // buffer for video char data
int num_points = 3;

/**  your part 4 user code here  **/
volatile sig_atomic_t stop; // used to exit the program cleanly
void catchSIGINT(int);
void draw_figure(int);
void swap(int* x, int* y);
void plot_pixel(int, int, char);
void plot_box(int, int, int, int, short int);
void draw_line(int x0, int x1, int y0, int y1, short int color);
void clean_screen(void);
void init_point(int idx);
void init_points();
void update_points();
struct point generate_random_point();
double increment_distance = 1.0;
struct point points[100];

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
            // speed up
            increment_distance *= 1.1;
        } else if ((key & 0x2) != 0) {
            // slow down
            increment_distance *= 0.9;
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

void init_point(int idx) {
    points[idx] = generate_random_point();
}

void init_points() {
    int i = 0;
    for (i = 0; i < num_points; i++) {
        struct point p = generate_random_point();
        points[i] = p;
    }
}

void draw_figure(int is_draw_lines) {
    int i = 0;
    clean_screen();

    if (is_draw_lines) {
        for (i = 0; i < num_points - 1; i++) {
            struct point p1 = points[i];
            struct point p2 = points[(i + 1) % num_points];

            draw_line((int)(p1.x), (int)(p2.x), (int)(p1.y), (int)(p2.y), WHITE);
        }
    }

    for (i = 0; i < num_points; i++) {
        struct point p = points[i];
        plot_box((int)(p.x) - BOX_OFFSET, (int)(p.y) - BOX_OFFSET, (int)(p.x) + BOX_OFFSET, (int)(p.y) + BOX_OFFSET, WHITE);
    }

    char command[64];
    sprintf (command, "sync\n");
    write (video_FD, command, strlen(command));
}

void update_points() {
    int i = 0;
    for (i = 0; i < num_points; i++) {
        struct point p = points[i];
        p.x += p.direction_x * increment_distance;
        p.y += p.direction_y * increment_distance;
        
        if (p.x < BOX_OFFSET) {
            p.direction_x = 1;
        }

        if (p.x > screen_x - BOX_OFFSET) {
            p.direction_x = -1;
        }

        if (p.y < BOX_OFFSET) {
            p.direction_y = 1;
        }

        if (p.y > screen_y - BOX_OFFSET) {
            p.direction_y = -1;
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