#ifdef KERNEL
#include <linux/kernel.h>
#include <linux/fs.h>               // struct file, struct file_operations
#include <linux/init.h>             // for __init, see code
#include <linux/module.h>           // for module init and exit macros
#include <linux/miscdevice.h>       // for misc_device_register and struct miscdev
#include <linux/uaccess.h>          // for copy_to_user, see code
#include <asm/io.h>                 // for mmap
#endif

#include "include/ADXL345.h"
#include "include/address_map_arm.h"


/** The following is provided so that you can use this file in both user and kernel space **/

volatile unsigned int * SYSMGR_virtual;
volatile unsigned int * I2C0_virtual;

#ifdef KERNEL

void pass_addrs(unsigned int* SYSMGR_ptr_ext, unsigned int* I2C0_ptr_ext) {

    SYSMGR_virtual = SYSMGR_ptr_ext;
    I2C0_virtual = I2C0_ptr_ext;

}

#else
#include "physical.h"

int fd = -1;
int map_mem() {

    // open /dev/mem
    if ((fd = open_physical (fd)) == -1)
        return (-1);
    else if ((SYSMGR_virtual = map_physical (fd, SYSMGR_BASE, SYSMGR_SPAN)) == 0)
        return (-1);
    else if ((I2C0_virtual = map_physical (fd, I2C0_BASE, I2C0_SPAN)) == 0)
        return (-1);

    return 0;

}
void unmap_mem() {
    unmap_physical ((void*) SYSMGR_virtual, SYSMGR_SPAN);
    unmap_physical ((void*) I2C0_virtual, I2C0_SPAN);
    close_physical(fd);
}

#endif


/** This sample mg_per_lsb calculator is provided for your convenience **/
int16_t calc_mg_per_lsb(uint8_t resolution, uint8_t range) {

    if (resolution == XL345_FULL_RESOLUTION)
        return 4;

    switch(range) {
        case XL345_RANGE_2G:  return ROUNDED_DIVISION(2*1000, 512);
        case XL345_RANGE_4G:  return ROUNDED_DIVISION(4*1000, 512);
        case XL345_RANGE_8G:  return ROUNDED_DIVISION(8*1000, 512);
        case XL345_RANGE_16G: return ROUNDED_DIVISION(16*1000, 512);
        default: return 0;
   }
}


/** This sample offset calculator is provided for your convenience **/
void calc_offsets(int16_t mg_per_lsb, int avx, int avy, int avz,
                            int8_t* ofx, int8_t* ofy, int8_t* ofz) {

    int g_per_lsb, offset_scale;

    g_per_lsb = mg_per_lsb * 1000;

    if (g_per_lsb > 15600) {

        offset_scale = ROUNDED_DIVISION(g_per_lsb, 15600);
        *ofx = 0 - (avx * offset_scale);
        *ofy = 0 - (avy * offset_scale);
        *ofz = 0 - (avz - ROUNDED_DIVISION(1000,mg_per_lsb)) * offset_scale;

    } else {

        offset_scale = ROUNDED_DIVISION(15600, g_per_lsb);
        *ofx = 0 - ROUNDED_DIVISION(avx, offset_scale);
        *ofy = 0 - ROUNDED_DIVISION(avy, offset_scale);
        *ofz = 0 - ROUNDED_DIVISION(avz - ROUNDED_DIVISION(1000,mg_per_lsb), offset_scale);

    }
}



/**  Please. Implement the following ADXL345 functions; your implementations      **/
/**  will be similar to accel_tutorial/ADXL345.c, but will use virtual addresses  **/
/**  NOTE: not all functions will be used in every part of the lab exercise       **/

// Set up pin muxing (in sysmgr) to connect ADXL345 wires to I2C0
void Pinmux_Config() {
    *(SYSMGR_virtual + SYSMGR_I2C0USEFPGA) = 0; // Set SYSMGR_I2C0USEFPGA
    *(SYSMGR_virtual + SYSMGR_GENERALIO7) = 1; // Set SYSMGR_GENERALIO7
    *(SYSMGR_virtual + SYSMGR_GENERALIO8) = 1; // Set SYSMGR_GENERALIO8
}

// Initialize the I2C0 controller for use with the ADXL345 chip
void I2C0_Init() {
    *(I2C0_virtual + I2C0_ENABLE) = 2; // Disable I2C0_ENABLE first

    while ((*(I2C0_virtual + I2C0_ENABLE_STATUS) & 0x1) == 1); // Wait for I2C0_ENABLE_STATUS until its disabled

    *(I2C0_virtual + I2C0_CON) = 0x65; // Set I2C0_CON
    *(I2C0_virtual + I2C0_TAR) = 0x53; // Set I2C0_TAR
    *(I2C0_virtual + I2C0_FS_SCL_HCNT) = 60 + 30; // Set I2C0_FS_SCL_HCNT
    *(I2C0_virtual + I2C0_FS_SCL_LCNT) = 130 + 30; // Set I2C0_FS_SCL_LCNT

    *(I2C0_virtual + I2C0_ENABLE) = 1; // Enable I2C0_ENABLE

    while ((*(I2C0_virtual + I2C0_ENABLE_STATUS) & 0x1) == 0); // Wait for I2C0_ENABLE_STATUS until its enabled
}

// Write value to internal register at address
void ADXL345_REG_WRITE(uint8_t address, uint8_t value) {
    *(I2C0_virtual + I2C0_DATA_CMD) = address + 0x400; // Send reg address to I2C0_DATA_CMD to send START signal

    *(I2C0_virtual + I2C0_DATA_CMD) = value; // Send value to I2C0_DATA_CMD
}

// Read value from internal register at address
void ADXL345_REG_READ(uint8_t address, uint8_t *value) {
    *(I2C0_virtual + I2C0_DATA_CMD) = address + 0x400; // Send reg address to I2C0_DATA_CMD to send START signal

    *(I2C0_virtual + I2C0_DATA_CMD) = 0x100; // Send read signal to I2C0_DATA_CMD

    while (*(I2C0_virtual + I2C0_RXFLR) == 0); // Wait for I2C0_RXFLR until its not empty

    *value = *(I2C0_virtual + I2C0_DATA_CMD); // Read value from I2C0_DATA_CMD
}

// Read multiple consecutive internal registers
void ADXL345_REG_MULTI_READ(uint8_t address, uint8_t values[], uint8_t len) {
    *(I2C0_virtual + I2C0_DATA_CMD) = address + 0x400; // Send reg address to I2C0_DATA_CMD to send START signal

    int i;
    for (i = 0; i < len; i++) {
        *(I2C0_virtual + I2C0_DATA_CMD) = 0x100; // Send read signal to I2C0_DATA_CMD
    }

    int nth_byte = 0;
    while (len) {
        if (*(I2C0_virtual + I2C0_RXFLR) > 0) {
            values[nth_byte] = *(I2C0_virtual + I2C0_DATA_CMD); // Read value from I2C0_DATA_CMD
            nth_byte++;
            len--;
        }
    }
}

// Initialize the ADXL345 chip
void ADXL345_Init() {
    ADXL345_REG_WRITE(ADXL345_REG_DATA_FORMAT, XL345_RANGE_16G | XL345_10BIT); // Set ADXL345_REG_DATA_FORMAT

    ADXL345_REG_WRITE(ADXL345_REG_BW_RATE, XL345_RATE_12_5); // Set ADXL345_REG_BW_RATE

    ADXL345_REG_WRITE(ADXL345_REG_THRESH_ACT, 0x04); // Set ADXL345_REG_THRESH_ACT
    ADXL345_REG_WRITE(ADXL345_REG_THRESH_INACT, 0x02); // Set ADXL345_REG_THRESH_INACT

    ADXL345_REG_WRITE(ADXL345_REG_TIME_INACT, 0x02); // Set ADXL345_REG_TIME_INACT
    ADXL345_REG_WRITE(ADXL345_REG_ACT_INACT_CTL, 0xFF); // Set ADXL345_REG_ACT_INACT_CTL
    ADXL345_REG_WRITE(ADXL345_REG_INT_ENABLE, XL345_ACTIVITY | XL345_INACTIVITY );

    // stop measure
    ADXL345_REG_WRITE(ADXL345_REG_POWER_CTL, XL345_STANDBY);

    // start measure
    ADXL345_REG_WRITE(ADXL345_REG_POWER_CTL, XL345_MEASURE);
}

// Calibrate the ADXL345. The DE1-SoC should be placed on a flat
// surface, and must remain stationary for the duration of the calibration.
void ADXL345_Calibrate() {
    int average_x = 0;
    int average_y = 0;
    int average_z = 0;
    int16_t XYZ[3];
    int8_t offset_x;
    int8_t offset_y;
    int8_t offset_z;

    // stop measure
    ADXL345_REG_WRITE(ADXL345_REG_POWER_CTL, XL345_STANDBY);
    
    // Get current offsets
    ADXL345_REG_READ(ADXL345_REG_OFSX, (uint8_t *)&offset_x);
    ADXL345_REG_READ(ADXL345_REG_OFSY, (uint8_t *)&offset_y);
    ADXL345_REG_READ(ADXL345_REG_OFSZ, (uint8_t *)&offset_z);

    // Use 100 hz rate for calibration. Save the current rate.
    uint8_t saved_bw;
    ADXL345_REG_READ(ADXL345_REG_BW_RATE, &saved_bw);
    ADXL345_REG_WRITE(ADXL345_REG_BW_RATE, XL345_RATE_100);
    
    // Use 16g range, full resolution. Save the current format.
    uint8_t saved_dataformat;
    ADXL345_REG_READ(ADXL345_REG_DATA_FORMAT, &saved_dataformat);
    ADXL345_REG_WRITE(ADXL345_REG_DATA_FORMAT, XL345_RANGE_16G | XL345_FULL_RESOLUTION);

    // start measure
    ADXL345_REG_WRITE(ADXL345_REG_POWER_CTL, XL345_MEASURE);
    
    // Get the average x,y,z accelerations over 32 samples (LSB 3.9 mg)
    int i = 0;
    while (i < 32){
		// Note: use DATA_READY here, can't use ACTIVITY because board is stationary.
        if (ADXL345_IsDataReady()){
            ADXL345_XYZ_Read(XYZ);
            average_x += XYZ[0];
            average_y += XYZ[1];
            average_z += XYZ[2];
            i++;
        }
    }

    average_x = ROUNDED_DIVISION(average_x, 32);
    average_y = ROUNDED_DIVISION(average_y, 32);
    average_z = ROUNDED_DIVISION(average_z, 32);
    
    // stop measure
    ADXL345_REG_WRITE(ADXL345_REG_POWER_CTL, XL345_STANDBY);
    
    // printf("Average X=%d, Y=%d, Z=%d\n", average_x, average_y, average_z);
    
    // Calculate the offsets (LSB 15.6 mg)
    offset_x += ROUNDED_DIVISION(0-average_x, 4);
    offset_y += ROUNDED_DIVISION(0-average_y, 4);
    offset_z += ROUNDED_DIVISION(256-average_z, 4);
    
    // printf("Calibration: offset_x: %d, offset_y: %d, offset_z: %d (LSB: 15.6 mg)\n",offset_x,offset_y,offset_z);
    
    // Set the offset registers
    ADXL345_REG_WRITE(ADXL345_REG_OFSX, offset_x);
    ADXL345_REG_WRITE(ADXL345_REG_OFSY, offset_y);
    ADXL345_REG_WRITE(ADXL345_REG_OFSZ, offset_z);
    
    // Restore original bw rate
    ADXL345_REG_WRITE(ADXL345_REG_BW_RATE, saved_bw);
    
    // Restore original data format
    ADXL345_REG_WRITE(ADXL345_REG_DATA_FORMAT, saved_dataformat);
    
    // start measure
    ADXL345_REG_WRITE(ADXL345_REG_POWER_CTL, XL345_MEASURE);
}

// Read acceleration data of all three axes
void ADXL345_XYZ_Read(int16_t szData16[3]) {
    uint8_t szData8[6];
    ADXL345_REG_MULTI_READ(0x32, (uint8_t *)&szData8, sizeof(szData8));

    szData16[0] = (szData8[1] << 8) | szData8[0];
    szData16[1] = (szData8[3] << 8) | szData8[2];
    szData16[2] = (szData8[5] << 8) | szData8[4];
}

// Read the ID register
void ADXL345_IdRead(uint8_t *pId) {
    ADXL345_REG_READ(ADXL345_REG_DEVID, pId);
}

// Return if there is any new data that hasn't been read yet
bool ADXL345_IsDataReady(){
    bool bReady = false;
    uint8_t data8;
    
    ADXL345_REG_READ(ADXL345_REG_INT_SOURCE, &data8);
    if (data8 & XL345_DATAREADY)
        bReady = true;
    
    return bReady;
}