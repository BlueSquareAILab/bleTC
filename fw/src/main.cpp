#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
// #include <BLEDevice.h>
// #include <BLEServer.h>
// #include <BLEUtils.h>
// #include <BLE2902.h>
#include <vector>
#include <TaskScheduler.h>
#include <esp_sleep.h>

#include "triggerCounter.hpp"
#include "config.hpp"
#include "etc.hpp"

Scheduler g_ts;
Config g_config;

// 시스템 상수
constexpr unsigned long INACTIVITY_SLEEP_DELAY_MS = 3 * 60 * 1000UL; // 3분

// 전역 상태 변수
unsigned long lastActivityTime = 0;
bool g_isAdvertising = false;

// GPIO 핀 정의
#if defined(SEED_XIAO_ESP32C3)
    constexpr int ACTION_PIN = D3;
    constexpr int TRIGGER_PIN = D1;
    constexpr int MAGAZINE_PIN = D2;
    constexpr int BATTERY_PIN = A0;
    constexpr int NEOPIXEL_PIN = D10;
    constexpr int LED_BUILTIN_PIN = LED_BUILTIN;
#elif defined(TENSTAR_ESP32C3)
    constexpr int ACTION_PIN = 4;
    constexpr int TRIGGER_PIN = 1;
    constexpr int MAGAZINE_PIN = 3;
    constexpr int NEOPIXEL_PIN = 8;
    constexpr int LED_BUILTIN_PIN = 8;
    constexpr int AMMO_RESET_PIN = 0; // 추가된 핀 정의
#else
    constexpr int ACTION_PIN = 4;
    constexpr int TRIGGER_PIN = 1;
    constexpr int MAGAZINE_PIN = 3;
    constexpr int NEOPIXEL_PIN = 8;
    constexpr int LED_BUILTIN_PIN = 4;
#endif

// 네오픽셀 설정
constexpr int NUM_PIXELS = 1;
Adafruit_NeoPixel pixels(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// BLE 설정
constexpr const char* SERVICE_UUID = "2ca354b0-5f62-11ef-b4d4-f7af9038ee7d";
constexpr const char* CHARACTERISTIC_UUID = "35c34c80-5f62-11ef-b4d4-f7af9038ee7d";

// BLEServer *pServer = NULL;
// BLECharacteristic *pCharacteristic = NULL;
bool deviceConnected = false;

// 게임 상태 변수
struct GameState {
    int maxAmmoCount = 30;
    int currentAmmoCount = 30;
    bool firingEnabled = true;
};

const int pulseDuration = 10000; // 액츄에이터 펄스 지속 시간 (ms)

GameState gameState;

// 함수 선언
extern String ParseCmd(String _strLine);
void handleStateChanges();
void updateActivityTime();

// RTC 메모리에 저장할 데이터 (Deep Sleep 간 유지)
// RTC_DATA_ATTR int rtc_bootCount = 0;
// RTC_DATA_ATTR bool rtc_wasConnected = false;

// Getter 함수들
bool getConnectionStatus() { return deviceConnected; }
String getServiceUUID() { return String(SERVICE_UUID); }
String getCharacteristicUUID() { return String(CHARACTERISTIC_UUID); }
// String getAddress() { return BLEDevice::getAddress().toString().c_str(); }
String getDeviceName() { return "BSQTC_" + getChipID(); }

// String getMtuSize() {
//     if (pServer) {
//         return String(pServer->getPeerMTU(pServer->getConnId()));
//     }
//     return "0";
// }

// 웨이크업 원인 확인
void printWakeupReason() {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    
    switch(wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            Serial.println("Wakeup caused by external signal using RTC_IO");
            break;
        case ESP_SLEEP_WAKEUP_EXT1:
            Serial.println("Wakeup caused by external signal using RTC_CNTL");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            Serial.println("Wakeup caused by timer");
            break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            Serial.println("Wakeup caused by touchpad");
            break;
        case ESP_SLEEP_WAKEUP_ULP:
            Serial.println("Wakeup caused by ULP program");
            break;
        case ESP_SLEEP_WAKEUP_GPIO:  // ESP32-C3 전용
            Serial.println("Wakeup caused by GPIO");
            break;
        default:
            Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
            break;
    }
}

// 게임 로직 함수들
void clearTriggerCount() {
    TriggerCounter::clearTriggerCount();
    gameState.currentAmmoCount = gameState.maxAmmoCount;
    gameState.firingEnabled = true;
    digitalWrite(ACTION_PIN, LOW);
}

int getTriggerCount() {
    return TriggerCounter::getTriggerCount();
}

bool isMagazineInserted() {
    return !digitalRead(MAGAZINE_PIN);
}

int getAmmoLevel() {
    return (float(gameState.currentAmmoCount) / float(gameState.maxAmmoCount)) * 100;
}

void stopFiring() {
    gameState.firingEnabled = false;
}

void resumeFiring() {
    gameState.firingEnabled = true;
}

// 액츄에이터 제어
Task task_EndPulse(TASK_IMMEDIATE, TASK_ONCE, []() {
    digitalWrite(ACTION_PIN, LOW);
}, &g_ts, false);

void doActuatorPulse(int duration = 1000) {
    digitalWrite(ACTION_PIN, HIGH);
    task_EndPulse.restartDelayed(duration);
}

void decreaseAmmoCount() {
    gameState.currentAmmoCount--;
    if (gameState.currentAmmoCount <= 0) {
        gameState.currentAmmoCount = 0;
        gameState.firingEnabled = false;
        doActuatorPulse(pulseDuration); // 액츄에이터 작동
    }
}

// 네오픽셀 제어
void updateNeoPixelColor() {
    int ammoLevel = getAmmoLevel();
    uint32_t color;

    if (ammoLevel <= 0) {
        color = pixels.Color(255, 0, 0);      // 빨간색
    } else if (ammoLevel <= 20) {
        color = pixels.Color(255, 165, 0);    // 주황색
    } else if (ammoLevel <= 50) {
        color = pixels.Color(255, 255, 0);    // 노란색
    } else if (ammoLevel <= 80) {
        color = pixels.Color(173, 255, 47);   // 연두색
    } else {
        color = pixels.Color(0, 255, 0);      // 초록색
    }

    pixels.setPixelColor(0, color);
    pixels.show();
}

void setupNeoPixel() {
    pixels.begin();
    pixels.clear();
    pixels.show();
    updateNeoPixelColor();
}

void offNeoPixel() {
    pixels.clear();
    pixels.show();
}

// 시리얼 명령 처리
Task task_Cmd(300, TASK_FOREVER, []() {
    if (Serial.available() > 0) {
        updateActivityTime();
        String _strLine = Serial.readStringUntil('\n');
        _strLine.trim();
        Serial.println("Received Serial command:");
        Serial.println(_strLine);

        String response = ParseCmd(_strLine);
        Serial.println("Response:");
        Serial.println(response);
    }
}, &g_ts, false);

// 유틸리티 함수들
void updateActivityTime() {
    lastActivityTime = millis();
}

void sleepNow() {
    lastActivityTime = (millis() - INACTIVITY_SLEEP_DELAY_MS) + 3000;
}

void saveCurrentState() {
    g_config.set("currentAmmo", gameState.currentAmmoCount);

    g_config.save();
}

void loadGameState() {
    // gameState.maxAmmoCount = g_config.getInt("maxAmmoCount", 30);
    gameState.maxAmmoCount = 30; // 고정
    gameState.currentAmmoCount = g_config.getInt("currentAmmo", gameState.maxAmmoCount);
    
    if (gameState.currentAmmoCount <= 0) {
        stopFiring();
    }
}

// 상태 변화 처리
void handleStateChanges() {
    static int oldTriggerCount = 0;
    static bool oldMagazineInserted = false;
    
    int currentTriggerCount = TriggerCounter::getTriggerCount();
    bool magazineInserted = isMagazineInserted();

    if (!digitalRead(AMMO_RESET_PIN) && gameState.currentAmmoCount < gameState.maxAmmoCount) {
        gameState.currentAmmoCount = gameState.maxAmmoCount;
        resumeFiring();                                   // gameState.firingEnabled = true
        TriggerCounter::clearTriggerCount();              // 내부 카운터 초기화
        oldTriggerCount = TriggerCounter::getTriggerCount();  // 로컬 oldTriggerCount 동기화
        updateNeoPixelColor();
        Serial.println("Ammo reset triggered. Current ammo count reset to max.");

        // 현재 상태 저장
        saveCurrentState();
    }


    // Serial.println(digitalRead(AMMO_RESET_PIN));

    if (magazineInserted) {
        updateActivityTime();

        //탄수가 0 이고 탄창이 삽입이 일어났다면 액츄에이터 작동
        if (gameState.currentAmmoCount <= 0 && !oldMagazineInserted) {            
            // doActuatorPulse(2000); // 2초 동안 액츄에이터 작동
            doActuatorPulse(pulseDuration); // 액츄에이터 작동
            updateNeoPixelColor();
        }
    }
    else {
        if(oldMagazineInserted) {
            // 탄창이 제거되었지만 이전에 삽입되어 있었던 경우
            saveCurrentState(); // 현재 상태 저장
        }
        
    }



    if (currentTriggerCount != oldTriggerCount || oldMagazineInserted != magazineInserted) {
        updateActivityTime();
        oldMagazineInserted = magazineInserted;
        
        if (oldTriggerCount < currentTriggerCount && gameState.firingEnabled) {
            decreaseAmmoCount();
        }
        
        oldTriggerCount = currentTriggerCount;

        int ammoLevel = getAmmoLevel();
        String data = "#," + String(currentTriggerCount) + "," + String(gameState.currentAmmoCount) + 
            "," + String(gameState.firingEnabled) + "," + String(magazineInserted) + 
            "," + String(ammoLevel) + ",0";
        
        Serial.println(data.c_str());
        updateNeoPixelColor();
    }
    
}

void enterDeepSleep() {
    Serial.println("Inactivity timeout. Entering deep sleep...");
    
    // 현재 상태 저장
    saveCurrentState();
    delay(200);

    // // BLE 완전 정리
    // if (g_isAdvertising) {
    //     pServer->getAdvertising()->stop();
    //     g_isAdvertising = false;
    // }
    
    // if (deviceConnected) {
    //     pServer->disconnect(pServer->getConnId());
    // }
    
    // BLEDevice::deinit(true); // BLE 완전 종료
    
    Serial.flush();
    delay(200);

    // 네오픽셀 완전 비활성화
    offNeoPixel();
    
    // GPIO 핀들을 절전 상태로 고정
    // 네오픽셀 핀을 LOW로 고정 (전력 소모 방지)
    pinMode(NEOPIXEL_PIN, OUTPUT);
    digitalWrite(NEOPIXEL_PIN, LOW);
    gpio_hold_en((gpio_num_t)NEOPIXEL_PIN);
    
    // 액츄에이터 핀을 LOW로 고정 (실수로 동작 방지)
    pinMode(ACTION_PIN, OUTPUT);
    digitalWrite(ACTION_PIN, LOW);
    gpio_hold_en((gpio_num_t)ACTION_PIN);

    
    // 비트마스크 생성: (1ULL << 핀번호)
    uint64_t wakeup_pin_mask = 1ULL << MAGAZINE_PIN;
    
    // GPIO 웨이크업 활성화: 핀이 LOW가 되면 깨어남
    esp_err_t result = esp_deep_sleep_enable_gpio_wakeup(wakeup_pin_mask, ESP_GPIO_WAKEUP_GPIO_LOW);
    
    if (result == ESP_OK) {
        Serial.printf("GPIO wakeup enabled on pin %d (LOW trigger)\n", MAGAZINE_PIN);
    } else {
        Serial.printf("Failed to enable GPIO wakeup: %d\n", result);
        // 실패 시 타이머 웨이크업으로 대체
        esp_sleep_enable_timer_wakeup(60 * 1000000ULL); // 1분 후 체크
        Serial.println("Using timer wakeup as fallback");
    }

    Serial.println("Going to deep sleep now");
    Serial.flush();
    delay(100);

    esp_deep_sleep_start();
    
    // 이 지점은 실행되지 않음 (Deep Sleep 후 재시작됨)
}

void setup() {
    // Deep Sleep 웨이크업 후 GPIO Hold 해제
    gpio_hold_dis((gpio_num_t)NEOPIXEL_PIN);
    gpio_hold_dis((gpio_num_t)ACTION_PIN);
    
    // GPIO 초기화
    pinMode(ACTION_PIN, OUTPUT);
    digitalWrite(ACTION_PIN, LOW);
    pinMode(MAGAZINE_PIN, INPUT_PULLUP);

    pinMode(AMMO_RESET_PIN, INPUT_PULLUP); // 탄창 리셋 핀 초기화

    Serial.begin(115200);
    delay(1000); // 시리얼 안정화
    
    // 부팅 카운트 증가
    // ++rtc_bootCount;
    // Serial.println("Boot number: " + String(rtc_bootCount));
    
    // 웨이크업 원인 출력
    printWakeupReason();

    // 설정 초기화
    g_config.begin();
    g_config.initDefaults();
    
    loadGameState();

    Serial.println(":-]");
    Serial.println("Serial connected");

    // 웨이크업 후 즉시 탄창 상태 확인
    if (isMagazineInserted()) {
        Serial.println("Magazine is inserted - staying awake");
        updateActivityTime();
    } else {
        Serial.println("Magazine not inserted - will sleep soon if no activity");
    }

    // 트리거 카운터 설정
    uint32_t debounceDelay = g_config.getUInt("debounceDelay", 50);
    TriggerCounter::setup(TRIGGER_PIN, debounceDelay);

    Serial.print("maxAmmoCount: ");
    Serial.println(gameState.maxAmmoCount);
    Serial.print("currentAmmoCount: ");
    Serial.println(gameState.currentAmmoCount);

    setupNeoPixel();
    g_ts.startNow();
    updateActivityTime();

    
}

void loop() {
    handleStateChanges();
    g_ts.execute();

    if (deviceConnected) {
        updateActivityTime();
    } else {
        if (!g_isAdvertising) {
            Serial.println("Start Advertising...");
            // pServer->getAdvertising()->start();
            g_isAdvertising = true;
            task_Cmd.enable();
            updateActivityTime();
        }

        // 탄창이 삽입되어 있으면 절전 모드 방지
        if (isMagazineInserted()) {
            updateActivityTime();
        }

        if (millis() - lastActivityTime > INACTIVITY_SLEEP_DELAY_MS) {
            enterDeepSleep(); // Deep Sleep 진입 (재시작됨)
        }
    }
}