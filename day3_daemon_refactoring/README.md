# Day 3 - 데몬화 및 코드 최적화

## 개요
Day 2에서 구현한 TCP 서버/클라이언트를 기반으로 서버 데몬화, 코드 최적화,
뮤텍스 적용, 시그널 처리를 완성했습니다.

## 주요 변경 사항

### 서버 데몬화
서버를 백그라운드 데몬 프로세스로 실행하도록 구현했습니다.
- fork() + setsid()로 데몬 프로세스 생성
- 표준 입출력을 /dev/null로 리다이렉션
- syslog를 통한 로그 기록
- SERVER_WORKDIR 경로로 작업 디렉토리 변경

### 코드 최적화
**서버**
- server_t 구조체로 소켓/epoll 관련 변수 통합 관리
- server_init(), run_server()로 기능 분리
- printf 대신 syslog로 로그 처리

**클라이언트**
- signal_setup() - 시그널 설정
- client_init() - 소켓 생성 및 서버 연결
- run_menu() - 메뉴 출력 및 명령 처리
- clear_input() - 입력 버퍼 초기화

### 뮤텍스 적용
- BUZZER: trylock으로 중복 실행 방지, OFF 명령은 즉시 처리
- SEG: trylock으로 카운트다운 중복 실행 방지

### 클라이언트 시그널 처리
- SIGINT(Ctrl+C)만 종료 처리
- SIGQUIT, SIGTSTP, SIGPIPE, SIGTERM 무시

## 파일 구성
| 파일 | 설명 |
|------|------|
| server.c | 데몬화 + epoll 기반 TCP 서버 (라즈베리파이 실행) |
| client.c | 시그널 처리 + 메뉴 기반 TCP 클라이언트 (우분투 실행) |
| Makefile | 빌드 자동화 |

## 아키텍쳐
클라이언트 (우분투)
→ TCP 명령 전송
→ 서버 (라즈베리파이 데몬)
→ epoll로 명령 수신
→ 명령마다 스레드 생성
→ dlopen/dlsym으로 동적 라이브러리 로딩
→ 장치 제어

## 빌드 및 실행

### 빌드 순서
1. 장치별 공유 라이브러리(.so) 빌드
2. TCP 서버 빌드
3. TCP 클라이언트 빌드
4. 서버 실행파일과 공유 라이브러리를 Raspberry Pi로 자동 전송

### 실행
```bash
# 전체 빌드 및 라즈베리파이 전송
make

# 라즈베리파이에서 서버 실행(데몬화로 인해 백그라운드에서 서버가 계속 동작)
sudo ./server

# 우분투에서 클라이언트 실행
./client \[라즈베리파이 IP\]
```

## 데몬 로그 확인
    sudo tail -f /var/log/syslog | grep device_server

## 비고
- 부저 ON 중 OFF 명령 시 trylock으로 즉시 처리
- 조도센서 값 >= 205 이면 LED OFF, 미만이면 LED ON 자동 제어

## 체크포인트
- [x] 서버 데몬화 확인
- [x] syslog 로그 기록 확인
- [x] BUZZER 뮤텍스 중복 실행 방지 확인
- [x] SEG 뮤텍스 중복 실행 방지 확인
- [x] 클라이언트 SIGINT 종료 처리 확인
- [x] 조도센서 자동 LED 제어 확인