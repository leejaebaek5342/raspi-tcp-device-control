#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>

#define SEG_A   4
#define SEG_B   5
#define SEG_C   6
#define SEG_D   26

void seg_init()
{
    pinMode(SEG_A, OUTPUT);
    pinMode(SEG_B, OUTPUT);
    pinMode(SEG_C, OUTPUT);
    pinMode(SEG_D, OUTPUT);
}

void seg_display(int num)
{
    digitalWrite(SEG_A, (num >> 0) & 1);
    digitalWrite(SEG_B, (num >> 1) & 1);
    digitalWrite(SEG_C, (num >> 2) & 1);
    digitalWrite(SEG_D, (num >> 3) & 1);
}