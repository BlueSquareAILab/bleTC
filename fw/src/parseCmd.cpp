/*
author: gbox3d
date: 2025-03-29

이 주석은 수정하지 마세요.
*/

#include <Arduino.h>
#include "tonkey.hpp"
#include "config.hpp"
#include "etc.hpp"

tonkey g_MainParser;

// 외부 변수 및 함수 참조
extern Config g_config;

// 시스템 정보 함수들
// extern bool getConnectionStatus();
// extern String getServiceUUID();
// extern String getCharacteristicUUID();
// extern String getAddress();
// extern String getMtuSize();
// extern String getDeviceName();

// 트리거 관련 함수들
extern void clearTriggerCount();
extern int getTriggerCount();
extern bool isMagazineInserted();

// 게임 상태 접근을 위한 외부 변수
extern struct GameState {
    int maxAmmoCount;
    int currentAmmoCount;
    bool firingEnabled;
} gameState;

// 게임 제어 함수들
// extern void stopFiring();
// extern void resumeFiring();
extern void doActuatorPulse(int duration = 1000);
extern void sleepNow();
extern void updateNeoPixelColor();
extern void saveCurrentState();

// 간단한 YAML 스타일 응답 생성 헬퍼 함수들
String makeResponse(const String& result, const String& message = "") {
    String response = "result: " + result + "\n";
    if (message.length() > 0) {
        response += "message: " + message + "\n";
    }
    return response;
}

String makeDataResponse(const String& result, const String& data) {
    String response = "result: " + result + "\n";
    response += data;
    return response;
}

String makeValueResponse(const String& result, const String& key, const String& value) {
    String response = "result: " + result + "\n";
    response += key + ": " + value + "\n";
    return response;
}

String makeStatusResponse() {
    String response = "result: ok\n";
    response += "count: " + String(getTriggerCount()) + "\n";
    response += "magazineInserted: " + String(isMagazineInserted() ? "true" : "false") + "\n";
    response += "ammo: " + String(gameState.currentAmmoCount) + "\n";
    response += "maxAmmo: " + String(gameState.maxAmmoCount) + "\n";
    response += "firingEnabled: " + String(gameState.firingEnabled ? "true" : "false") + "\n";
    return response;
}

// String makeBleInfoResponse() {
//     String response = "result: ok\n";
//     response += "name: " + getDeviceName() + "\n";
//     response += "address: " + getAddress() + "\n";
//     response += "serviceUUID: " + getServiceUUID() + "\n";
//     response += "characteristicUUID: " + getCharacteristicUUID() + "\n";
//     response += "mtuSize: " + getMtuSize() + "\n";
//     response += "connection: " + String(getConnectionStatus() ? "true" : "false") + "\n";
//     return response;
// }

String makeAboutResponse() {
    String response = "result: ok\n";
    response += "os: cronos-v1\n";
    response += "app: bleTC_smartMagazine\n";
    response += "version: 1.0.3\n";
    response += "author: gbox3d\n";
    response += "chipid: " + getChipID() + "\n";
    return response;
}

// 명령 파싱 및 처리 함수
String ParseCmd(String _strLine) {
    g_MainParser.parse(_strLine);
    
    if (g_MainParser.getTokenCount() == 0) {
        return makeResponse("fail", "need command");
    }

    String cmd = g_MainParser.getToken(0);

    // === 시스템 정보 명령들 ===
    if (cmd == "about") {
        return makeAboutResponse();
    }
    else if (cmd == "reboot") {
        ESP.restart();
        return makeResponse("ok", "rebooting");
    }
    else if (cmd == "sleep") {
        sleepNow();
        return makeResponse("ok", "entering sleep mode");
    }
    else if (cmd == "save") {
        saveCurrentState();
        // g_config.save();
        
        return makeResponse("ok", "config saved");
    }
    // else if (cmd == "bleinfo") {
    //     return makeBleInfoResponse();
    // }
    else if (cmd == "heap") {
        String response = "result: ok\n";
        response += "heapSize: " + String(ESP.getHeapSize()) + "\n";
        response += "freeHeap: " + String(ESP.getFreeHeap()) + "\n";
        response += "minFreeHeap: " + String(ESP.getMinFreeHeap()) + "\n";
        response += "maxAllocHeap: " + String(ESP.getMaxAllocHeap()) + "\n";
        return response;

    }

    // === 설정 관련 명령들 ===
    else if (cmd == "config") {
        if (g_MainParser.getTokenCount() < 2) {
            return makeResponse("fail", "need sub command");
        }

        String subCmd = g_MainParser.getToken(1);
        
        if (subCmd == "dump") {
            return makeDataResponse("ok", g_config.dump());
        }
        else if (subCmd == "clear") {
            g_config.clear();
            g_config.initDefaults();
            return makeResponse("ok", "config cleared and defaults restored");
        }
        else if (subCmd == "set" && g_MainParser.getTokenCount() > 3) {
            String key = g_MainParser.getToken(2);
            String value = g_MainParser.getToken(3);
            
            // 타입에 따른 설정 저장
            if (key == "debounceDelay") {
                g_config.set(key.c_str(), static_cast<uint32_t>(value.toInt()));
            } else if (key == "maxAmmoCount" || key == "currentAmmo") {
                g_config.set(key.c_str(), static_cast<int>(value.toInt()));
            } else {
                g_config.set(key.c_str(), value);
            }
            
            return makeResponse("ok", "config set: " + key + " = " + value);
        }
        else if (subCmd == "get" && g_MainParser.getTokenCount() > 2) {
            String key = g_MainParser.getToken(2);
            
            if (!g_config.hasKey(key.c_str())) {
                return makeResponse("fail", "key not exist: " + key);
            } else {
                String value;
                if (key == "debounceDelay") {
                    value = String(g_config.getUInt(key.c_str(), 0));
                } else if (key == "maxAmmoCount" || key == "currentAmmo") {
                    value = String(g_config.getInt(key.c_str(), 0));
                } else {
                    value = g_config.getString(key.c_str(), "");
                }
                return makeValueResponse("ok", key, value);
            }
        }
        else {
            return makeResponse("fail", "invalid config command");
        }
    }

    // === 게임 상태 관련 명령들 ===
    else if (cmd == "clear") {
        clearTriggerCount();
        return makeResponse("ok", "trigger count cleared");
    }
    else if (cmd == "status") {
        return makeStatusResponse();
    }
    else if (cmd == "getTriggerCount") {
        return makeValueResponse("ok", "count", String(getTriggerCount()));
    }
    else if (cmd == "getMagazineStatus") {
        return makeValueResponse("ok", "magazineInserted", 
                               String(isMagazineInserted() ? "true" : "false"));
    }

    // === 탄약 관리 명령들 ===
    else if (cmd == "ammo") {
        if (g_MainParser.getTokenCount() < 2) {
            String response = "result: ok\n";
            response += "ammo: " + String(gameState.currentAmmoCount) + "\n";
            response += "maxAmmo: " + String(gameState.maxAmmoCount) + "\n";
            response += "firingEnabled: " + String(gameState.firingEnabled ? "true" : "false") + "\n";
            return response;
        }

        String subCmd = g_MainParser.getToken(1);
        
        if (subCmd == "set" && g_MainParser.getTokenCount() > 2) {
            int newAmmo = g_MainParser.getToken(2).toInt();
            gameState.currentAmmoCount = constrain(newAmmo, 0, gameState.maxAmmoCount);
            
            if (gameState.currentAmmoCount <= 0) {
                // stopFiring();
                doActuatorPulse(1000);
            } else {
                // resumeFiring();
            }
            
            return makeValueResponse("ok", "ammo", String(gameState.currentAmmoCount));
        }
        else if (subCmd == "setmax" && g_MainParser.getTokenCount() > 2) {
            int newMaxAmmo = g_MainParser.getToken(2).toInt();
            gameState.maxAmmoCount = max(1, newMaxAmmo);
            g_config.set("maxAmmoCount", gameState.maxAmmoCount);
            
            return makeValueResponse("ok", "maxAmmo", String(gameState.maxAmmoCount));
        }
        else if (subCmd == "reset") {
            gameState.currentAmmoCount = gameState.maxAmmoCount;
            // resumeFiring();

            updateNeoPixelColor();
            
            return makeValueResponse("ok", "ammo", String(gameState.currentAmmoCount));
        }
        else if (subCmd == "stop") {
            // stopFiring();
            
            String response = "result: ok\n";
            response += "message: firing stopped\n";
            response += "firingEnabled: false\n";
            return response;
        }
        else if (subCmd == "resume") {
            if (gameState.currentAmmoCount > 0) {
                // resumeFiring();
                String response = "result: ok\n";
                response += "message: firing resumed\n";
                response += "firingEnabled: true\n";
                response += "ammo: " + String(gameState.currentAmmoCount) + "\n";
                return response;
            } else {
                String response = "result: fail\n";
                response += "message: cannot resume firing: no ammo\n";
                response += "firingEnabled: false\n";
                response += "ammo: 0\n";
                return response;
            }
        }
        else {
            return makeResponse("fail", "unknown ammo sub command");
        }
    }

    // === 액츄에이터 제어 ===
    else if (cmd == "pulse") {
        if (g_MainParser.getTokenCount() > 1) {
            int duration = g_MainParser.getToken(1).toInt();
            if (duration <= 0) {
                return makeResponse("fail", "duration must be positive");
            } else {
                doActuatorPulse(duration);
                return makeResponse("ok", "pulse sent for " + String(duration) + "ms");
            }
        } else {
            doActuatorPulse(1000);
            return makeResponse("ok", "pulse sent for 1000ms");
        }
    }

    // === BLE 정보 ===
    // else if (cmd == "ble") {
    //     if (g_MainParser.getTokenCount() > 1 && g_MainParser.getToken(1) == "info") {
    //         return makeBleInfoResponse();
    //     } else {
    //         return makeResponse("fail", "need 'info' sub command");
    //     }
    // }

    // === 알 수 없는 명령 ===
    else {
        return makeResponse("fail", "unknown command: " + cmd);
    }
}