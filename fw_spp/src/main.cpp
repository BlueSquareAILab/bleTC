#include <Arduino.h>
#include <WiFi.h>
#include <vector>

#include <BluetoothSerial.h>

#include <TaskScheduler.h>
#include <ArduinoJson.h>
#include "config.hpp"

#include "A3144.hpp"

#if defined(LOLIN_D32) | defined(LOLIN_D32_PRO) | defined(WROVER_KIT)
const int ledPins_status = LED_BUILTIN; // D5 is the built-in LED on LOLIN D32
const int triggerPin = 4;

#else 
const int ledPins_status = 2; // D5 is the built-in LED on LOLIN D32
const int triggerPin = D8;

#endif


Scheduler g_ts;
Config g_config;
BluetoothSerial SerialBT;  // BluetoothSerial 객체 선언

extern String parseCmd(String _strLine);


String getChipID() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  String macStr = "";
  for (int i = 0; i < 6; i++) {
    if (mac[i] < 0x10) macStr += "0"; // 한 자리 숫자 앞에 0 추가
    macStr += String(mac[i], HEX);
  }
  macStr.toUpperCase();
  return macStr;
}

Task task_LedBlink(500, TASK_FOREVER, []()
              {
                digitalWrite(ledPins_status, !digitalRead(ledPins_status));
              }, &g_ts, true);


Task task_Cmd(100, TASK_FOREVER, []()
              {

    if (Serial.available() > 0)
    {
        String _strLine = Serial.readStringUntil('\n');
        _strLine.trim();

        Serial.println(parseCmd(_strLine));
    }

    if (SerialBT.available()) {
        String _strLine = SerialBT.readStringUntil('\n');
        _strLine.trim();

        SerialBT.println(parseCmd(_strLine));
    
    }}
    , &g_ts, true);

bool deviceConnected = false;  // Bluetooth 연결 상태 변수

Task task_CheckBTConnection(500, TASK_FOREVER, []() {
  if (SerialBT.hasClient() && !deviceConnected) {

    task_LedBlink.disable();
    Serial.println("Bluetooth device is connected");

#if defined(LOLIN_D32) | defined(LOLIN_D32_PRO) | defined(WROVER_KIT)
    digitalWrite(ledPins_status, LOW); // LOLIN D32의 내장 LED를 끈다.
#else
    digitalWrite(ledPins_status, HIGH); // LOLIN D32의 내장 LED를 끈다.
#endif
    deviceConnected = true;  // Bluetooth가 연결되었을 때 상태 업데이트
  } 
  else if (!SerialBT.hasClient() && deviceConnected) {
    Serial.println("Bluetooth device is disconnected");
    task_LedBlink.enable();
    deviceConnected = false;  // Bluetooth가 연결이 끊겼을 때 상태 업데이트
  }

}, &g_ts, true);


Task taskNotify(20, TASK_FOREVER, []() {

    static int oldValue = 0;
    int _value = A3144::getTriggerCount();
    if (_value != oldValue)
    {
        Serial.println("Magnetic Sensor: " + String(_value));
        oldValue = _value;

        if (deviceConnected) {
          SerialBT.println("d");
            // int currentCount = triggerCount.load(std::memory_order_relaxed);  // atomic 값 읽기
            // String _data = "#," + String(_value) + ",0,0,0,0,0";
            // pCharacteristic->setValue(_data.c_str());
            // pCharacteristic->notify();
        }
    }
    else {
      if(deviceConnected)
        SerialBT.println("c");
    }
}, &g_ts, true); 


void setup() {


  Serial.begin(115200);

  const char *pin = g_config.get<const char *>("password","1111");
  SerialBT.setPin(pin);

  Serial.println("Bluetooth device is ready to pair with PIN code: " + String(pin));
  bool pinSetResult = SerialBT.setPin(pin);
  if (pinSetResult) {
      Serial.println("PIN code set successfully");
  } else {
      Serial.println("Failed to set PIN code");
  }

  // Bluetooth 장치 이름으로 Chip ID 사용
  String deviceName = "BSQ_SPPTC_" + getChipID();
  
  SerialBT.begin(deviceName.c_str());
  // SerialBT.setPin("3117"); // Bluetooth PIN code
  Serial.println("Bluetooth device is ready to pair with name: " + deviceName);  
  Serial.println("Bluetooth device is ready to pair");

  pinMode(ledPins_status, OUTPUT);
  digitalWrite(ledPins_status, HIGH);

  u32_t debounceDelay = g_config.get<u32_t>("debounceDelay",150);
  A3144::setup(triggerPin, debounceDelay);

  


  g_ts.startNow();
}

void loop() {

  g_ts.execute();
  
}
