#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include "physical.h"
#include "address_map_arm.h"
#include <stdbool.h>

void* lw_bridge_virtual;
static bool run = true;
/**  your part 1 user code here  **/
int main() {
    int adc_fd = -1;
    if ((adc_fd = open_physical (adc_fd)) == -1)
        return (-1);
    else if ((lw_bridge_virtual = map_physical (adc_fd, LW_BRIDGE_BASE, LW_BRIDGE_SPAN)) == 0)
        return (-1);

    unsigned int * adc_ptr = (unsigned int *)(lw_bridge_virtual + ADC_BASE);
    // enable auto update
    *(adc_ptr + 1) = 0x1;

    // read the ADC from channel 0
    while (run) {
        unsigned int adc_val = (*(adc_ptr + 0)) & 0xFFF;

        printf("ADC value of channel 0: %d\n", adc_val);
        usleep(1000000);
    }

    // disable auto update
    *(adc_ptr + 1) = 0x0;
    unmap_physical ((void*) lw_bridge_virtual, LW_BRIDGE_SPAN);
    close_physical(adc_fd);
    return 0;
}