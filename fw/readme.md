# 블루투스 트리거 카운터 (BLE Trigger Counter)

이 프로젝트는 ESP32를 기반으로 한 블루투스 트리거 카운터 시스템입니다. 주로 총기의 발사 횟수를 카운트하고 탄약 관리 기능을 제공하는 디바이스로, BLE(Bluetooth Low Energy)를 통해 스마트폰이나 다른 기기와 통신할 수 있습니다.

## 주요 기능

- 트리거 감지 및 카운팅
- BLE를 통한 무선 통신
- 탄약 관리 (최대 탄약 수 설정, 현재 탄약 수 모니터링)
- 탄창 삽입 여부 감지
- 배터리 상태 모니터링
- 네오픽셀 LED를 통한 상태 표시 (배터리 레벨, 연결 상태)
- 액츄에이터를 통한 발사 중지 기능

## 하드웨어 요구사항

- ESP32 기반 보드 (Seed XIAO ESP32C3 권장)
- 트리거 센서 (홀 센서 등)
- 탄창 삽입 감지 센서
- 네오픽셀 LED
- 배터리 (리튬 폴리머 배터리 권장)
- 액츄에이터 (발사 중지용)

## 핀 설정

SEED XIAO ESP32C3 기준:
- D3: 액츄에이터 1 (발사 중지용)
- D1: 트리거 감지 핀
- D2: 탄창 삽입 여부 감지 핀
- A0: 배터리 전압 측정 핀
- D10: 네오픽셀 제어 핀

## 설치 및 설정

1. Arduino IDE 또는 PlatformIO를 사용하여 코드를 ESP32 보드에 업로드합니다.
2. 필요한 라이브러리:
   - Adafruit_NeoPixel
   - BLEDevice
   - TaskScheduler
   - ArduinoJson

## 설정 저장소

이 프로젝트는 EEPROM을 사용하여 설정을 저장합니다. 저장되는 주요 설정:
- 패스워드 (기본값: "1111")
- 디바운스 딜레이 (기본값: 50ms)
- 최대 탄약 수 (기본값: 30)

## 블루투스 연결 정보

- 장치 이름: "BSQTC_" + ChipID
- 서비스 UUID: `2ca354b0-5f62-11ef-b4d4-f7af9038ee7d`
- 특성 UUID: `35c34c80-5f62-11ef-b4d4-f7af9038ee7d`

## 명령어 시스템

이 시스템은 시리얼 또는 BLE를 통해 명령어를 받아 처리할 수 있습니다. 모든 응답은 JSON 형식으로 반환됩니다.

### 기본 명령어

#### about
시스템 정보를 반환합니다.

**명령어:**
```
about
```

**응답 예시:**
```json
{
  "result": "ok",
  "os": "cronos-v1",
  "app": "bleTC",
  "version": "1.0.2",
  "author": "gbox3d",
  "chipid": "3C71BF6D0270"
}
```

#### reboot
시스템을 재부팅합니다.

**명령어:**
```
reboot
```

#### clear
트리거 카운트를 초기화하고 탄약 수를 최대치로 리셋합니다.

**명령어:**
```
clear
```

**응답 예시:**
```json
{
  "result": "ok",
  "ms": "trigger count cleared"
}
```

#### status
현재 시스템 상태를 확인합니다.

**명령어:**
```
status
```

**응답 예시:**
```json
{
  "result": "ok",
  "count": 15,
  "magazineInserted": true,
  "battery": 85,
  "ammo": 15,
  "maxAmmo": 30,
  "firingEnabled": true
}
```

### 설정 관련 명령어 (config)

#### 설정 불러오기
EEPROM에서 설정을 불러옵니다.

**명령어:**
```
config load
```

**응답 예시:**
```json
{
  "result": "ok",
  "ms": "config loaded"
}
```

#### 설정 저장하기
현재 설정을 EEPROM에 저장합니다.

**명령어:**
```
config save
```

**응답 예시:**
```json
{
  "result": "ok",
  "ms": "config saved"
}
```

#### 설정 덤프
현재 모든 설정을 JSON 형태로 출력합니다.

**명령어:**
```
config dump
```

**응답 예시:**
```json
{
  "result": "ok",
  "cfg": {
    "password": "1111",
    "debounceDelay": 50,
    "maxAmmoCount": 30
  }
}
```

#### 설정 초기화
모든 설정을 초기화합니다.

**명령어:**
```
config clear
```

**응답 예시:**
```json
{
  "result": "ok",
  "ms": "config cleared"
}
```

#### 설정 값 변경
특정 설정 값을 변경합니다.

**명령어:**
```
config set [키] [값]
```

**예시:**
```
config set debounceDelay 100
```

**응답 예시:**
```json
{
  "result": "ok",
  "ms": "config set"
}
```

#### 설정 값 가져오기
특정 설정 값을 가져옵니다.

**명령어:**
```
config get [키]
```

**예시:**
```
config get debounceDelay
```

**응답 예시:**
```json
{
  "result": "ok",
  "value": "100"
}
```

#### JSON 배열 설정하기
JSON 배열 형태의 설정을 저장합니다.

**명령어:**
```
config setA [키] [JSON 배열]
```

**예시:**
```
config setA targets ["target1", "target2", "target3"]
```

**응답 예시:**
```json
{
  "result": "ok",
  "ms": ["target1", "target2", "target3"]
}
```

### 탄약 관련 명령어 (ammo)

#### 탄약 정보 조회
현재 탄약 상태를 조회합니다.

**명령어:**
```
ammo
```

**응답 예시:**
```json
{
  "result": "ok",
  "ammo": 15,
  "maxAmmo": 30,
  "firingEnabled": true
}
```

#### 현재 탄약 수 설정
현재 탄약 수를 지정합니다.

**명령어:**
```
ammo set [수량]
```

**예시:**
```
ammo set 20
```

**응답 예시:**
```json
{
  "result": "ok",
  "ammo": 20
}
```

#### 최대 탄약 수 설정
최대 탄약 수를 지정합니다. 이 설정은 EEPROM에 저장됩니다.

**명령어:**
```
ammo setmax [수량]
```

**예시:**
```
ammo setmax 50
```

**응답 예시:**
```json
{
  "result": "ok",
  "maxAmmo": 50
}
```

#### 탄약 리셋
탄약 수를 최대치로 리셋합니다.

**명령어:**
```
ammo reset
```

**응답 예시:**
```json
{
  "result": "ok",
  "ammo": 50
}
```

#### 발사 중지
액츄에이터를 활성화하여 발사를 중지합니다.

**명령어:**
```
ammo stop
```

**응답 예시:**
```json
{
  "result": "ok",
  "ms": "firing stopped",
  "firingEnabled": false
}
```

#### 발사 재개
탄약이 남아 있다면 발사를 재개합니다.

**명령어:**
```
ammo resume
```

**응답 예시:**
```json
{
  "result": "ok",
  "ms": "firing resumed",
  "firingEnabled": true,
  "ammo": 20
}
```

### BLE 관련 명령어 (ble)

#### BLE 정보 조회
BLE 연결 관련 정보를 조회합니다.

**명령어:**
```
ble info
```

**응답 예시:**
```json
{
  "result": "ok",
  "name": "BSQTC_3C71BF6D0270",
  "address": "3C:71:BF:6D:02:70",
  "serviceUUID": "2ca354b0-5f62-11ef-b4d4-f7af9038ee7d",
  "characteristicUUID": "35c34c80-5f62-11ef-b4d4-f7af9038ee7d",
  "mtuSize": "23",
  "connection": true
}
```

## BLE 상태 알림

디바이스는 트리거 카운트가 변경될 때마다 BLE를 통해 자동으로 상태를 전송합니다. 알림 포맷은 다음과 같습니다:

```
#,[트리거 카운트],[탄약 수],[발사 가능 상태],[탄창 삽입 여부],[배터리 레벨],0
```

**예시:**
```
#,15,15,1,1,85,0
```

## 기술 상세

### 트리거 카운터 모듈
- 인터럽트 기반 트리거 감지
- 디바운스 처리로 오작동 방지
- 원자적 카운터 변수를 통한 안정적인 카운팅

### 전력 관리
- 배터리 전압 측정 및 상태 표시
- 네오픽셀을 통한 배터리 상태 시각화

### 사용자 인터페이스
- 네오픽셀 LED를 통한 상태 표시
  - 녹색: 배터리 80% 이상
  - 노란색: 배터리 50-79%
  - 주황색: 배터리 20-49%
  - 빨간색: 배터리 20% 미만
  - 파란색: BLE 연결됨

## 라이센스

이 프로젝트는 MIT 라이센스 하에 배포됩니다.

## 제작자

gbox3d