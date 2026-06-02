#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <stdio.h>
#include <stdlib.h>

#define PCF8591_ADDR    0x48

static int fd;

int temp_init()
{
    fd = wiringPiI2CSetup(PCF8591_ADDR);
    if(fd == -1)
    {
        printf("PCF8591 연결 실패\n");
        return -1;
    }
    return 0;
}

int temp_read()
{
    int a2dChannel = 1;
    wiringPiI2CWrite(fd, 0x00 | a2dChannel);
    wiringPiI2CRead(fd);
    return wiringPiI2CRead(fd);
}