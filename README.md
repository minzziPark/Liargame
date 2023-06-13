![header](https://capsule-render.vercel.app/api?type=waving&color=auto&height=300&section=header&text=Liar%20game&fontSize=90&animation=fadeIn&fontAlignY=38&desc=&descAlignY=51&descAlign=62)

# 🎮 MQTT를 이용한 Liar game 🎮

라이어 게임이란 한가지 주제에 동일한 단어를 가지고 단 한 명의 라이어만 그 단어를 알지 못한 채 라이어를 찾아내는 게임이다.

## Nevigator
- [Liar game 설명](#Liar-game-설명)
- [1. setting](#1.-setting)
- [2. 추가 setting](#2.-추가-setting)
- [3. 실행방법](#3.-실행방법)

## Liar game 설명
- 라이어는 본인이 라이어임을 들키면 안된다.
- Player 서로 순서대로 돌아가면서 그 단어에 대해 설명한다.
- Player1부터 한 사람씩 다 얘기하면 라이어가 누구인지 예측한다.
- 지목당한 사람은 단어를 이야기 한다. 만약 라이어가 단어를 맞추면 라이어 승, 아니면 나머지 승리
- 라이어를 찾지 못해도 라이어는 승리한다. 

## 1. setting
- 환경
실제 구축 환경은 __VirtualBox__ 을 이용하여 __우분투(Ubuntu)__ 를 사용하였다. 

## 2. 추가 setting
- 본 프로젝트에서는 tcp와 mosquitto를 사용하여 게임을 구현하였다.   
또한 MYSQL을 이용하여 게임의 상황을 저장할 수 있는 database를 구축하였다.
- [mosquitto 설치방법](#HOW-TO-INSTALL-MOSQUITTO)
- [mysql 설치방법](#HOW-TO_INSTALL-MYSQL)

#### HOW TO INSTALL MOSQUITTO
📍터미널에서 설치
```
$ sudo apt-get update && sudo apt-get upgrade
$ sudo apt-get install mosquitto
$ sudo /etc/init.d/mosquitto status
    // active(running)이라고 떠야함
$ sudo apt update 
$ sudo apt install libmosquitto-dev
```

#### HOW TO INSTALL MYSQL
📍터미널에서 설치
```
$ sudo apt-get install aptitude
$ sudo aptitude install mysql-server
$ sudo aptitude install mysql-client 
$ mysql_config --cflags
```
## 3. 실행방법
- 시작하기 전 ip 주소를 알고 있어야 한다. 
```
$ ip addr //를 통해 ip 확인
```

- mosquitto와 mysql을 설치한다.
    - [설치방법](#HOW-TO-INSTALL-MOSQUITTO)
    - [설치방법](#HOW-TO_INSTALL-MYSQL)
- 게임에 사용되는 소스코드를 다운받는다.
    - liar_client.c
    - liar_server.c
    - log_sub.c
- 각 소스파일을 컴파일한다. (이때 mosquitto와 mysql이 정상 설치가 되어있어야 한다.)
```
$ gcc liar_client.c -o liar_client -lmosquitto
$ gcc liar_server.c -o liar_server -lpthread -lmosquitto
$ gcc log_sub.c -o log_sub -lmosquitto -lmysqlclient
```

