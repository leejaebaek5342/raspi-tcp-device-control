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
#include <signal.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <syslog.h>

pthread_mutex_t buz_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t seg_mutex = PTHREAD_MUTEX_INITIALIZER;

volatile int buzzer_stop = 0;

typedef struct {
    int efd;
    int sockfd;
    struct epoll_event *events_list;
} server_t;

#define SERVER_WORKDIR "/home/jambaek/fortest"
#define MAX_SIZES       256
#define EVENT_SIZES     20
#define SERVER_PORT        60000
#define BACKLOG         10
#define TEMP_ADC_SCALE  10.17f
#define CDS_THRESHOLD 205

void daemonize(void)
{
    struct sigaction sa;
    struct rlimit rl;
    pid_t pid;
    int fd0, fd1, fd2;
    int i;

    umask(0);

    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        perror("getrlimit");
        exit(1);
    }

    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }
    else if (pid != 0) {
        exit(0);
    }

    if (setsid() == -1) {
        perror("setsid");
        exit(1);
    }

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGHUP, &sa, NULL) < 0) {
        perror("sigaction");
        exit(1);
    }

    if (chdir(SERVER_WORKDIR) < 0) {
        perror("chdir");
        exit(1);
    }

    if (rl.rlim_max == RLIM_INFINITY) {
        rl.rlim_max = 1024;
    }

    for (i = 0; i < (int)rl.rlim_max; i++) {
        close(i);
    }

    fd0 = open("/dev/null", O_RDWR);
    fd1 = dup(0);
    fd2 = dup(0);

    openlog("device_server", LOG_CONS | LOG_PID, LOG_DAEMON);

    if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
        syslog(LOG_ERR, "unexpected file descriptors %d %d %d", fd0, fd1, fd2);
        exit(1);
    }

    syslog(LOG_INFO, "device server daemon started");
}

void* load_lib(const char* path, const char** func_names, void** func_ptrs, int count)
{
    void* lib = dlopen(path, RTLD_LAZY);
    if (!lib) {
        syslog(LOG_ERR, "dlopen failed: %s", dlerror());
        return NULL;
    }

    for (int i = 0; i < count; i++) {
        dlerror();
        func_ptrs[i] = dlsym(lib, func_names[i]);

        char* error = dlerror();
        if (error != NULL) {
            syslog(LOG_ERR, "dlsym failed: %s - %s", func_names[i], error);
            dlclose(lib);
            return NULL;
        }
    }

    return lib;
}

void* led_thread(void* arg)
{
    void* led_ptrs[3];
    const char* led_funcs[] = {"led_init", "led_on_off", "led_brightness"};

    void* led_lib = load_lib("./libled.so", led_funcs, led_ptrs, 3);
    if (!led_lib) {
        free(arg);
        return NULL;
    }

    void (*led_init)(void)          = led_ptrs[0];
    void (*led_on_off)(char*)       = led_ptrs[1];
    void (*led_brightness)(char*)   = led_ptrs[2];

    char *cmd = (char *)arg;

    led_init();

    if (strcmp(cmd, "ON") == 0 || strcmp(cmd, "OFF") == 0)
        led_on_off(cmd);
    else
        led_brightness(cmd);

    free(arg);
    dlclose(led_lib);
    return NULL;
}

void* buzzer_thread(void* arg)
{
    void* buz_ptrs[3];
    const char* buz_funcs[] = {"buzzer_init", "buzzer_on", "buzzer_off"};

    void* buz_lib = load_lib("./libbuzzer.so", buz_funcs, buz_ptrs, 3);
    if (!buz_lib) {
        free(arg);
        return NULL;
    }

    void (*buzzer_init)(void) = buz_ptrs[0];
    void (*buzzer_on)(void)   = buz_ptrs[1];
    void (*buzzer_off)(void)  = buz_ptrs[2];

    char *cmd = (char *)arg;

    buzzer_init();

    if (strcmp(cmd, "OFF") == 0) {
        buzzer_stop = 1;
        buzzer_off();

        free(arg);
        dlclose(buz_lib);
        return NULL;
    }

    if (pthread_mutex_trylock(&buz_mutex) != 0) {
        free(arg);
        dlclose(buz_lib);
        return NULL;
    }

    if (strcmp(cmd, "ON") == 0) {
        buzzer_stop = 0;
        buzzer_on();
    }

    pthread_mutex_unlock(&buz_mutex);

    free(arg);
    dlclose(buz_lib);
    return NULL;
}

void* cds_thread(void* arg)
{
    void* cds_ptrs[2];
    const char* cds_funcs[] = {"cds_init", "cds_read"};

    void* cds_lib = load_lib("./libcds.so", cds_funcs, cds_ptrs, 2);
    if (!cds_lib) {
        free(arg);
        return NULL;
    }

    int (*cds_init)(void) = cds_ptrs[0];
    int (*cds_read)(void) = cds_ptrs[1];

    cds_init();

    int val = cds_read();
    int fd = *(int*)arg;

    char result[MAX_SIZES];

    pthread_t thr;
    char* cmd;

    if (val >= CDS_THRESHOLD){
        cmd = strdup("ON");
        snprintf(result, MAX_SIZES, "조도센서: %d\n현재 빛이 감지되지 않고 있습니다.\n", val);
        send(fd, result, strlen(result), 0);
    }
    else{
        cmd = strdup("OFF");
        snprintf(result, MAX_SIZES, "조도센서: %d\n현재 빛이 감지되고 있습니다.", val);
        send(fd, result, strlen(result), 0);
    }

    if (cmd == NULL) {
        syslog(LOG_ERR, "strdup failed in cds_thread");
        free(arg);
        dlclose(cds_lib);
        return NULL;
    }

    if (pthread_create(&thr, NULL, led_thread, cmd) != 0) {
        syslog(LOG_ERR, "pthread_create led_thread failed");
        free(cmd);
    } else {
        pthread_detach(thr);
    }

    free(arg);
    dlclose(cds_lib);
    return NULL;
}

void* seg_thread(void* arg)
{
    if (pthread_mutex_trylock(&seg_mutex) != 0) {
        free(arg);
        return NULL;
    }

    void* seg_ptrs[2];
    const char* seg_funcs[] = {"seg_init", "seg_display"};

    void* seg_lib = load_lib("./libseg.so", seg_funcs, seg_ptrs, 2);
    if (!seg_lib) {
        free(arg);
        pthread_mutex_unlock(&seg_mutex);
        return NULL;
    }

    void (*seg_init)(void)       = seg_ptrs[0];
    void (*seg_display)(int)     = seg_ptrs[1];

    seg_init();

    int num = atoi((char*)arg);
    if (num < 0 || num > 9) {
        free(arg);
        dlclose(seg_lib);
        pthread_mutex_unlock(&seg_mutex);
        return NULL;
    }

    for (int i = num; i >= 0; i--) {
        seg_display(i);
        delay(1000);
    }

    pthread_t thr;
    char* cmd = strdup("ON");

    if (cmd == NULL) {
        syslog(LOG_ERR, "strdup failed in seg_thread");
    } else {
        if (pthread_create(&thr, NULL, buzzer_thread, cmd) != 0) {
            syslog(LOG_ERR, "pthread_create buzzer_thread failed");
            free(cmd);
        } else {
            pthread_detach(thr);
        }
    }

    pthread_mutex_unlock(&seg_mutex);

    free(arg);
    dlclose(seg_lib);
    return NULL;
}

void* temp_thread(void* arg)
{
    void* temp_ptrs[2];
    const char* temp_funcs[] = {"temp_init", "temp_read"};

    void* temp_lib = load_lib("./libtemp.so", temp_funcs, temp_ptrs, 2);
    if (!temp_lib) {
        free(arg);
        return NULL;
    }

    int (*temp_init)(void) = temp_ptrs[0];
    int (*temp_read)(void) = temp_ptrs[1];

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

void server_init(server_t* server)
{
    int opt = 1;
    struct epoll_event ev;
    struct sockaddr_in serveraddr;

    server->efd = -1;
    server->sockfd = -1;
    server->events_list = NULL;

    if (wiringPiSetup() == -1) {
        syslog(LOG_ERR, "wiringPiSetup failed");
        exit(1);
    }

    if ((server->efd = epoll_create(100)) < 0) {
        syslog(LOG_ERR, "epoll_create failed");
        exit(1);
    }

    server->events_list = malloc(sizeof(struct epoll_event) * EVENT_SIZES);
    if (server->events_list == NULL) {
        syslog(LOG_ERR, "malloc failed for events_list");
        close(server->efd);
        exit(1);
    }

    if ((server->sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        syslog(LOG_ERR, "socket failed");
        close(server->efd);
        free(server->events_list);
        exit(1);
    }

    if (setsockopt(server->sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        syslog(LOG_ERR, "setsockopt failed");
        close(server->sockfd);
        close(server->efd);
        free(server->events_list);
        exit(1);
    }

    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(SERVER_PORT);
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server->sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) == -1) {
        syslog(LOG_ERR, "bind failed");
        close(server->sockfd);
        close(server->efd);
        free(server->events_list);
        exit(1);
    }

    if (listen(server->sockfd, BACKLOG) == -1) {
        syslog(LOG_ERR, "listen failed");
        close(server->sockfd);
        close(server->efd);
        free(server->events_list);
        exit(1);
    }

    ev.events = EPOLLIN;
    ev.data.fd = server->sockfd;

    if (epoll_ctl(server->efd, EPOLL_CTL_ADD, server->sockfd, &ev) == -1) {
        syslog(LOG_ERR, "epoll_ctl server socket failed");
        close(server->sockfd);
        close(server->efd);
        free(server->events_list);
        exit(1);
    }

    syslog(LOG_INFO, "server initialized: port=%d", SERVER_PORT);
}

void run_server(server_t* server)
{
    int n;
    int numbytes;
    int client_sd;
    int socklen = sizeof(struct sockaddr_in);

    char dev[MAX_SIZES];
    char cmd[MAX_SIZES];
    char buf[MAX_SIZES];

    pthread_t thr;
    struct epoll_event ev;
    struct sockaddr_in clientaddr;

    while (1) {
        n = epoll_wait(server->efd, server->events_list, EVENT_SIZES, -1);
        if (n == -1) {
            syslog(LOG_ERR, "epoll_wait failed");
            exit(1);
        }

        for (int i = 0; i < n; i++) {
            if (server->events_list[i].data.fd == server->sockfd) {
                client_sd = accept(server->sockfd,
                                   (struct sockaddr*)&clientaddr,
                                   (socklen_t*)&socklen);

                if (client_sd == -1) {
                    syslog(LOG_ERR, "accept failed");
                    continue;
                }

                syslog(LOG_INFO, "client connected: ip=%s, fd=%d",
                       inet_ntoa(clientaddr.sin_addr), client_sd);

                ev.events = EPOLLIN;
                ev.data.fd = client_sd;

                if (epoll_ctl(server->efd, EPOLL_CTL_ADD, client_sd, &ev) == -1) {
                    syslog(LOG_ERR, "epoll_ctl client failed: fd=%d", client_sd);
                    close(client_sd);
                    continue;
                }
            } else {
                int sender_fd = server->events_list[i].data.fd;

                numbytes = read(sender_fd, buf, MAX_SIZES - 1);
                if (numbytes <= 0) {
                    epoll_ctl(server->efd, EPOLL_CTL_DEL, sender_fd, NULL);
                    syslog(LOG_INFO, "client disconnected: fd=%d", sender_fd);
                    close(sender_fd);
                    continue;
                }

                buf[numbytes] = '\0';

                dev[0] = '\0';
                cmd[0] = '\0';
                sscanf(buf, "%s %s", dev, cmd);

                if (strcmp(dev, "LED") == 0) {
                    char* arg = strdup(cmd);
                    if (arg == NULL) {
                        syslog(LOG_ERR, "strdup failed for LED command");
                        continue;
                    }

                    if (pthread_create(&thr, NULL, led_thread, arg) != 0) {
                        syslog(LOG_ERR, "pthread_create led_thread failed");
                        free(arg);
                        continue;
                    }

                    pthread_detach(thr);
                }
                else if (strcmp(dev, "BUZZER") == 0) {
                    char* arg = strdup(cmd);
                    if (arg == NULL) {
                        syslog(LOG_ERR, "strdup failed for BUZZER command");
                        continue;
                    }

                    if (pthread_create(&thr, NULL, buzzer_thread, arg) != 0) {
                        syslog(LOG_ERR, "pthread_create buzzer_thread failed");
                        free(arg);
                        continue;
                    }

                    pthread_detach(thr);
                }
                else if (strcmp(dev, "SEG") == 0) {
                    char* arg = strdup(cmd);
                    if (arg == NULL) {
                        syslog(LOG_ERR, "strdup failed for SEG command");
                        continue;
                    }

                    if (pthread_create(&thr, NULL, seg_thread, arg) != 0) {
                        syslog(LOG_ERR, "pthread_create seg_thread failed");
                        free(arg);
                        continue;
                    }

                    pthread_detach(thr);
                }
                else if (strcmp(dev, "CDS") == 0) {
                    int* p = malloc(sizeof(int));
                    if (p == NULL) {
                        syslog(LOG_ERR, "malloc failed for CDS fd");
                        continue;
                    }

                    *p = sender_fd;

                    if (pthread_create(&thr, NULL, cds_thread, p) != 0) {
                        syslog(LOG_ERR, "pthread_create cds_thread failed");
                        free(p);
                        continue;
                    }

                    pthread_detach(thr);
                }
                else if (strcmp(dev, "TEMP") == 0) {
                    int* p = malloc(sizeof(int));
                    if (p == NULL) {
                        syslog(LOG_ERR, "malloc failed for TEMP fd");
                        continue;
                    }

                    *p = sender_fd;

                    if (pthread_create(&thr, NULL, temp_thread, p) != 0) {
                        syslog(LOG_ERR, "pthread_create temp_thread failed");
                        free(p);
                        continue;
                    }

                    pthread_detach(thr);
                }
                else {
                    syslog(LOG_WARNING, "unknown command: %s", buf);
                }
            }
        }
    }
}

int main(void)
{
    server_t server;

    daemonize();
    server_init(&server);
    run_server(&server);

    return 0;
}