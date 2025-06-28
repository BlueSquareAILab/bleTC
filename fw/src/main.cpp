#include <Arduino.h>

#include <Adafruit_NeoPixel.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// #include <WiFi.h>
#include <vector>

#include <TaskScheduler.h>
#include <ArduinoJson.h>

#include "triggerCounter.hpp"  // A3144.hpp 대신 새 이름으로 변경

#include "config.hpp"
#include "etc.hpp"

Scheduler g_ts;
Config g_config;

extern String ParseCmd(String _strLine);

#if defined(SEED_XIAO_ESP32C3)

const int actionPin1 = D3;       // 액츄에이터 1 (발사 중지용)

const int triggerPin = D1;       // 트리거 감지 핀
const int magazineInsertedPin = D2;  // 탄창 삽입 여부 감지 핀 (이전 modePin)

const int batteryPin = A0;       // 배터리 전압 측정 핀
const int neoPixelPin = D10;      // 네오픽셀 제어 핀 (이전 batStatusPin)

// 네오픽셀 설정 (픽셀 수에 맞게 조정)
#define NUM_PIXELS 1
Adafruit_NeoPixel pixels(NUM_PIXELS, neoPixelPin, NEO_GRB + NEO_KHZ800);

// 탄약 관련 설정
int maxAmmoCount = 30;           // 최대 탄약 수
int currentAmmoCount = 30;       // 현재 탄약 수
bool firingEnabled = true;       // 발사 가능 상태

// 네오픽셀 상태 플래그
bool showBatteryColor = true;    // true면 배터리 상태, false면 연결 상태 표시

#elif defined(TENSTAR_ESP32C3)

const int actionPin1 = 4;       // 액츄에이터 1 (발사 중지용)

const int triggerPin = 1;       // 트리거 감지 핀
const int magazineInsertedPin = 3;  // 탄창 삽입 여부 감지 핀 (이전 modePin)

const int batteryPin = 0;       // 배터리 전압 측정 핀
const int neoPixelPin = 10;      // 네오픽셀 제어 핀 (이전 batStatusPin)

// 네오픽셀 설정 (픽셀 수에 맞게 조정)
#define NUM_PIXELS 1
Adafruit_NeoPixel pixels(NUM_PIXELS, neoPixelPin, NEO_GRB + NEO_KHZ800);

// 탄약 관련 설정
int maxAmmoCount = 30;           // 최대 탄약 수
int currentAmmoCount = 30;       // 현재 탄약 수
bool firingEnabled = true;       // 발사 가능 상태

// 네오픽셀 상태 플래그
bool showBatteryColor = true;    // true면 배터리 상태, false면 연결 상태 표시

#define LED_BUILTIN 8

#else
#define LED_BUILTIN 4
#endif

// UUID for service and characteristic
#define SERVICE_UUID "2ca354b0-5f62-11ef-b4d4-f7af9038ee7d"
#define CHARACTERISTIC_UUID "35c34c80-5f62-11ef-b4d4-f7af9038ee7d"

BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
bool deviceConnected = false;

// 배터리 관련 설정
const float MIN_BATTERY_VOLTAGE = 3.0;  // 최소 배터리 전압
const float MAX_BATTERY_VOLTAGE = 4.2;  // 최대 배터리 전압

bool getConnectionStatus() {
    return deviceConnected;
}

String getServiceUUID() {
    return String(SERVICE_UUID);
}

String getCharacteristicUUID() {
    return String(CHARACTERISTIC_UUID);
}

String getAddress() {
    return BLEDevice::getAddress().toString().c_str();
}

String getMtuSize() {
    return String(pServer->getPeerMTU(pServer->getConnId()));
}

String getDeviceName() {
    return "BSQTC_" + getChipID();
}

void clearTriggerCount() {
    TriggerCounter::clearTriggerCount();
    currentAmmoCount = maxAmmoCount;  // 트리거 카운트 초기화 시 탄약 수도 초기화
    firingEnabled = true;            // 발사 가능 상태로 설정
    digitalWrite(actionPin1, LOW);   // 액츄에이터 비활성화
}

int getTriggerCount() {
    return TriggerCounter::getTriggerCount();
}

// 펄스 종료를 위한 태스크 선언
Task task_EndPulse(TASK_IMMEDIATE, TASK_ONCE, []() {
    digitalWrite(actionPin1, LOW);  // 펄스 종료 (액츄에이터 비활성화)
}, &g_ts, false);  // 초기에는 비활성화 상태

void doActuatorPulse(int duration = 1000) {
    digitalWrite(actionPin1, HIGH);  // 액츄에이터 활성화
    task_EndPulse.restartDelayed(duration);  // 주어진 시간 후에 펄스 종료 태스크 예약
}

// 수정된 decreaseAmmoCount 함수
void decreaseAmmoCount() {
    currentAmmoCount--;
    if (currentAmmoCount <= 0) {
        currentAmmoCount = 0;
        firingEnabled = false;
        
        // 펄스 시작
        doActuatorPulse();
        
        // // 1초 후에 펄스 종료 태스크 예약
        // task_EndPulse.restartDelayed(1000);
    }
}



// 탄창 삽입 여부 확인 함수
bool isMagazineInserted() {
    return !digitalRead(magazineInsertedPin);  // LOW일 때 삽입된 상태
}

void resumeFiring() {
    firingEnabled = true;  // 발사 가능 상태로 설정
    // digitalWrite(actionPin1, LOW);  // 액츄에이터 비활성화
}

void stopFiring() {
    firingEnabled = false;  // 발사 불가능 상태로 설정
    // digitalWrite(actionPin1, HIGH);  // 액츄에이터 활성화하여 발사 중지
}


// 배터리 레벨 읽기 함수 (0-100%)
int getBatteryLevel() {
    float voltage = analogRead(batteryPin) * 3.3 / 4095 * 2;  // 전압 분배기 사용 시 곱하기 2
    int level = map(voltage * 100, MIN_BATTERY_VOLTAGE * 100, MAX_BATTERY_VOLTAGE * 100, 0, 100);
    level = constrain(level, 0, 100);  // 0-100 범위로 제한
    return level;
}

// 배터리 레벨에 따른 색상 반환 함수
uint32_t getBatteryColor(int batteryLevel) {
    if (batteryLevel >= 80) {
        return pixels.Color(0, 255, 0);  // 녹색 (충전 상태 좋음)
    } else if (batteryLevel >= 50) {
        return pixels.Color(255, 255, 0);  // 노란색 (중간 충전 상태)
    } else if (batteryLevel >= 20) {
        return pixels.Color(255, 165, 0);  // 주황색 (충전 필요)
    } else {
        return pixels.Color(255, 0, 0);  // 빨간색 (충전 필요 긴급)
    }
}

// 연결 상태에 따른 색상 반환 함수
uint32_t getConnectionColor(bool connected) {
    if (connected) {
        return pixels.Color(0, 0, 255);  // 파란색 (연결됨)
    } else {
        return pixels.Color(0, 0, 0);    // 검은색 (연결 안됨)
    }
}

// 네오픽셀 색상 업데이트 함수
void updateNeoPixelColor() {
    int batteryLevel = getBatteryLevel();
    uint32_t color;
    
    if (showBatteryColor) {
        // 배터리 상태 표시
        color = getBatteryColor(batteryLevel);
    } else {
        // 연결 상태 표시
        color = getConnectionColor(deviceConnected);
    }
    
    pixels.setPixelColor(0, color);
    pixels.show();
}

// 배터리 모니터링 태스크 - 더 이상 필요 없음 (LED 깜빡임 태스크에 통합)

// 네오픽셀 상태 토글 태스크 (0.5초마다 배터리 상태와 연결 상태 번갈아 표시)
Task task_NeoPixelBlink(500, TASK_FOREVER, []() {
    showBatteryColor = !showBatteryColor;  // 상태 토글
    updateNeoPixelColor();
}, &g_ts, true);

Task taskNotify(10, TASK_FOREVER, []() {
    static int oldValue = 0;
    static bool oldMagazineInserted = false;
    int _value = TriggerCounter::getTriggerCount();
    
    if (_value != oldValue || oldMagazineInserted != isMagazineInserted()) {
        // Serial.println("Trigger Count: " + String(_value));

        oldMagazineInserted = isMagazineInserted();
        
        // 탄 수 감소
        if (oldValue < _value && firingEnabled) {
            decreaseAmmoCount();
        }
        
        oldValue = _value;

        if (deviceConnected) {
            // BLE를 통해 현재 상태 전송
            String _data = "#," + String(_value) + "," + String(currentAmmoCount) + 
                          "," + String(firingEnabled) + "," + String(isMagazineInserted()) + 
                          "," + String(getBatteryLevel()) + ",0";
            pCharacteristic->setValue(_data.c_str());
            pCharacteristic->notify();
        }
    }
}, &g_ts, true); 

Task task_Cmd(100, TASK_FOREVER, []() {
    if (Serial.available() > 0) {
        String _strLine = Serial.readStringUntil('\n');
        _strLine.trim();
        Serial.println("Received Serial command:");
        Serial.println(_strLine);

        String response = ParseCmd(_strLine);
        Serial.println("Response:");
        Serial.println(response);
    }
}, &g_ts, true);

void printMtuSize(BLEServer *pServer) {
    uint16_t currentMtu = pServer->getPeerMTU(pServer->getConnId());
    Serial.print("current MTU size: ");
    Serial.println(currentMtu);
}

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) {
        deviceConnected = true;
        Serial.println("client connected");
        pServer->getAdvertising()->stop(); // 클라이언트 연결 시 광고 중지

        // 환영 메시지 설정 및 알림 전송
        pCharacteristic->setValue("welcome to ESP32 BLE Server");
        
        // 연결 상태 변경되었으므로 네오픽셀 색상 업데이트
        updateNeoPixelColor();

        printMtuSize(pServer);
    }

    void onDisconnect(BLEServer *pServer) {
        deviceConnected = false;
        Serial.println("client disconnected");
        pServer->getAdvertising()->start(); // 클라이언트 연결 해제 시 광고 재시작
        
        // 연결 상태 변경되었으므로 네오픽셀 색상 업데이트
        updateNeoPixelColor();
    }

    void onMtuChanged(BLEServer *pServer, uint16_t mtu) {
        Serial.print("MTU size changed to: ");
        Serial.println(mtu);
    }
};

class MyCharateristicCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();

        if (value.length() > 0) {
            Serial.println("Received BLE command:");
            Serial.println(value.c_str());

            String response = ParseCmd(String(value.c_str()));
            Serial.println("Response:");
            Serial.println(response);

            printMtuSize(pServer);

            // BLE를 통해 응답 전송
            pCharacteristic->setValue(response.c_str());
            pCharacteristic->notify();
        }
    }

    void onRead(BLECharacteristic *pCharacteristic) {
        Serial.println("BLE read : ");
        
        std::string value = pCharacteristic->getValue();
        Serial.println(value.c_str());
        
        printMtuSize(pServer);
    }
};

// the setup function runs once when you press reset or power the board
void setup() {
    // GPIO 설정
    pinMode(actionPin1, OUTPUT);
    digitalWrite(actionPin1, LOW);  // 초기 상태: 액츄에이터 비활성화
    
    // 탄창 감지 핀 설정
    pinMode(magazineInsertedPin, INPUT_PULLUP);

    // 네오픽셀 초기화
    pixels.begin();
    pixels.clear();
    pixels.show();

    Serial.begin(115200);

    g_config.load();

    Serial.println(":-]");
    Serial.println("Serial connected");

    // 설정에서 디바운스 딜레이 읽기
    u32_t debounceDelay = g_config.get<u32_t>("debounceDelay", 50);
    
    // 설정에서 최대 탄약 수 읽기 (없으면 기본값 30)
    maxAmmoCount = g_config.get<int>("maxAmmoCount", 30);
    currentAmmoCount = maxAmmoCount;  // 현재 탄약 수 초기화

    // 트리거 카운터 설정
    TriggerCounter::setup(triggerPin, debounceDelay);
    
    g_ts.startNow();

    // BLE 장치 생성
    BLEDevice::init(getDeviceName().c_str());
    
    // BLE 서버 생성
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // BLE 서비스 생성
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // BLE 특성 생성
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | // 읽기 속성
        BLECharacteristic::PROPERTY_WRITE | // 쓰기 속성
        BLECharacteristic::PROPERTY_NOTIFY | // 알림 속성
        BLECharacteristic::PROPERTY_INDICATE // 표시 속성
    );

    // 디스크립터 추가
    pCharacteristic->addDescriptor(new BLE2902());
    pCharacteristic->setCallbacks(new MyCharateristicCallbacks());

    // 서비스 시작
    pService->start();

    // 광고 시작
    pServer->getAdvertising()->start();

    Serial.println("BLE Ready....");
    
    // 초기 네오픽셀 색상 설정
    updateNeoPixelColor();
}

// the loop function runs over and over again forever
void loop() {
    g_ts.execute();
}