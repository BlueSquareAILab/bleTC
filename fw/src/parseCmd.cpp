/*
author: gbox3d
date: 2025-03-29

이 주석은 수정하지 마세요.
*/

#include <Arduino.h>
#include <ArduinoJson.h>
#include "tonkey.hpp"
#include "config.hpp"
#include "etc.hpp"

tonkey g_MainParser;

extern Config g_config;

extern bool getConnectionStatus();
extern String getServiceUUID();
extern String getCharacteristicUUID();
extern String getAddress();
extern String getMtuSize();
extern String getDeviceName();

extern void clearTriggerCount();
extern int getTriggerCount();

extern bool isMagazineInserted();  // magazineInsertedPin 상태 확인 함수 (이전 getModeStatus)
extern int getBatteryLevel();

// 추가 변수 및 함수 선언
extern int maxAmmoCount;       // 최대 탄약 수
extern int currentAmmoCount;   // 현재 탄약 수
extern bool firingEnabled;     // 발사 가능 상태

extern void stopFiring();        // 발사 중지 함수
extern void resumeFiring();      // 발사 재개 함수

String ParseCmd(String _strLine) {
    
    JsonDocument _res_doc;
    g_MainParser.parse(_strLine);
    
    if(g_MainParser.getTokenCount() > 0) {
        String cmd = g_MainParser.getToken(0);
        if (cmd == "about")
        {
            /* code */
            _res_doc["result"] = "ok";
            _res_doc["os"] = "cronos-v1";
            _res_doc["app"] = "bleTC";
            _res_doc["version"] = "1.0.2";
            _res_doc["author"] = "gbox3d";
            _res_doc["chipid"] = getChipID();
            
        }
        else if(cmd == "reboot") {
            ESP.restart();
        }
        else if(cmd == "config") {
            if(g_MainParser.getTokenCount() > 1) {
                String subCmd = g_MainParser.getToken(1);
                if(subCmd == "load") {
                    g_config.load();
                    _res_doc["result"] = "ok";
                    _res_doc["ms"] = "config loaded";
                }
                else if(subCmd == "save") {
                    g_config.save();
                    _res_doc["result"] = "ok";
                    _res_doc["ms"] = "config saved";
                }
                else if(subCmd == "dump") {
                    
                    //parse json g_config.dump()
                    String jsonStr = g_config.dump();
                    DeserializationError error = deserializeJson(_res_doc["cfg"], jsonStr);
                    if (error) {
                        _res_doc["result"] = "fail";
                        _res_doc["ms"] = "json parse error";
                    }
                    else {
                        _res_doc["result"] = "ok";
                    }
                    
                }
                else if(subCmd == "clear") {
                    g_config.clear();
                    _res_doc["result"] = "ok";
                    _res_doc["ms"] = "config cleared";
                }
                else if(subCmd == "set") {
                    if(g_MainParser.getTokenCount() > 2) {
                        String key = g_MainParser.getToken(2);
                        String value = g_MainParser.getToken(3);
                        g_config.set(key.c_str(), value);
                        _res_doc["result"] = "ok";
                        _res_doc["ms"] = "config set";
                    }
                    else {
                        _res_doc["result"] = "fail";
                        _res_doc["ms"] = "need key and value";
                    }
                }
                else if(subCmd == "setA") { //set json array
                    if(g_MainParser.getTokenCount() > 2) {
                        String key = g_MainParser.getToken(2);
                        String value = g_MainParser.getToken(3);
                        //parse json value
                        // JSON 문자열 파싱을 위한 임시 객체
                        JsonDocument tempDoc; // 임시 JSON 문서

                        // JSON 문자열 파싱
                        DeserializationError error = deserializeJson(tempDoc, value);
                        if (error) {
                            _res_doc["result"] = "fail";
                            _res_doc["ms"] = "json parse error";
                        }
                        else {
                            g_config.set(key.c_str(), tempDoc);
                            _res_doc["result"] = "ok";
                            _res_doc["ms"] = tempDoc;
                        }
                    }
                    else {
                        _res_doc["result"] = "fail";
                        _res_doc["ms"] = "need key and value";
                    }
                    
                }
                else if(subCmd == "get") {
                    if(g_MainParser.getTokenCount() > 2) {
                        String key = g_MainParser.getToken(2);

                        //check key exist
                        if(!g_config.hasKey(key.c_str())) {
                            _res_doc["result"] = "fail";
                            _res_doc["ms"] = "key not exist";
                        }
                        else {
                            _res_doc["result"] = "ok";
                            _res_doc["value"] = g_config.get<String>(key.c_str());
                        }
                    }
                    else {
                        _res_doc["result"] = "fail";
                        _res_doc["ms"] = "need key";
                    }
                }
                else {
                    _res_doc["result"] = "fail";
                    _res_doc["ms"] = "unknown sub command";
                
                }
            }
            else {
                _res_doc["result"] = "fail";
                _res_doc["ms"] = "need sub command";
            }
        }
        else if(cmd == "clear") {
            clearTriggerCount();
            _res_doc["result"] = "ok";
            _res_doc["ms"] = "trigger count cleared";
        }
        else if(cmd == "status") {
            _res_doc["result"] = "ok";
            _res_doc["count"] = getTriggerCount();
            _res_doc["magazineInserted"] = isMagazineInserted();  // 이름 변경
            _res_doc["battery"] = getBatteryLevel();
            _res_doc["ammo"] = currentAmmoCount;  // 현재 탄약 수 추가
            _res_doc["maxAmmo"] = maxAmmoCount;   // 최대 탄약 수 추가
            _res_doc["firingEnabled"] = firingEnabled;  // 발사 가능 상태 추가
        }
        else if(cmd == "ammo") {
            if(g_MainParser.getTokenCount() > 1) {
                String subCmd = g_MainParser.getToken(1);
                if(subCmd == "set") {
                    if(g_MainParser.getTokenCount() > 2) {
                        int newAmmo = g_MainParser.getToken(2).toInt();
                        currentAmmoCount = constrain(newAmmo, 0, maxAmmoCount);
                        
                        // 탄약이 0이면 발사 중지
                        if(currentAmmoCount <= 0) {
                            firingEnabled = false;
                            digitalWrite(D9, HIGH);  // 액츄에이터 활성화
                        } else {
                            firingEnabled = true;
                            digitalWrite(D9, LOW);   // 액츄에이터 비활성화
                        }
                        
                        _res_doc["result"] = "ok";
                        _res_doc["ammo"] = currentAmmoCount;
                    } else {
                        _res_doc["result"] = "fail";
                        _res_doc["ms"] = "need ammo count";
                    }
                }
                else if(subCmd == "setmax") {
                    if(g_MainParser.getTokenCount() > 2) {
                        int newMaxAmmo = g_MainParser.getToken(2).toInt();
                        maxAmmoCount = max(1, newMaxAmmo);  // 최소 1발은 설정
                        g_config.set("maxAmmoCount", maxAmmoCount);  // 설정에 저장
                        
                        _res_doc["result"] = "ok";
                        _res_doc["maxAmmo"] = maxAmmoCount;
                    } else {
                        _res_doc["result"] = "fail";
                        _res_doc["ms"] = "need max ammo count";
                    }
                }
                else if(subCmd == "reset") {
                    currentAmmoCount = maxAmmoCount;
                    // firingEnabled = true;
                    // digitalWrite(D9, LOW);  // 액츄에이터 비활성화
                    resumeFiring();  // 발사 재개
                    
                    _res_doc["result"] = "ok";
                    _res_doc["ammo"] = currentAmmoCount;
                }
                else if(subCmd == "stop") {
                    // 발사 중지 명령 추가
                    // firingEnabled = false;
                    // digitalWrite(D9, HIGH);  // 액츄에이터 활성화
                    stopFiring();  // 발사 중지
                    
                    _res_doc["result"] = "ok";
                    _res_doc["ms"] = "firing stopped";
                    _res_doc["firingEnabled"] = firingEnabled;
                }
                else if(subCmd == "resume") {
                    // 발사 재개 명령 추가 (단, 탄약이 0이면 재개 불가)
                    if(currentAmmoCount > 0) {
                        // firingEnabled = true;
                        // digitalWrite(D9, LOW);  // 액츄에이터 비활성화
                        resumeFiring();  // 발사 재개
                        
                        _res_doc["result"] = "ok";
                        _res_doc["ms"] = "firing resumed";
                    } else {
                        _res_doc["result"] = "fail";
                        _res_doc["ms"] = "cannot resume firing: no ammo";
                    }
                    _res_doc["firingEnabled"] = firingEnabled;
                    _res_doc["ammo"] = currentAmmoCount;
                }
                else {
                    _res_doc["result"] = "fail";
                    _res_doc["ms"] = "unknown sub command";
                }
            } else {
                _res_doc["result"] = "ok";
                _res_doc["ammo"] = currentAmmoCount;
                _res_doc["maxAmmo"] = maxAmmoCount;
                _res_doc["firingEnabled"] = firingEnabled;
            }
        }
        else if(cmd == "ble") {
            // BLE
            if(g_MainParser.getTokenCount() > 1) {
                String subCmd = g_MainParser.getToken(1);
                if(subCmd == "info") {
                    _res_doc["result"] = "ok";
                    _res_doc["name"] = getDeviceName();
                    _res_doc["address"] = getAddress();
                    _res_doc["serviceUUID"] = getServiceUUID();
                    _res_doc["characteristicUUID"] = getCharacteristicUUID();
                    _res_doc["mtuSize"] = getMtuSize();
                    _res_doc["connection"] = getConnectionStatus();
                }
                else {
                    _res_doc["result"] = "fail";
                    _res_doc["ms"] = "unknown sub command";
                }
            }
            else {
                _res_doc["result"] = "fail";
                _res_doc["ms"] = "need sub command";
            }
        }
        else {
            _res_doc["result"] = "fail";
            _res_doc["ms"] = "unknown command";
        }
    }
    else {
        _res_doc["result"] = "fail";
        _res_doc["ms"] = "need command";
    }

    return _res_doc.as<String>();
}