#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include "ADXL345.h"
#include "physical.h"
#include "address_map_arm.h"
#include <signal.h>


int main(void){

    if (map_mem() < 0)
        return 1;

    /**  Your part 1 user code here  **/
    Pinmux_Config();
    I2C0_Init();

    uint8_t id;
    int16_t mg_per_lsb = 31; 
    ADXL345_IdRead(&id);

    printf("ADXL345 ID: %x\n", id);

    if (id == 0xE5) {
        printf("ADXL345 detected\n");

        ADXL345_Init();

        int16_t szData16[3];

        while (1) {
            ADXL345_XYZ_Read(szData16);
            printf("%d, %d, %d\n", szData16[0]*mg_per_lsb, szData16[1]*mg_per_lsb, szData16[2]*mg_per_lsb);
            usleep(1000000);
        }


    } else {
        printf("ADXL345 not detected\n");
    }

    unmap_mem();

    return 0;
}
