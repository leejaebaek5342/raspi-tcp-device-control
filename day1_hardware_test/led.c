#include <wiringPi.h>    // GPIO 제어
#include <softPwm.h>     // 밝기 조절 (PWM)
#include <stdio.h>      
#include <string.h>
#include <stdlib.h> 

#define BRIGHTNESS_HIGH   255
#define BRIGHTNESS_MID    130
#define BRIGHTNESS_LOW    50

#define LED   1

void led_on_off(char* arg)
{
    if(strcmp(arg, "ON") == 0)
    {
	    softPwmWrite(LED, BRIGHTNESS_HIGH);
    }
    else if(strcmp(arg, "OFF") == 0)
    {
        softPwmWrite(LED, 0);
    }
}

void led_brightness(char* arg)
{
     if(strcmp(arg, "HIGH") == 0)
    {
        softPwmWrite(LED, BRIGHTNESS_HIGH);
    }
    else if(strcmp(arg, "MID") == 0)
    {
        softPwmWrite(LED, BRIGHTNESS_MID);
    }
    else
    {
        softPwmWrite(LED, BRIGHTNESS_LOW);
    }
}

int main(int argc,char** argv)
{
    wiringPiSetup();
    pinMode(LED, OUTPUT); 
    softPwmCreate(LED,0,255);
  
    if(argc != 2)
    {
        printf("USAGE: ./led [ON/OFF/HIGH/MID/LOW]\n");
        exit(1);
    }

    if(strcmp(argv[1], "ON") == 0 || strcmp(argv[1], "OFF") == 0)
    {
        led_on_off(argv[1]);
    }
    else if(strcmp(argv[1], "HIGH") == 0 || strcmp(argv[1], "MID") == 0 || strcmp(argv[1], "LOW") == 0)
    {
        led_brightness(argv[1]);
    }
    else
    {
        printf("잘못된 입력입니다: %s\n", argv[1]);
        exit(1);
    }
    
    delay(3000);

    return 0;
}