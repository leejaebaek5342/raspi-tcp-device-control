#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <dlfcn.h>   
#include <wiringPi.h>                       

#define MAX_SIZES   256
#define EVENT_SIZES 20
#define FD_PORTS    60000
#define BACKLOG     10
#define TEMP_ADC_SCALE 10.17f              

void* load_lib(const char* path, const char** func_names, void** func_ptrs, int count)
{
    void* lib = dlopen(path, RTLD_LAZY);
    if(!lib)
    {
        printf("dlopen 실패: %s\n", dlerror());
        return NULL;
    }
    for(int i = 0; i < count; i++)
    {    
        dlerror();
        func_ptrs[i] = dlsym(lib, func_names[i]);

        char* error = dlerror();
        if(error != NULL)
        {
            printf("dlsym 실패: %s - %s\n", func_names[i], error);
            dlclose(lib);
            return NULL;
        }
    }
    return lib;
}

void* led_thread(void* arg)
{
    void* led_ptrs[3];
    const char* led_funcs[] = {"led_init","led_on_off", "led_brightness"};  
    void* led_lib;
    
    led_lib = load_lib("./libled.so",led_funcs,led_ptrs,3);
    void (*led_init)()            = led_ptrs[0];
    void (*led_on_off)(char*)     = led_ptrs[1];
    void (*led_brightness)(char*) = led_ptrs[2];

    led_init();

    if(strcmp((char*)arg,"ON") == 0 || strcmp((char*)arg,"OFF") == 0)
        led_on_off(arg);    
    else
        led_brightness(arg);

    free(arg);
    dlclose(led_lib);
    return NULL;
}

void* buzzer_thread(void* arg)
{
    void* buz_ptrs[3];
    const char* buz_funcs[] = {"buzzer_init","buzzer_on","buzzer_off"};  
    void* buz_lib;

    buz_lib = load_lib("./libbuzzer.so",buz_funcs,buz_ptrs,3);
    void (*buzzer_init)() = buz_ptrs[0];  
    void (*buzzer_on)()   = buz_ptrs[1];  
    void (*buzzer_off)()  = buz_ptrs[2];  

    buzzer_init();

    if(strcmp((char*)arg,"ON") == 0)
        buzzer_on();
    else
        buzzer_off();

    free(arg);
    dlclose(buz_lib);
    return NULL;
}

void* cds_thread(void* arg)
{
    void* cds_ptrs[2];
    const char* cds_funcs[] = {"cds_init","cds_read"}; 
    void* cds_lib;

    cds_lib = load_lib("./libcds.so",cds_funcs,cds_ptrs,2);
    int (*cds_init)() = cds_ptrs[0];
    int (*cds_read)() = cds_ptrs[1];

    cds_init();

    int val = cds_read();
    int fd = *(int*)arg;
    char result[MAX_SIZES];
    snprintf(result, MAX_SIZES, "조도센서: %d\n", val);
    send(fd, result, strlen(result), 0);

    free(arg);
    dlclose(cds_lib);
    return NULL;  
}

void* seg_thread(void* arg)
{
    void* seg_ptrs[2];
    const char* seg_funcs[] = {"seg_init", "seg_display"};
    void* seg_lib = load_lib("./libseg.so", seg_funcs, seg_ptrs, 2);

    if(!seg_lib)
    {
        free(arg);
        return NULL;
    }
    void (*seg_init)()       = seg_ptrs[0];
    void (*seg_display)(int) = seg_ptrs[1];

    seg_init();

    int num = atoi((char*)arg);
    if(num < 0 || num > 9)
    {
        free(arg);
        dlclose(seg_lib);
        return NULL;
    }
    for(int i = num; i >= 0; i--)
    {
        seg_display(i);
        delay(1000);
    }

    pthread_t thr;
    char* cmd = strdup("ON");
    pthread_create(&thr, NULL, buzzer_thread, cmd);
    pthread_detach(thr);

    free(arg);
    dlclose(seg_lib);
    return NULL;
}

void* temp_thread(void* arg)
{
    void* temp_ptrs[2];
    const char* temp_funcs[] = {"temp_init", "temp_read"};
    void* temp_lib = load_lib("./libtemp.so", temp_funcs, temp_ptrs, 2);

    if(!temp_lib)
    {
        free(arg);
        return NULL;
    }
    int (*temp_init)() = temp_ptrs[0];
    int (*temp_read)() = temp_ptrs[1];

    temp_init();

    int val = temp_read();
    float temp = val / TEMP_ADC_SCALE;
    int fd = *(int*)arg;
    char result[MAX_SIZES];
    snprintf(result, MAX_SIZES, "온도: %.1f°C\n", temp);
    send(fd, result, strlen(result), 0);

    free(arg);
    dlclose(temp_lib);
    return NULL;
}

int main(void)
{
    int sockfd,efd;
    pthread_t thr;
    int client_sd;
    int n;
    int opt = 1;
    int socklen;
    int numbytes;
    char dev[MAX_SIZES];
    char cmd[MAX_SIZES];
    char buf[MAX_SIZES];
    struct epoll_event ev,*events_list;
    struct sockaddr_in serveraddr, clientaddr;

    wiringPiSetup();

    if((efd = epoll_create(100)) < 0)
    {
        perror("epoll_create");
        exit(1);
    }

    events_list = malloc(sizeof(*events_list)*EVENT_SIZES);  

    socklen = sizeof(struct sockaddr_in);

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0))==-1)
    {
        perror("socket error :");
        close(sockfd);
        return 1;
    }
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        perror("setsockopt");
        close(sockfd); close(efd); free(events_list); exit(1);
    }
    
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(FD_PORTS);
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if(bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) == -1)
    {
        close(sockfd);
        return 1;
    }

    if(listen(sockfd,BACKLOG) == -1)
    {
        perror("listen");
        exit(1);
    }
    ev.events = EPOLLIN;
    ev.data.fd = sockfd;
    epoll_ctl(efd,EPOLL_CTL_ADD,sockfd,&ev);

    while(1)
    {
        if((n = epoll_wait(efd,events_list,EVENT_SIZES,-1)) == -1)
        {
            perror("epoll_wait");
            exit(1);
        }
        for(int i = 0; i < n; i++)
        {
            if(events_list[i].data.fd == sockfd)
            {
                printf("Accept\n");
                client_sd = accept(sockfd, (struct sockaddr*)&clientaddr, (socklen_t*)&socklen);
                printf("get connected from %s, fd = %d\n", inet_ntoa(clientaddr.sin_addr), events_list[i].data.fd);  

                ev.events = EPOLLIN;
                ev.data.fd = client_sd;
                epoll_ctl(efd,EPOLL_CTL_ADD,client_sd,&ev);
            }
            else
            {
                int sender_fd = events_list[i].data.fd;
                numbytes = read(sender_fd,buf,MAX_SIZES-1);
                if(numbytes <= 0)
                {
                    epoll_ctl(efd,EPOLL_CTL_DEL,sender_fd,NULL);
                    printf("연결 종료\n");
                    close(sender_fd);
                    close(sockfd);
                    close(efd);
                    free(events_list);
                    exit(0);
                }

                buf[numbytes] = '\0';
                sscanf(buf,"%s %s", dev, cmd);

                if(strcmp(dev,"LED") == 0)
                {
                    char* arg = strdup(cmd);
                    if(pthread_create(&thr, NULL, led_thread, arg) != 0)
                    {
                        free(arg);
                        continue;  
                    }
                    pthread_detach(thr);
                }
                else if(strcmp(dev,"BUZZER") == 0)  
                {
                    char* arg = strdup(cmd);
                    if(pthread_create(&thr, NULL, buzzer_thread, arg) != 0)
                    {
                        free(arg);
                        continue;  
                    }
                    pthread_detach(thr);
                }
                else if(strcmp(dev,"SEG") == 0)  
                {
                    char* arg = strdup(cmd);
                    if(pthread_create(&thr, NULL, seg_thread, arg) != 0)
                    {
                        free(arg);
                        continue;  
                    }
                    pthread_detach(thr);
                }
                else if(strcmp(dev, "CDS") == 0)  
                {
                    int* p = malloc(sizeof(int));
                    *p = sender_fd;  
                    if(pthread_create(&thr, NULL, cds_thread, p) != 0)
                    {
                        free(p);
                        continue;  
                    }
                    pthread_detach(thr);
                }
                else if(strcmp(dev, "TEMP") == 0)  
                {
                    int* p = malloc(sizeof(int));
                    *p = sender_fd;  
                    if(pthread_create(&thr, NULL, temp_thread, p) != 0)
                    {
                        free(p);
                        continue;  
                    }
                    pthread_detach(thr);
                }
                else
                {
                    printf("알 수 없는 명령: %s\n", buf);
                }    
            }
        }
    }
    return 0;
}