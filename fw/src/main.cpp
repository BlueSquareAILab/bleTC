#include <Arduino.h>

// #include <esp_system.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <WiFi.h>
#include <vector>

#include <TaskScheduler.h>
#include <ArduinoJson.h>
#include <atomic>  // atomic 헤더 추가



#include "config.hpp"
#include "etc.hpp"

Scheduler g_ts;
Config g_config;

extern String ParseCmd(String _strLine);

#ifdef SEED_XIAO_ESP32C3

const int ledPins_status = D10;
// const int analogPins[] = {D0, D1};
// const int buttonPins[] = {D8, D2};
const int triggerPin = D8;
const int modePin = D2;
const int batteryPin = A0;
const int batStatusPin[] = {D3, D4, D5};


#else
#define LED_BUILTIN 4
#endif

// UUID for service and characteristic
#define SERVICE_UUID "2ca354b0-5f62-11ef-b4d4-f7af9038ee7d"
#define CHARACTERISTIC_UUID "35c34c80-5f62-11ef-b4d4-f7af9038ee7d"


BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
bool deviceConnected = false;

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

// volatile int triggerCount = 0;
std::atomic<int> triggerCount(0);  // volatile int triggerCount = 0; 대신 사용

// 디바운스를 위한 변수들
volatile unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;  // 50ms의 디바운스 시간

// 인터럽트 처리 함수
void IRAM_ATTR handleTriggerInterrupt() {
    unsigned long currentTime = millis();
    if ((currentTime - lastDebounceTime) > debounceDelay) {
        triggerCount.fetch_add(1, std::memory_order_relaxed);  // atomic 연산으로 변경
        lastDebounceTime = currentTime;
    }
}

Task taskNotify(20, TASK_FOREVER, []() {
    if (deviceConnected) {
        int currentCount = triggerCount.load(std::memory_order_relaxed);  // atomic 값 읽기
        String _data = "#," + String(currentCount) + ",0,0,0";

        pCharacteristic->setValue(_data.c_str());
        pCharacteristic->notify();
    }
}, &g_ts, false); 

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

Task task_LedBlink(500, TASK_FOREVER, []()
              {
                //   digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
                digitalWrite(ledPins_status, !digitalRead(ledPins_status));
              }, &g_ts, true);


void printMtuSize(BLEServer *pServer) {
    uint16_t currentMtu = pServer->getPeerMTU(pServer->getConnId());
    Serial.print("current MTU size: ");
    Serial.println(currentMtu);
}

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) {

        task_LedBlink.disable();
        deviceConnected = true;

        // digitalWrite(LED_BUILTIN, HIGH);
        digitalWrite(ledPins_status, HIGH);
        Serial.println("client connected");
        pServer->getAdvertising()->stop(); // 클라이언트 연결 시 광고 중지

        // // 환영 메시지 설정 및 알림 전송
        pCharacteristic->setValue("welcome to ESP32 BLE Server");

        taskNotify.enable();

        printMtuSize(pServer);
    }

    void onDisconnect(BLEServer *pServer) {
        deviceConnected = false;
        // digitalWrite(LED_BUILTIN, LOW);
        digitalWrite(ledPins_status, LOW);
        Serial.println("client disconnected");
        pServer->getAdvertising()->start(); // 클라이언트 연결 해제 시 광고 재시작

        task_LedBlink.enable();
        taskNotify.disable();
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

// 배터리 정보를 읽는 함수 (이 함수는 실제 하드웨어에 맞게 구현해야 합니다)
int getBatteryLevel() {
    // 예시 코드: 실제로는 배터리 핀에서 아날로그 값을 읽어 변환해야 합니다
    int rawValue = analogRead(batteryPin);
    // 여기에서 rawValue를 실제 배터리 레벨(%)로 변환하는 로직을 구현해야 합니다
    return map(rawValue, 0, 4095, 0, 100);  // 예시: 0-4095 범위를 0-100%로 변환
}

// modePin 상태를 읽는 함수
bool getModeStatus() {
    return digitalRead(modePin);
}

// the setup function runs once when you press reset or power the board
void setup()
{

    pinMode(ledPins_status, OUTPUT);  
    
    //triggerPin interrupt
    pinMode(triggerPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(triggerPin), handleTriggerInterrupt, FALLING);

    //modePin
    pinMode(modePin, INPUT_PULLUP);
    

    Serial.begin(115200);

    g_config.load();

    Serial.println(":-]");
    Serial.println("Serial connected");
    
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
}

// the loop function runs over and over again forever
void loop()
{
  g_ts.execute();
}