#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <ArduinoJson.h>

// BLE SECTION
BLEServer *pServer = NULL;
BLECharacteristic *message_characteristic = NULL;
String boxValue = "0";

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define MESSAGE_CHARACTERISTIC_UUID "6d68efe5-04b6-4a85-abc4-c2670b7bf7fd"

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    Serial.println("Connected");
    message_characteristic->setValue("Connected");
    message_characteristic->notify();
  };

  void onDisconnect(BLEServer *pServer) {
    Serial.println("Disconnected");
    pServer->startAdvertising();
  }
};

class CharacteristicsCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();
    if (rxValue.length() > 0) {
      Serial.print("Received Value: ");
      Serial.println(rxValue.c_str());
      
      if (!error) {
        const char* command = doc["command"];
        Serial.print("Command: ");
        Serial.println(command);
        if (command) {
          message_characteristic->setValue("RECEIVED");
          message_characteristic->notify();
          //handleMessage(rxValue.c_str());
        }
      } else {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
      }
    }
  }
};

void setup() {
  Serial.begin(115200);
  BLEDevice::init("ESP32-BLE");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  delay(100);

  message_characteristic = pService->createCharacteristic(
    MESSAGE_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | 
    BLECharacteristic::PROPERTY_WRITE | 
    BLECharacteristic::PROPERTY_NOTIFY | 
    BLECharacteristic::PROPERTY_INDICATE
  );

  message_characteristic->setValue("ESP32");
  message_characteristic->setCallbacks(new CharacteristicsCallbacks());
  pService->start();
  pServer->getAdvertising()->start();

  Serial.println("Waiting for a client connection to notify...");
}

void loop() {
}
