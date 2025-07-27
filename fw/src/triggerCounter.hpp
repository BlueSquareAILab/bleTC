// triggerCounter.hpp
#ifndef TRIGGER_COUNTER_HPP
#define TRIGGER_COUNTER_HPP

#include <Arduino.h>
#include <atomic>

namespace TriggerCounter {
    // 카운트(오직 방향==1일 때만 증가)
    std::atomic<int> triggerCount(0);

    // 핀 번호
    volatile int triggerPinNumber  = -1;
    volatile int magazinePinNumber = -1;

    // 디바운스 타이밍
    volatile unsigned long lastDebounceTrigger  = 0;
    volatile unsigned long lastDebounceMagazin  = 0;
    unsigned long debounceDelay = 1000;

    // 최근 엣지 타입 (0=TRIGGER, 1=MAGAZINE, -1=없음)
    volatile int lastEdge = -1;

    // 마지막으로 계산된 방향 (0 또는 1)
    std::atomic<int> lastDirection(-1);
    // 방향 이벤트 플래그
    std::atomic<bool> directionEvent(false);

    // 공통: 두 엣지가 연달아 들어왔을 때 방향 계산 함수
    inline void IRAM_ATTR processEdge(int thisEdge) {
        if (lastEdge < 0 || lastEdge == thisEdge) {
            // 첫 엣지거나 동일 핀 연속 토글: 상태만 저장
            lastEdge = thisEdge;
        } else {
            // 다른 핀에서 온 두 번째 엣지
            // TRIGGER->MAGAZINE  ⇒ 0
            // MAGAZINE->TRIGGER  ⇒ 1
            int dir = (lastEdge == 1 && thisEdge == 0) ? 1 : 0;
            lastDirection.store(dir, std::memory_order_relaxed);
            directionEvent.store(true, std::memory_order_relaxed);
            if (dir == 0) {
                triggerCount.fetch_add(1, std::memory_order_relaxed);
            }
            lastEdge = thisEdge;
        }
    }

    void IRAM_ATTR handleTriggerInterrupt() {
        unsigned long now = millis();
        if (now - lastDebounceTrigger > debounceDelay) {
            lastDebounceTrigger = now;
            processEdge(0);
        }
    }

    void IRAM_ATTR handleMagazineInterrupt() {
        unsigned long now = millis();
        if (now - lastDebounceMagazin > debounceDelay) {
            lastDebounceMagazin = now;
            processEdge(1);
        }
    }

    // setup: 트리거 핀, 매거진 핀, 디바운스(ms)
    bool setup(int tPin, int mPin, unsigned long delay = 50) {
        if (tPin < 0 || mPin < 0) return false;
        triggerPinNumber  = tPin;
        magazinePinNumber = mPin;
        debounceDelay     = delay;

        pinMode(triggerPinNumber, INPUT_PULLUP);
        pinMode(magazinePinNumber, INPUT_PULLUP);

        attachInterrupt(digitalPinToInterrupt(triggerPinNumber),
                        handleTriggerInterrupt, CHANGE);
        attachInterrupt(digitalPinToInterrupt(magazinePinNumber),
                        handleMagazineInterrupt, CHANGE);

        Serial.printf("TriggerCounter setup on pins: %d, %d (debounce %lums)\n",
                      triggerPinNumber, magazinePinNumber, debounceDelay);
        return true;
    }

    int  getTriggerCount()       { return triggerCount.load(std::memory_order_relaxed); }
    int  getLastDirection()      { return lastDirection.load(std::memory_order_relaxed); }
    bool hasDirectionEvent()     { return directionEvent.load(std::memory_order_relaxed); }
    void clearDirectionEvent()   { directionEvent.store(false, std::memory_order_relaxed); }
    void clearTriggerCount() {
        triggerCount.store(0, std::memory_order_relaxed);
        lastEdge = -1;
        lastDirection.store(-1, std::memory_order_relaxed);
        directionEvent.store(false, std::memory_order_relaxed);
    }
}

#endif // TRIGGER_COUNTER_HPP
