#ifndef TRIGGER_COUNTER_HPP
#define TRIGGER_COUNTER_HPP

#include <Arduino.h>
#include <atomic>

namespace TriggerCounter {
    // 원자적 카운터 변수
    std::atomic<int> triggerCount(0);
    
    // [수정] 첫 번째 엣지(라이징/폴링)를 감지했는지 확인하는 상태 변수
    std::atomic<bool> isFirstEdgeDetected(false);
    
    // 트리거 핀과 디바운스 변수
    volatile int triggerPinNumber = -1;
    volatile unsigned long lastDebounceTime = 0;
    unsigned long debounceDelay = 1000;
    
    // [수정] 엣지 감지 상태를 외부에서 리셋하는 함수
    void resetEdgeState() {
        isFirstEdgeDetected.store(false, std::memory_order_relaxed);
    }

    // 인터럽트 처리 함수
    void IRAM_ATTR handleTriggerInterrupt() {
        unsigned long currentTime = millis();
        if ((currentTime - lastDebounceTime) > debounceDelay) {
            lastDebounceTime = currentTime;
            
            // [수정] 두 번째 엣지에서 카운트하는 로직
            if (isFirstEdgeDetected.load(std::memory_order_relaxed)) {
                // 두 번째 엣지: 카운트하고 상태를 리셋
                triggerCount.fetch_add(1, std::memory_order_relaxed);
                isFirstEdgeDetected.store(false, std::memory_order_relaxed);
            } else {
                // 첫 번째 엣지: 상태만 true로 변경
                isFirstEdgeDetected.store(true, std::memory_order_relaxed);
            }
        }
    }
    
    // 설정 함수
    bool setup(int pin, unsigned long delay = 50) {
        if (pin < 0) return false;
        
        triggerPinNumber = pin;
        debounceDelay = delay;
        
        pinMode(triggerPinNumber, INPUT_PULLUP);
        
        Serial.println("TriggerCounter setup on pin: " + String(triggerPinNumber));
        Serial.println("Debounce delay: " + String(debounceDelay) + "ms");
        
        // 라이징과 폴링 엣지 모두 감지하도록 CHANGE로 설정
        attachInterrupt(digitalPinToInterrupt(triggerPinNumber), handleTriggerInterrupt, CHANGE);
        
        return true;
    }
    
    int getTriggerCount() {
        return triggerCount.load(std::memory_order_relaxed);
    }
    
    void clearTriggerCount() {
        triggerCount.store(0, std::memory_order_relaxed);
        // [수정] 카운트 클리어 시 엣지 상태도 리셋
        resetEdgeState(); 
    }
    
    bool isInitialized() {
        return triggerPinNumber >= 0;
    }
}

#endif // TRIGGER_COUNTER_HPP