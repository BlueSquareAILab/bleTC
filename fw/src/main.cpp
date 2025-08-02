#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <vector>
#include <TaskScheduler.h>
#include <esp_sleep.h>

#include "triggerCounter.hpp"
#include "config.hpp"
#include "etc.hpp"

Scheduler g_ts;
Config g_config;

// 시스템 상수
constexpr unsigned long INACTIVITY_SLEEP_DELAY_MS = 10 * 60 * 1000UL; // 10분

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

// --- [추가] 네오픽셀 깜박임 설정 ---
constexpr unsigned long NEOPIXEL_ON_DURATION_MS = 50;  // 0.05초 ON
constexpr unsigned long NEOPIXEL_OFF_DURATION_MS = 1000; // 1초 OFF
bool g_isNeoPixelOn = false; // 네오픽셀 현재 ON/OFF 상태

// 게임 상태 변수
struct GameState {
    
    // 20250731 탄약 맥스 및 현재 탄 수 설정 (32 > 30)
    int maxAmmoCount = 30;
    int currentAmmoCount = 30;
    // bool firingEnabled = true;
};

const int pulseDuration = 5000; // 액츄에이터 펄스 지속 시간 (ms)

GameState gameState;

// 함수 선언
extern String ParseCmd(String _strLine);
void handleStateChanges();
void updateActivityTime();
void updateNeoPixelColor(); // 함수 선언 위치 변경 또는 추가

// --- [추가] 네오픽셀 깜박임 콜백 함수 선언 ---
void blinkNeoPixelCallback(); 

// --- [추가] 네오픽셀 깜박임 제어 Task ---
Task task_BlinkNeoPixel(TASK_IMMEDIATE, TASK_FOREVER, &blinkNeoPixelCallback, &g_ts, false); // 처음에는 비활성화 상태로 시작


// RTC 메모리에 저장할 데이터 (Deep Sleep 간 유지)
// RTC_DATA_ATTR int rtc_bootCount = 0;
// RTC_DATA_ATTR bool rtc_wasConnected = false;

// Getter 함수들
String getDeviceName() { return "BSQTC_" + getChipID(); }

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
    // gameState.firingEnabled = true;
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

// void stopFiring() {
//     gameState.firingEnabled = false;
// }

// void resumeFiring() {
//     gameState.firingEnabled = true;
// }

// 액츄에이터 제어
Task task_EndPulse(TASK_IMMEDIATE, TASK_ONCE, []() {
    digitalWrite(ACTION_PIN, LOW);
}, &g_ts, false);

void doActuatorPulse(int duration = 1000) {
    digitalWrite(ACTION_PIN, HIGH);
    task_EndPulse.restartDelayed(duration);
}

void decreaseAmmoCount(int decValue) {
    gameState.currentAmmoCount -= decValue;
    if (gameState.currentAmmoCount <= 0) {
        gameState.currentAmmoCount = 0;
        // gameState.firingEnabled = false;
        doActuatorPulse(pulseDuration); // 액츄에이터 작동
    }
}

// 네오픽셀 제어
void offNeoPixel() {
    pixels.clear();
    pixels.show();
    g_isNeoPixelOn = false; // [수정] 상태 변수 업데이트
}

// --- [수정] 네오픽셀 색상 업데이트 및 깜박임 리셋 함수 ---
void updateNeoPixelColor() {
    int ammoLevel = getAmmoLevel();
    uint32_t color;

    if (ammoLevel <= 0) {
        color = pixels.Color(255, 0, 0);       // 빨간색
    } else if (ammoLevel <= 20) {
        color = pixels.Color(255, 165, 0);     // 주황색
    } else if (ammoLevel <= 50) {
        color = pixels.Color(255, 255, 0);     // 노란색
    } else if (ammoLevel <= 80) {
        color = pixels.Color(173, 255, 47);    // 연두색
    } else {
        color = pixels.Color(0, 255, 0);       // 초록색
    }

    pixels.setPixelColor(0, color);
    pixels.show();

    // LED를 즉시 켜고, ON 상태 지속 시간 후 꺼지도록 태스크를 재시작
    g_isNeoPixelOn = true;
    task_BlinkNeoPixel.restartDelayed(NEOPIXEL_ON_DURATION_MS);
}

// --- [추가] 네오픽셀 깜박임을 위한 콜백 함수 ---
void blinkNeoPixelCallback() {
    if (g_isNeoPixelOn) {
        // 현재 ON 상태 -> OFF로 변경하고, OFF 지속 시간 후에 다시 태스크 실행
        offNeoPixel();
        task_BlinkNeoPixel.delay(NEOPIXEL_OFF_DURATION_MS);
    } else {
        // 현재 OFF 상태 -> ON으로 변경 (이때 색상을 다시 계산)
        updateNeoPixelColor(); // 이 함수가 다시 task_BlinkNeoPixel.restartDelayed를 호출하여 사이클이 이어짐
    }
}

void setupNeoPixel() {
    pixels.begin();
    pixels.clear();
    pixels.show();
    // updateNeoPixelColor(); // [수정] 직접 호출 대신 태스크 활성화
    task_BlinkNeoPixel.enable(); // [수정] 부팅 시 깜박임 시작
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
    // gameState.maxAmmoCount = 32; // 고정
    gameState.currentAmmoCount = g_config.getInt("currentAmmo", gameState.maxAmmoCount);
    
    if (gameState.currentAmmoCount <= 0) {
        // stopFiring();
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
        TriggerCounter::clearTriggerCount();
        
        updateNeoPixelColor(); // [기존 로직 유지] 색상 즉시 업데이트 및 깜박임 재시작
        Serial.println("Ammo reset triggered. Current ammo count reset to max.");

        saveCurrentState();
    }

    if( !magazineInserted && oldMagazineInserted) {
        // 탄창이 제거되었을 때
        Serial.println("Magazine removed.");
        
        updateNeoPixelColor(); // [기존 로직 유지] 색상 즉시 업데이트 및 깜박임 재시작
    }
    // 탄창이 삽입되었을 때
    else if (magazineInserted && !oldMagazineInserted) {
        Serial.println("Magazine inserted. ");
        
        if( gameState.currentAmmoCount <= 0) {
            doActuatorPulse(pulseDuration);
        }
        updateNeoPixelColor(); // [기존 로직 유지] 색상 즉시 업데이트 및 깜박임 재시작
    }



    if (currentTriggerCount != oldTriggerCount) {
        updateActivityTime();
        if (oldTriggerCount < currentTriggerCount) {
            decreaseAmmoCount(currentTriggerCount - oldTriggerCount);
        }
        
        String data = "#," + String(currentTriggerCount) + "," + String(gameState.currentAmmoCount) + 
            "," + String(magazineInserted);
        
        Serial.println(data.c_str());
        updateNeoPixelColor(); // [기존 로직 유지] 색상 즉시 업데이트 및 깜박임 재시작
    }

    oldMagazineInserted = magazineInserted;
    oldTriggerCount = currentTriggerCount;
}

void enterDeepSleep() {
    Serial.println("Inactivity timeout. Entering deep sleep...");
    
    // [추가] Deep Sleep 진입 전 태스크 비활성화 및 LED 끄기
    task_BlinkNeoPixel.disable();
    offNeoPixel();

    saveCurrentState();
    delay(200);

    Serial.flush();
    delay(200);
    
    pinMode(NEOPIXEL_PIN, OUTPUT);
    digitalWrite(NEOPIXEL_PIN, LOW);
    gpio_hold_en((gpio_num_t)NEOPIXEL_PIN);
    
    pinMode(ACTION_PIN, OUTPUT);
    digitalWrite(ACTION_PIN, LOW);
    gpio_hold_en((gpio_num_t)ACTION_PIN);

    uint64_t wakeup_pin_mask = 1ULL << MAGAZINE_PIN;
    
    esp_err_t result = esp_deep_sleep_enable_gpio_wakeup(wakeup_pin_mask, ESP_GPIO_WAKEUP_GPIO_LOW);
    
    if (result == ESP_OK) {
        Serial.printf("GPIO wakeup enabled on pin %d (LOW trigger)\n", MAGAZINE_PIN);
    } else {
        Serial.printf("Failed to enable GPIO wakeup: %d\n", result);
        esp_sleep_enable_timer_wakeup(60 * 1000000ULL); // 1분 후 체크
        Serial.println("Using timer wakeup as fallback");
    }

    Serial.println("Going to deep sleep now");
    Serial.flush();
    delay(100);

    esp_deep_sleep_start();
}

void setup() {
    gpio_hold_dis((gpio_num_t)NEOPIXEL_PIN);
    gpio_hold_dis((gpio_num_t)ACTION_PIN);
    
    pinMode(ACTION_PIN, OUTPUT);
    digitalWrite(ACTION_PIN, LOW);
    pinMode(MAGAZINE_PIN, INPUT_PULLUP);

    pinMode(AMMO_RESET_PIN, INPUT_PULLUP);

    Serial.begin(115200);
    delay(1000);
    
    printWakeupReason();

    g_config.begin();
    g_config.initDefaults();
    
    loadGameState();

    Serial.println(":-]");
    Serial.println("Serial connected");

    Serial.println("App version: 1.0.1");

    if (isMagazineInserted()) {
        Serial.println("Magazine is inserted - staying awake");
        updateActivityTime();
    } else {
        Serial.println("Magazine not inserted - will sleep soon if no activity");
    }

    // uint32_t debounceDelay = g_config.getUInt("debounceDelay", 50);
    TriggerCounter::setup(TRIGGER_PIN, 15, 500);

    Serial.print("maxAmmoCount: ");
    Serial.println(gameState.maxAmmoCount);
    Serial.print("currentAmmoCount: ");
    Serial.println(gameState.currentAmmoCount);

    setupNeoPixel(); // 여기에서 깜박임 Task가 활성화됩니다.
    g_ts.startNow();
    updateActivityTime();
}

void loop() {
    handleStateChanges();
    g_ts.execute();

    {
        if (!g_isAdvertising) {
            Serial.println("Start Advertising...");
            // pServer->getAdvertising()->start();
            g_isAdvertising = true;
            task_Cmd.enable();
            updateActivityTime();
        }

        if (isMagazineInserted()) {
            updateActivityTime();
        }

        if (millis() - lastActivityTime > INACTIVITY_SLEEP_DELAY_MS) {
            enterDeepSleep();
        }
    }
}