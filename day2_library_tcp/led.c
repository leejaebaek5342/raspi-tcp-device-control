// led.c
#include <wiringPi.h>
#include <softPwm.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define BRIGHTNESS_HIGH   255
#define BRIGHTNESS_MID    130
#define BRIGHTNESS_LOW    50

#define LED   1

void led_init()
{
    pinMode(LED, OUTPUT);
    softPwmCreate(LED, 0, 255);
    delay(100);
}

void led_on_off(char* arg)
{
    if(strcmp(arg, "ON") == 0)
        softPwmWrite(LED, BRIGHTNESS_HIGH);
    else if(strcmp(arg, "OFF") == 0)
        softPwmWrite(LED, 0);
}

void led_brightness(char* arg)
{
    if(strcmp(arg, "HIGH") == 0)
        softPwmWrite(LED, BRIGHTNESS_HIGH);
    else if(strcmp(arg, "MID") == 0)
        softPwmWrite(LED, BRIGHTNESS_MID);
    else
        softPwmWrite(LED, BRIGHTNESS_LOW);
}