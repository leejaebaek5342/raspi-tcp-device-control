#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <wiringPi.h>

int main()
{
    // 라이브러리 로딩
    void* led_lib   = dlopen("./libled.so", RTLD_LAZY);
    void* buz_lib   = dlopen("./libbuzzer.so", RTLD_LAZY);
    void* cds_lib   = dlopen("./libcds.so", RTLD_LAZY);
    void* seg_lib   = dlopen("./libseg.so", RTLD_LAZY);
    void* temp_lib  = dlopen("./libtemp.so", RTLD_LAZY);

    // 함수 포인터
    void (*led_init)()          = dlsym(led_lib, "led_init");
    void (*led_on_off)(char*)   = dlsym(led_lib, "led_on_off");
    void (*led_brightness)(char*) = dlsym(led_lib, "led_brightness");
    void (*buzzer_init)()       = dlsym(buz_lib, "buzzer_init");
    void (*buzzer_on)()         = dlsym(buz_lib, "buzzer_on");
    void (*buzzer_off)()        = dlsym(buz_lib, "buzzer_off");
    int  (*cds_init)()          = dlsym(cds_lib, "cds_init");
    int  (*cds_read)()          = dlsym(cds_lib, "cds_read");
    void (*seg_init)()          = dlsym(seg_lib, "seg_init");
    void (*seg_display)(int)    = dlsym(seg_lib, "seg_display");
    int  (*temp_init)()         = dlsym(temp_lib, "temp_init");
    int  (*temp_read)()         = dlsym(temp_lib, "temp_read");

    // 초기화
    wiringPiSetup();
    led_init();
    buzzer_init();
    seg_init();
    cds_init();
    temp_init();

    // LED 테스트
    printf("LED ON\n");
    led_on_off("ON");
    delay(2000);
    printf("LED MID\n");
    led_brightness("MID");
    delay(2000);
    printf("LED OFF\n");
    led_on_off("OFF");
    delay(1000);

    // 부저 테스트
    printf("BUZZER ON\n");
    buzzer_on();
    printf("BUZZER OFF\n");
    buzzer_off();
    delay(1000);

    // 조도센서 테스트
    for(int i = 0; i < 5; i++)
    {
        printf("조도센서: %d\n", cds_read());
        delay(1000);
    }

    // 온도센서 테스트
    for(int i = 0; i < 5; i++)
    {
        printf("온도: %.1f\n", (float)(temp_read() / 10.17));
        delay(1000);
    }

    // 7세그먼트 테스트
    for(int i = 0; i <= 9; i++)
    {
        printf("세그먼트: %d\n", i);
        seg_display(i);
        delay(1000);
    }

    // 라이브러리 해제
    dlclose(led_lib);
    dlclose(buz_lib);
    dlclose(cds_lib);
    dlclose(seg_lib);
    dlclose(temp_lib);

    return 0;
}