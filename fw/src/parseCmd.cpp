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

extern bool getModeStatus();
extern int getBatteryLevel();

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
            _res_doc["version"] = "1.0.1";
            _res_doc["author"] = "gbox3d";
            _res_doc["chipid"] = getChipID();
            
        }
        else if(cmd == "reboot") {
            ESP.restart();
        }
        // else if(cmd == "status") {
        //     _res_doc["result"] = "ok";
        //     _res_doc["mode"] = getModeStatus();
        //     _res_doc["battery"] = getBatteryLevel();
        // }
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
                        // Serial.print(F("deserializeJson() failed: "));
                        // Serial.println(error.f_str());
                        // return;
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
                        // DeserializationError error = deserializeJson(g_config[key.c_str()], value);
                        if (error) {
                            // Serial.print(F("deserializeJson() failed: "));
                            // Serial.println(error.f_str());
                            // return;
                            _res_doc["result"] = "fail";
                            _res_doc["ms"] = "json parse error";
                        }
                        else {
                            // JsonArray array = tempDoc[key].as<JsonArray>();

                            g_config.set(key.c_str(), tempDoc);
                            _res_doc["result"] = "ok";
                            _res_doc["ms"] = tempDoc;
                        }
                        // g_config.set(key.c_str(), value);
                        // _res_doc["result"] = "ok";
                        // _res_doc["ms"] = "config set";
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
                            // serializeJson(_res_doc, Serial);
                            // Serial.println();
                            // return;
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
            _res_doc["mode"] = getModeStatus();
            _res_doc["battery"] = getBatteryLevel();
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
        // serializeJson(g_res_doc, Serial);
        // Serial.println();
        
    }
    else {
        _res_doc["result"] = "fail";
        _res_doc["ms"] = "need command";
    }

    return _res_doc.as<String>();
}


