#include <atomic>  // atomic 헤더 추가

namespace A3144 {
    // const int triggerPin = D8;
    // volatile int triggerCount = 0;
    std::atomic<int> triggerCount(0);  // volatile int triggerCount = 0; 대신 사용

    // 디바운스를 위한 변수들
    volatile unsigned long lastDebounceTime = 0;
    unsigned long debounceDelay;  // 50ms의 디바운스 시간

    // 인터럽트 처리 함수
    void IRAM_ATTR handleTriggerInterrupt() {
        unsigned long currentTime = millis();
        if ((currentTime - lastDebounceTime) > debounceDelay) {
            triggerCount.fetch_add(1, std::memory_order_relaxed);  // atomic 연산으로 변경
            lastDebounceTime = currentTime;
        }
    }

    void setup(int triggerPin,u32_t _debounceDelay = 50) {
        debounceDelay = _debounceDelay;
        pinMode(triggerPin, INPUT_PULLUP);
        // attachInterrupt(digitalPinToInterrupt(triggerPin), handleTriggerInterrupt, FALLING);
        attachInterrupt(digitalPinToInterrupt(triggerPin), handleTriggerInterrupt, CHANGE);  
    }

    int getTriggerCount() {
        return triggerCount.load(std::memory_order_relaxed);
    }

    void clearTriggerCount() {
        triggerCount.store(0, std::memory_order_relaxed);
    }
}
