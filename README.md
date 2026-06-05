# raspi-tcp-device-control - TCP 장치 제어 시스템

## 프로젝트 개요
Raspberry Pi 4와 Ubuntu(UTM)를 활용하여 TCP 소켓 통신 기반의 원격 장치 제어 시스템을 구현했습니다.
클라이언트에서 명령을 전송하면 서버가 명령을 파싱하여 각 장치를 제어합니다.

## 실행 전 설정

환경에 맞게 아래 값을 수정해야 합니다.

### Makefile - 라즈베리파이 접속 정보
빌드한 서버와 라이브러리를 전송할 라즈베리파이 주소와 경로를 본인 환경에 맞게 수정합니다.

    RASPI_HOST = 사용자명@라즈베리파이_IP
    RASPI_DIR  = ~/전송받을_디렉토리

예- `RASPI_HOST = pi@192.168.0.10`

### client.c - 서버 포트 (선택)
기본 포트는 60000으로 설정되어 있으며 필요 시 수정합니다.
서버(server.c)의 SERVER_PORT와 동일하게 맞춰야 합니다.

    #define SERVER_PORT 60000


## 빌드 및 실행
    # 라이브러리 + 서버(라즈베리파이로 자동 전송) / 클라이언트 빌드
    make

    # 라즈베리파이에서 서버 실행
    sudo ./server

    # 우분투에서 클라이언트 실행
    ./client \[라즈베리파이 IP\]

    # 데몬 로그 확인
    sudo tail -f /var/log/syslog | grep device_server
    

## 장치 제어 구현 기능

### LED
- 클라이언트에서 ON/OFF 제어
- 밝기 최대/중간/최저 조절 제어

### 부저
- 클라이언트에서 음악 소리 ON/OFF 제어

### 조도센서
- 클라이언트에서 조도 센서 값 확인
- 빛이 감지되지 않으면 LED ON, 빛이 감지되면 LED OFF 자동 제어

### 7세그먼트
- 클라이언트에서 전송한 숫자(0~9) 표시 (초 단위 시간)
- 1초 지날 때마다 -1씩 감소 표시
- 0이 되면 부저 울림

### 온도센서 (추가기능)
- PCF8591 AIN1 채널을 통해 NTC thermistor 값 읽기
- 실내 온도(23°C) 기준으로 보정계수(10.17) 산출하여 온도 변환
- 클라이언트에서 현재 온도 값 확인

## 환경
| 구분 | 환경 |
|------|------|
| 서버 | Raspberry Pi 4 |
| 클라이언트 | UTM Ubuntu (macOS) |
| 개발 | macOS + VS Code |
| 통신 | TCP (port 60000) |

## 장치 구성
| 장치 | 연결 방식 | BCM | wiringPi |
|------|----------|-----|----------|
| LED | GPIO | BCM 18 | 1 |
| BUZZER | GPIO | BCM 22 | 3 |
| SEG_A | GPIO | BCM 23 | 4 |
| SEG_B | GPIO | BCM 24 | 5 |
| SEG_C | GPIO | BCM 25 | 6 |
| SEG_D | GPIO | BCM 12 | 26 |
| CDS/TEMP | I2C (PCF8591 0x48) | - | - |

## 시스템 구조
클라이언트 (우분투)
→ TCP 명령 전송 (LED ON, BUZZER OFF 등)
→ 서버 (라즈베리파이 데몬)
→ epoll로 명령 수신
→ 명령마다 스레드 생성
→ dlopen/dlsym으로 동적 라이브러리 로딩
→ 장치 제어

## 명령어 프로토콜
| 명령 | 동작 |
|------|------|
| LED ON/OFF | LED 켜기/끄기 |
| LED HIGH/MID/LOW | 밝기 조절 |
| BUZZER ON/OFF | 부저 켜기/끄기 |
| SEG \[0-9\] | 카운트다운 시작 |
| CDS | 조도센서 값 반환 + 빛에 따라 LED 제어 |
| TEMP | 온도값 반환 |

## 세부 구현 내용

### 추가 기능 구현
- 온도 측정 기능 , epoll 기반 다중 접속 기능 , 서버 동작 및 에러 로그 추출 기능

### 공유 라이브러리 (동적 라이브러리)
- 각 장치 제어 코드를 led.c, buzzer.c, cds.c, segment.c, temperature.c로 분리하고 각각 독립적인 .so 파일로 빌드했습니다.
- 하나로 합치지 않은 이유는 특정 장치 함수를 수정했을 때 make를 통해 해당 파일만 재컴파일하여 라즈베리파이로 전송하기 위함입니다. 수정되지 않은 라이브러리는 재컴파일 없이 그대로 사용되어 자원 소모가 줄어듭니다.

### dlopen / dlsym 동적 로딩
- 서버는 컴파일 시점에 라이브러리를 링크하지 않고 런타임에 dlopen으로 .so 파일을 로딩합니다.
- load_lib() 함수로 라이브러리 경로와 함수 이름 배열을 받아 dlsym으로 함수 포인터를 일괄 등록하는 방식으로 공통화했습니다.
- 각 함수 포인터를 함수 원형 형태로 선언하여 가독성을 높였습니다.

### epoll 기반 서버 (추가기능)
- epoll_create로 이벤트 인스턴스를 생성하고, 서버 소켓과 접속한 클라이언트 소켓을 EPOLLIN 이벤트로 등록합니다.
- epoll_wait로 이벤트를 감지하여 서버 소켓 이벤트면 accept로 새 클라이언트를 등록하고, 클라이언트 소켓 이벤트면 명령을 수신하여 처리합니다.
- 클라이언트 연결이 종료되면 epoll에서 제거하고 소켓을 닫습니다.

### 멀티스레드 명령 처리
- 클라이언트 명령을 수신하면 device와 cmd로 파싱한 뒤 모든 명령을 장치별 스레드로 생성하여 처리합니다.
- 각 스레드는 dlopen으로 라이브러리를 로딩하고 장치를 제어한 뒤 pthread_detach로 자원을 자동 해제합니다.
- 명령마다 스레드를 분리함으로써 부저 음악 재생이나 세그먼트 카운트다운처럼 시간이 걸리는 작업이 실행되어도 메인 epoll 루프가 블록되지 않고 다른 명령을 계속 처리할 수 있습니다.

### 뮤텍스 동기화
- BUZZER와 SEG는 pthread_mutex_trylock을 사용하여 이미 실행 중이면 새 명령을 무시합니다.
- 부저 OFF는 volatile int buzzer_stop 전역 플래그로 ON 스레드의 재생 루프를 중단시킵니다.

### 서버 데몬화
- fork() 후 부모 프로세스를 종료하고 setsid()로 새 세션을 생성하여 제어 터미널과 분리합니다.
- 작업 디렉토리를 변경하고 표준 입출력을 /dev/null로 리다이렉션합니다.
- syslog로 서버 동작 및 에러 로그를 기록합니다. (추가기능)
```bash
2026-06-05T12:43:23.735601+09:00 jambaek-desktop device_server[31788]: device server daemon started
2026-06-05T12:43:23.736167+09:00 jambaek-desktop device_server[31788]: server initialized: port=60000
2026-06-05T12:43:27.343127+09:00 jambaek-desktop device_server[31788]: client connected: ip=100.89.181.97, fd=7
2026-06-05T12:43:34.237638+09:00 jambaek-desktop device_server[31788]: client disconnected: fd=7
```

### 실행 경로 자동 감지
- 데몬화 시 작업 디렉토리가 변경되어도 라이브러리를 정상 로딩하도록 실행 파일 경로를 자동 감지합니다.
- readlink("/proc/self/exe")로 서버 실행 파일의 절대 경로를 구하고 dirname()으로 디렉토리를 추출하여 lib_path에 저장합니다.
- 각 스레드는 lib_path와 .so 파일명을 조합한 절대경로로 dlopen을 호출하여, 환경에 관계없이 실행 파일과 같은 위치의 라이브러리를 로딩합니다.

### 클라이언트 시그널 처리
- SIGINT(Ctrl+C)만 핸들러로 처리하여 소켓을 닫고 정상 종료합니다.
- 다른 시그널은 무시하여 실행 도중 강제 종료되지 않도록 했습니다.



## 개발 일정
| 일차 | 내용 |
|------|------|
| Day 1 | 하드웨어 단독 테스트 (LED, 부저, 조도센서, 7세그먼트, 온도센서) |
| Day 2 | 장치별 공유 라이브러리 생성, dlopen/dlsym 동적 로딩, TCP 서버/클라이언트 기본 구현 |
| Day 3 | 서버 데몬화, 뮤텍스 적용, 코드 최적화, 시그널 처리 |

## 폴더 구성
| 폴더 | 설명 |
|------|------|
| day1_hardware_test/ | 각 장치 단독 테스트 소스코드 |
| day2_library_tcp/ | 공유 라이브러리 + TCP 기본 구현 |
| day3_daemon_refactoring/ | 데몬화 + 코드 최적화 |
| final_project/ | 제출용 최종 결과물 |


## 문제점 및 보완 사항


### 데몬화 후 라이브러리 경로 문제
- 서버를 데몬화하면 `chdir("/")`로 작업 디렉토리가 루트로 변경됩니다.
- 이 상태에서 `dlopen("./libled.so", ...)`처럼 상대경로로 라이브러리를 로딩하면 루트(/)에서 .so 파일을 찾기 때문에 로딩에 실패합니다.
- 초기에는 `#define SERVER_WORKDIR "/home/jambaek/fortest"`처럼 작업 디렉토리를 절대경로로 하드코딩했으나, 이 방식은 다른 사용자 환경에서는 경로가 달라 동작하지 않는 문제가 있었습니다.
- 이를 해결하기 위해 `readlink("/proc/self/exe", ...)`로 서버 실행 파일의 절대 경로를 구하고, `dirname()`으로 실행 파일이 위치한 디렉토리를 추출하여 `lib_path`에 저장했습니다.
- 각 스레드에서 `lib_path`와 라이브러리 파일명을 조합하여 절대경로로 dlopen을 호출함으로써, 데몬화로 작업 디렉토리가 변경되더라도 실행 파일과 같은 위치의 .so 파일을 정확히 로딩할 수 있도록 개선했습니다.
- 이를 통해 사용자 환경에 관계없이 서버 실행 파일과 .so 파일이 같은 디렉토리에 있기만 하면 정상 동작하도록 경로 의존성을 제거했습니다.

```c
char lib_path[PATH_MAX];

int getLibDir()
{
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);

    if(len != -1)
    {
        exe_path[len] = '\0';
        char* dir = dirname(exe_path);
        sprintf(lib_path, "%s/", dir);
        return 0;
    }
    return -1;
}

// 각 스레드에서 절대경로로 라이브러리 로딩
char temp_lib[PATH_MAX + 20];
sprintf(temp_lib, "%slibled.so", lib_path);
void* led_lib = load_lib(temp_lib, led_funcs, led_ptrs, 3);
```

### 세그먼트/부저 중복 실행 문제
- 세그먼트 카운트다운 중 새로운 SEG 명령이 오거나, 부저 재생 중 새로운 BUZZER 명령이 오면
  두 스레드가 동시에 같은 장치에 접근하여 충돌이 발생하는 문제가 있었습니다.
- `pthread_mutex_trylock`을 적용하여 이미 실행 중인 스레드가 있으면
  새로 들어온 명령을 무시하는 방식으로 해결했습니다.
- `trylock`을 사용한 이유는 `lock` 방식은 대기 후 실행되어 중복 실행을 막지 못하기 때문입니다.

### 부저 OFF 명령 처리 문제
- `BUZZER OFF`로 소리를 끄더라도 `BUZZER ON` 스레드는 뮤텍스를 점유한 채로 for 루프를 계속 실행하여 노래가 끝날 때까지 뮤텍스가 해제되지 않는 문제가 추가로 발생했습니다.
- 이 상태에서 새로운 `BUZZER ON` 명령이 오면 `trylock`에 실패하여 소리가 나지 않고, 세그먼트 카운트다운이 끝났을 때 부저가 울려야 하는데 울리지 못하는 상황이 발생했습니다.
- 이를 해결하기 위해 `volatile int buzzer_stop` 전역 플래그를 선언하고 `OFF` 명령 시 `buzzer_stop = 1`로 설정했습니다.
- `buzzer_on()` 함수 내부 for 루프에서 매 반복마다 `buzzer_stop` 플래그를 확인하고 1이면 즉시 break하여 루프를 중단합니다.
- for 루프를 빠져나온 ON 스레드가 `pthread_mutex_unlock()`을 호출하여 스스로 뮤텍스를 반납하게 됩니다.
- 이를 통해 OFF 명령 후 즉시 뮤텍스가 해제되어 새로운 BUZZER 명령이나 세그먼트 종료 시 부저가 정상적으로 동작하도록 개선했습니다.

```c
// buzzer_on() 내부
for(int i = 0; i < TOTAL; i++)
{
    if(buzzer_stop) break;  // 플래그 확인 후 루프 중단
    softToneWrite(SPKR, notes[i]);
    delay(280);
}

// ON 스레드에서 뮤텍스 반납
pthread_mutex_unlock(&buz_mutex);

// OFF 명령 처리 - buzzer_stop 플래그로 ON 스레드 루프 중단 유도
if (strcmp(cmd, "OFF") == 0) {
    buzzer_stop = 1;
    buzzer_off();
    free(arg);
    dlclose(buz_lib);
    return NULL;
}
```
### 스레드 인자 전달 문제

- 서버는 클라이언트 명령을 수신한 뒤 `cmd` 배열에 명령 값을 저장하고, 장치별 스레드를 생성하여 처리합니다.
- 초기에는 스레드에 `cmd`의 주소를 직접 넘길 경우, `cmd`가 `run_server()` 내부의 지역 배열이기 때문에 다음 명령 수신 시 값이 변경될 수 있는 문제가 있었습니다.
- 이 경우 스레드가 실행되는 시점에 원래 명령이 아닌 변경된 명령을 읽어 잘못된 장치 제어가 발생할 수 있습니다.
- 이를 방지하기 위해 `strdup()`을 사용하여 명령 문자열을 동적으로 복사한 뒤 스레드 인자로 전달했습니다.

```c
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
```

### 다중 클라이언트 장치 공유 문제
- epoll로 다중 클라이언트 접속은 지원하지만 제공 받은 보드가 하나뿐이라 장치 제어 상태는 클라이언트 간 공유됩니다.
- 예를 들어 클라이언트 A가 BUZZER ON 실행 중일 때 클라이언트 B가 BUZZER OFF 명령을 보내면 A의 재생이 중단됩니다.
- 완전한 다중 클라이언트 지원을 위해서는 클라이언트별로 장치 상태를 독립적으로 관리하는 구조로 개선이 필요합니다.
