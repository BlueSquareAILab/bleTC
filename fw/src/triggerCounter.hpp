#ifndef TRIGGER_COUNTER_HPP
#define TRIGGER_COUNTER_HPP

#include <Arduino.h>
#include <atomic>

namespace TriggerCounter {
    // 원자적 카운터 변수
    std::atomic<int> triggerCount(0);

    // 트리거 핀과 디바운스 변수
    volatile int triggerPinNumber = -1;
    //volatile unsigned long lastDebounceTime = 0;
    volatile unsigned long lastDebounceTime = 0;
    unsigned long debounceDelay = 0;

    // 첫 번째 입력 후 추가 무시 시간
    unsigned long firstTriggerIgnoreDelay = 50; // 70ms

    // threshold 변수 (두 번째 입력 간 최대 간격)
//    unsigned long thresholdDelay = 500; // 500ms
    unsigned long thresholdDelay = 100; // 100ms

    volatile unsigned long lastTriggerTime = 0;
    volatile bool waitingSecond = false;

    // 인터럽트 처리 함수
    void IRAM_ATTR handleTriggerInterrupt() {
        unsigned long currentTime = millis();
        if ((currentTime - lastDebounceTime) > debounceDelay) {
            if (!waitingSecond) {
                // 첫 번째 입력: 대기 상태로 전환
                waitingSecond = true;
                lastTriggerTime = currentTime;
                lastDebounceTime = currentTime;

            } else {
                unsigned long timeSinceFirst = currentTime - lastTriggerTime;

                if (timeSinceFirst < firstTriggerIgnoreDelay) {
                    // 70ms 이내: 무시
                    return;
                }
                else if ((timeSinceFirst) <= thresholdDelay) {
                    // 두 번째 입력: threshold 이내 -> 카운트 증가
                    triggerCount.fetch_add(1, std::memory_order_relaxed);
                    waitingSecond = false;
                } else {
                    // threshold 초과: 새로운 첫 번째 입력으로 재설정
                    lastTriggerTime = currentTime;
                    waitingSecond = true;
                }
            }
            lastDebounceTime = currentTime;
        }
    }

    // 설정 함수: 핀, 디바운스 지연, threshold 지연 설정
    bool setup(int pin, unsigned long debounce = 0, unsigned long threshold = 100) {
        if (pin < 0) return false;

        triggerPinNumber = pin;
        debounceDelay = debounce;
        thresholdDelay = threshold;
        waitingSecond = false;
        lastDebounceTime = 0;
        lastTriggerTime = 0;

        pinMode(triggerPinNumber, INPUT_PULLUP);

        Serial.println("TriggerCounter setup on pin: " + String(triggerPinNumber));
        Serial.println("Debounce delay: " + String(debounceDelay) + "ms");
        Serial.println("Threshold delay: " + String(thresholdDelay) + "ms");

        attachInterrupt(digitalPinToInterrupt(triggerPinNumber), handleTriggerInterrupt, FALLING);
        return true;
    }

    int getTriggerCount() {
        return triggerCount.load(std::memory_order_relaxed);
    }

    void clearTriggerCount() {
        triggerCount.store(0, std::memory_order_relaxed);
        waitingSecond = false;
        lastTriggerTime = 0;
    }

    bool isInitialized() {
        return triggerPinNumber >= 0;
    }
}

#endif // TRIGGER_COUNTER_HPP
