#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

const uint16_t kIrLed = 4;  // ESP8266 GPIO pin to use. Recommended: 4 (D2).

IRsend irsend(2);  // Set the GPIO to be used to sending the message.

// Example of data captured by IRrecvDumpV2.ino
uint16_t rawData[211] = {9034, 4504,  560, 1694,  560, 1692,  
562, 552,  558, 554,  560, 552,  558, 554,  558, 1696,  556, 1694,  560, 1694,  
560, 1700,  552, 1694,  560, 552,  560, 552,  558, 1694,  558, 554,  560, 1694,  560, 
550,  560, 554,  560, 552,  558, 552,  560, 554,  558, 1694,  560, 1692,  560, 1696,  558, 554,  558, 554,  
558, 556,  558, 554,  558, 552,  560, 552,  560, 552,  560, 554,  560, 552,  560, 554,  560, 552,  560, 552,  
560, 552,  560, 1694,  558, 1692,  560, 552,  560, 552,  560, 552,  560, 552,  560, 552,  560, 550,  560, 552,  
560, 552,  560, 552,  560, 552,  560, 552,  560, 552,  560, 552,  560, 552,  562, 1690,  560, 552,  560, 552,  560, 
552,  560, 552,  560, 552,  560, 552,  560, 552,  562, 550,  562, 550,  562, 550,  560, 552,  560, 552,  560, 552,  
560, 552,  560, 552,  560, 550,  562, 552,  560, 552,  562, 550,  562, 552,  560, 552,  560, 552,  560, 552,  560, 1692,  
562, 550,  560, 554,  560, 552,  560, 550,  564, 550,  562, 550,  560, 552,  562, 552,  560, 552,  560, 552,  560, 1692,  
562, 550,  560, 1694,  562, 550,  562, 552,  560, 550,  562, 550,  562, 550,  562, 1690,  564, 1690,  562, 1692,  560, 1692,  
568, 546,  560, 1694,  560, 1690,  562, 1692,  560};
// Example Samsung A/C state captured from IRrecvDumpV2.ino
uint8_t state[13] = {0xC3, 0xA7, 0xE0, 0x00, 0x60, 0x00, 0x20, 0x00, 0x00, 0x20, 0x00, 0x05, 0xEF};

void setup() {
  irsend.begin();
#if ESP8266
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
#else  // ESP8266
  Serial.begin(115200, SERIAL_8N1);
#endif  // ESP8266
}

void loop() {
  Serial.println("Raw from TestReceiveRaw");
  irsend.sendRaw(rawData, 211, 38);  // Send a raw data capture at 38kHz.  //Raw work )))))bu lol kkkk
  //irsend.sendElectraAC(state); //State worked
  delay(2000);
}
