#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>

#define A   4   // BCM 23
#define B   5   // BCM 24
#define C   6   // BCM 25
#define D   26  // BCM 12

void seg_display(int num)
{
    digitalWrite(A, (num >> 0) & 1);
    digitalWrite(B, (num >> 1) & 1);
    digitalWrite(C, (num >> 2) & 1);
    digitalWrite(D, (num >> 3) & 1);
}

int main(int argc, char** argv)
{
    wiringPiSetup();
    pinMode(A, OUTPUT);
    pinMode(B, OUTPUT);
    pinMode(C, OUTPUT);
    pinMode(D, OUTPUT);

    if(argc != 2)
    {
        printf("USAGE: ./segment [0-9]\n");
        exit(1);
    }

    int num = atoi(argv[1]);
    if(num < 0 || num > 9)
    {
        printf("0~9 사이 숫자만 입력하세요\n");
        exit(1);
    }

    seg_display(num);
    delay(3000);

    return 0;
}