# Advanced Linux Practice - TCP 장치 제어 시스템

## 프로젝트 개요
Raspberry Pi 4와 Ubuntu(UTM)를 활용하여 TCP 소켓 통신 기반의 원격 장치 제어 시스템을 구현했습니다.
클라이언트에서 명령을 전송하면 서버가 명령을 파싱하여 각 장치를 제어합니다.

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
| LED | GPIO | - | 1 |
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

## 개발 일정
| 일차 | 내용 |
|------|------|
| Day 1 | 하드웨어 단독 테스트 (LED, 부저, 조도센서, 7세그먼트, 온도센서) |
| Day 2 | 장치별 공유 라이브러리 생성, dlopen/dlsym 동적 로딩, TCP 서버/클라이언트 기본 구현 |
| Day 3 | 서버 데몬화, 뮤텍스 적용, 코드 최적화, 시그널 처리 |
| Day 4 | 최종 문서 작성 |

## 폴더 구성
| 폴더 | 설명 |
|------|------|
| day1_hardware_test/ | 각 장치 단독 테스트 소스코드 |
| day2_library_tcp/ | 공유 라이브러리 + TCP 기본 구현 |
| day3_daemon_refactoring/ | 데몬화 + 코드 최적화 |
| day4_docs/ | 최종 문서 |
| final_project/ | 제출용 최종 결과물 |

## 핵심 구현 사항
- 장치별 독립 공유 라이브러리(.so) 분리로 개별 수정 가능
- dlopen/dlsym 동적 로딩으로 런타임 라이브러리 교체 가능
- epoll 기반 다중 클라이언트 접속 지원
- pthread 뮤텍스로 BUZZER, SEG 중복 실행 방지
- fork() + setsid() 데몬화로 백그라운드 실행
- syslog 기반 서버 로그 기록

## 문제점 및 보완 사항

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

## 빌드 및 실행
    # 우분투에서 라이브러리 + 서버(라즈베리파이로 자동 전송) / 클라이언트 빌드
    cd final_project
    make

    # 라즈베리파이에서 서버 실행
    sudo ./server

    # 우분투에서 클라이언트 실행
    ./client \[라즈베리파이 IP\]

    # 데몬 로그 확인
    sudo tail -f /var/log/syslog | grep device_server

