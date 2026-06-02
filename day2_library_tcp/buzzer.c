#include <wiringPi.h>
#include <softTone.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SPKR    3
#define TOTAL   32

int notes[] = {
    391, 391, 440, 440, 391, 391, 329, 329,
    391, 391, 329, 329, 293, 293, 293, 0,
    391, 391, 440, 440, 391, 391, 329, 329,
    391, 329, 293, 329, 261, 261, 261, 0
};

void buzzer_init()
{
    pinMode(SPKR, OUTPUT);
}

void buzzer_on()
{
    softToneCreate(SPKR);
    for(int i = 0; i < TOTAL; i++)
    {
        softToneWrite(SPKR, notes[i]);
        delay(280);
    }
}

void buzzer_off()
{
    softToneWrite(SPKR, 0);
}