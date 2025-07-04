#include <Arduino.h>

#include <Adafruit_NeoPixel.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <vector>

#include <TaskScheduler.h>
#include <ArduinoJson.h>

#include "triggerCounter.hpp"  // A3144.hpp 대신 새 이름으로 변경

#include "config.hpp"
#include "etc.hpp"

#include <esp_sleep.h> 

Scheduler g_ts;
Config g_config;

// 마지막 활동 시간을 추적하기 위한 전역 변수 및 타임아웃 설정
unsigned long lastActivityTime = 0;
const unsigned long INACTIVITY_SLEEP_DELAY_MS =  10 * 1000UL; // 1분 동안 활동이 없으면 절전 모드로 전환
bool g_isAdvertising = false; // 광고 상태를 직접 추적하는 플래그

extern String ParseCmd(String _strLine);
void handleStateChanges(); // [수정] 상태 변경 처리 함수 선언

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
const int neoPixelPin = 8;      // 네오픽셀 제어 핀 (이전 batStatusPin)

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
const float MIN_BATTERY_VOLTAGE = 1.0;  // 최소 배터리 전압
const float MAX_BATTERY_VOLTAGE = 3.7;  // 최대 배터리 전압

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
    if (pServer) {
        return String(pServer->getPeerMTU(pServer->getConnId()));
    }
    return "0";
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
    }
}

// 탄창 삽입 여부 확인 함수
bool isMagazineInserted() {
    return !digitalRead(magazineInsertedPin);  // LOW일 때 삽입된 상태
}

void resumeFiring() {
    firingEnabled = true;  // 발사 가능 상태로 설정
}

void stopFiring() {
    firingEnabled = false;  // 발사 불가능 상태로 설정
}


// 배터리 레벨 읽기 함수 (0-100%)
int getBatteryLevel() {
    uint32_t Vbatt = 0;
    for(int i = 0; i < 16; i++) {
        Vbatt = Vbatt + analogReadMilliVolts(A0); // ADC with correction   
    }
    float voltage = 2 * Vbatt / 16 / 1000.0;     // attenuation ratio 1/2, mV --> V
    
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

void setupNeoPixel() {
    pixels.begin();
    pixels.clear();
    pixels.show();
    
    // 초기 색상 설정
    updateNeoPixelColor();
}

void offNeoPixel() {
    pixels.clear();
    pixels.show();
}

// 네오픽셀 상태 토글 태스크 (1 초마다 배터리 상태와 연결 상태 번갈아 표시)
Task task_NeoPixelBlink(1000, TASK_FOREVER, []() {
    showBatteryColor = !showBatteryColor;  // 상태 토글
    updateNeoPixelColor();
}, &g_ts, false);

// [수정] taskNotify가 하던 일을 처리하는 함수
void handleStateChanges() {
    static int oldValue = 0;
    static bool oldMagazineInserted = false;
    int _value = TriggerCounter::getTriggerCount();
    
    if (_value != oldValue || oldMagazineInserted != isMagazineInserted()) {
        lastActivityTime = millis(); // 트리거 또는 탄창 삽입 활동 시 시간 리셋
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
}

Task task_Cmd(300, TASK_FOREVER, []() {
    if (Serial.available() > 0) {
        lastActivityTime = millis(); // 시리얼 입력 활동 시 시간 리셋
        String _strLine = Serial.readStringUntil('\n');
        _strLine.trim();
        Serial.println("Received Serial command:");
        Serial.println(_strLine);

        String response = ParseCmd(_strLine);
        Serial.println("Response:");
        Serial.println(response);
    }
}, &g_ts, false);

void printMtuSize(BLEServer *pServer) {
    if (pServer) {
        uint16_t currentMtu = pServer->getPeerMTU(pServer->getConnId());
        Serial.print("current MTU size: ");
        Serial.println(currentMtu);
    }
}

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) {
        lastActivityTime = millis(); // BLE 연결 시 시간 리셋
        deviceConnected = true;
        g_isAdvertising = false; // 연결 시 광고가 중지되므로 플래그 업데이트
        Serial.println("client connected");
        // pServer->getAdvertising()->stop(); // onConnect에서 자동으로 중지됨

        // 환영 메시지 설정 및 알림 전송
        pCharacteristic->setValue("welcome to ESP32 BLE Server");
        
        // 연결 상태 변경되었으므로 네오픽셀 색상 업데이트
        updateNeoPixelColor();

        // [수정] taskNotify.enable() 호출 제거
        printMtuSize(pServer);
    }

    void onDisconnect(BLEServer *pServer) {
        deviceConnected = false;
        Serial.println("client disconnected");
        // 연결 해제 시 loop()에서 광고를 다시 시작하도록 처리
    }

    void onMtuChanged(BLEServer *pServer, esp_ble_gatts_cb_param_t *param) {
        Serial.print("MTU size changed to: ");
        Serial.println(param->mtu.mtu);
    }
};

class MyCharateristicCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();

        if (value.length() > 0) {
            lastActivityTime = millis(); // BLE 쓰기 활동 시 시간 리셋
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
    setupNeoPixel();

    Serial.begin(115200);

    g_config.load();

    Serial.println(":-]");
    Serial.println("Serial connected");

    // 설정에서 디바운스 딜레이 읽기
    uint32_t debounceDelay = g_config.get<uint32_t>("debounceDelay", 50);
    
    // 설정에서 최대 탄약 수 읽기 (없으면 기본값 30)
    maxAmmoCount = g_config.get<int>("maxAmmoCount", 30);
    currentAmmoCount = maxAmmoCount;  // 현재 탄약 수 초기화

    // 트리거 카운터 설정
    TriggerCounter::setup(triggerPin, debounceDelay);
    
    g_ts.startNow();
    // [수정] taskNotify.enable() 호출 제거
    lastActivityTime = millis(); // 마지막 활동 시간 초기화

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
}

// loop 함수 전체 로직 변경
void loop() {
    // [수정] 항상 상태 변화를 감지합니다.
    handleStateChanges();
    // 항상 태스크 스케줄러를 실행합니다.
    g_ts.execute();

    if (deviceConnected) {
        // 연결된 상태이면, 마지막 활동 시간을 계속 갱신하여 절전 모드로 들어가지 않도록 합니다.
        lastActivityTime = millis();
    } else {
        // 연결되지 않은 상태
        // BLE 광고가 실행 중이 아니면 시작합니다.
        if (!g_isAdvertising) {
            Serial.println("Start Advertising...");
            pServer->getAdvertising()->start();
            g_isAdvertising = true;
            task_Cmd.enable();
            task_NeoPixelBlink.enable();
            lastActivityTime = millis(); // 광고 시작 시 활동 시간 리셋
        }

        // 마지막 활동 시간으로부터 설정된 유휴 시간이 지나면 절전 모드로 들어갑니다.
        if (millis() - lastActivityTime > INACTIVITY_SLEEP_DELAY_MS) {
            if (g_isAdvertising) {
                pServer->getAdvertising()->stop();
                g_isAdvertising = false;
            }
            Serial.println("Inactivity timeout. Entering light sleep...");
            Serial.flush(); // 잠들기 전 시리얼 버퍼 비우기
            Serial.end();   // 시리얼 포트를 완전히 비활성화합니다.

            task_Cmd.disable();
            task_NeoPixelBlink.disable();
            offNeoPixel();

            pinMode(neoPixelPin, OUTPUT);
            digitalWrite(neoPixelPin, LOW);
            gpio_hold_en((gpio_num_t)neoPixelPin);

            // actuator 핀을 LOW로 설정하여 액츄에이터 비활성화
            pinMode(actionPin1, OUTPUT);
            digitalWrite(actionPin1, LOW);
            gpio_hold_en((gpio_num_t)actionPin1);


            // 일어나기 위한 GPIO 설정
            gpio_wakeup_enable((gpio_num_t)triggerPin, GPIO_INTR_LOW_LEVEL);
            gpio_wakeup_enable((gpio_num_t)magazineInsertedPin, GPIO_INTR_LOW_LEVEL);
            esp_sleep_enable_gpio_wakeup();

            // Light-sleep 진입
            esp_light_sleep_start();

            // GPIO 핀을 다시 활성화합니다.
            gpio_hold_dis((gpio_num_t)neoPixelPin);
            gpio_hold_dis((gpio_num_t)actionPin1);

            // --- 깨어난 후 처리 ---
            // 시리얼 포트를 다시 초기화하고 안정될 때까지 기다립니다.
            Serial.begin(115200);
            long entry = millis();
            while(!Serial && millis() - entry < 1000) { 
              ; // 1초 동안 기다리거나 시리얼이 연결되면 탈출
            }
            delay(100); // 추가적인 안정화 시간

            // 네오픽셀을 다시 초기화합니다.
            setupNeoPixel();

            //actuator 핀을 다시 설정합니다.
            pinMode(actionPin1, OUTPUT);
            digitalWrite(actionPin1, LOW);

            Serial.println("\n\nWoke up from light sleep.");
            lastActivityTime = millis(); // 깨어난 후 활동 시간 리셋
        }
    }
}
