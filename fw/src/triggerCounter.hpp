#ifndef TRIGGER_COUNTER_HPP
#define TRIGGER_COUNTER_HPP

#include <Arduino.h>
#include <atomic>

namespace TriggerCounter {
    // 원자적 카운터 변수
    std::atomic<int> triggerCount(0);
    
    // 트리거 핀과 디바운스 변수
    volatile int triggerPinNumber = -1;  // 초기값 -1로 설정하여 미설정 상태 표시
    volatile unsigned long lastDebounceTime = 0;
    unsigned long debounceDelay = 50;
    
    // 인터럽트 처리 함수
    void IRAM_ATTR handleTriggerInterrupt() {
        unsigned long currentTime = millis();
        if ((currentTime - lastDebounceTime) > debounceDelay) {
            triggerCount.fetch_add(1, std::memory_order_relaxed);
            lastDebounceTime = currentTime;
            
            // 디버깅용 코드 (실제 인터럽트 핸들러에서는 Serial 사용 금지)
            // 대신 인터럽트 발생 플래그를 설정하고 main 루프에서 확인하는 방식 사용 필요
        }
    }
    
    // 설정 함수
    bool setup(int pin, unsigned long delay = 50) {
        if (pin < 0) return false;
        
        triggerPinNumber = pin;
        debounceDelay = delay;
        
        pinMode(triggerPinNumber, INPUT_PULLUP);
        
        // 디버그 메시지 출력
        Serial.println("TriggerCounter setup on pin: " + String(triggerPinNumber));
        Serial.println("Debounce delay: " + String(debounceDelay) + "ms");
        
        // 인터럽트 설정 (FALLING: HIGH에서 LOW로 변경 시)
        attachInterrupt(digitalPinToInterrupt(triggerPinNumber), handleTriggerInterrupt, FALLING);
        
        return true;
    }
    
    int getTriggerCount() {
        return triggerCount.load(std::memory_order_relaxed);
    }
    
    void clearTriggerCount() {
        triggerCount.store(0, std::memory_order_relaxed);
    }
    
    // 상태 확인 함수 추가
    bool isInitialized() {
        return triggerPinNumber >= 0;
    }
}

#endif // TRIGGER_COUNTER_HPP