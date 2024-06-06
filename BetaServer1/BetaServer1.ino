#include <WiFi.h>
#include <WiFiUdp.h>
#include <IRremoteESP8266.h>
#include <IRac.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define UDP_PORT 12345
#define SSID "Tue Minh Vlog"
#define PASSWD "wifichongtrom"
#define SOCK_PORT 124

//#define IR_RECV_PIN 14 // GPIO5 (D5) for IR receiver
#define IR_LED_PIN 15  // GPIO4 (D2) for IR LED
#ifdef ARDUINO_ESP32C3_DEV
const uint16_t kRecvPin = 10;  // 14 on a ESP32-C3 causes a boot loop.
#else                          // ARDUINO_ESP32C3_DEV
const uint16_t kRecvPin = 14;
#endif                         // ARDUINO_ESP32C3_DEV
#if DECODE_AC
// Some A/C units have gaps in their protocols of ~40ms. e.g. Kelvinator
// A value this large may swallow repeats of some protocols
const uint8_t kTimeout = 50;
#else   // DECODE_AC
// Suits most messages, while not swallowing many repeats.
const uint8_t kTimeout = 15;
#endif  // DECODE_AC

const uint16_t kMinUnknownSize = 12;

const uint16_t kCaptureBufferSize = 1024;

const uint8_t kTolerancePercentage = kTolerance;  // kTolerance is normally 25%

// Legacy (No longer supported!)
//
// Change to `true` if you miss/need the old "Raw Timing[]" display.
#define LEGACY_TIMING_INFO false
// ==================== end of TUNEABLE PARAMETERS ====================

// Use turn on the save buffer feature for more complete capture coverage.
WiFiUDP UDP;
//15 is GPIO15 and D15
IRrecv irrecv(15, kCaptureBufferSize, kTimeout, true);  // Initialize IRrecv without capture buffer size (ESP32 doesn't require it)
IRac ac(2);                                             // Adjust IR LED pin if needed  // Create a A/C object using GPIO to sending messages with.

char packet[255];
char reply[] = "Packet received!";
const char* apiBaseURL = "http://192.168.1.39:3001";  // Replace with your actual API URL
// const char* buttonId = "";
// const char* deviceId = "";

// Function prototypes
void startUdpServer();
void handleMessage(const char* message);
bool callAPI(const char* buttonId, const char* deviceId, uint16_t* rawData, uint16_t length);
void receiveRaw(const char* device_id, const char* button_id);

void setup() {
  Serial.begin(115200);
  delay(1000);
  WiFi.begin(SSID, PASSWD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }

  Serial.println("");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  UDP.begin(UDP_PORT);
  Serial.print("Listening on UDP port: ");
  Serial.println(UDP_PORT);

#if DECODE_HASH
  // Ignore messages with less than minimum on or off pulses.
  irrecv.setUnknownThreshold(kMinUnknownSize);
#endif                                        // DECODE_HASH
  irrecv.setTolerance(kTolerancePercentage);  // Override the default tolerance.
  irrecv.enableIRIn();                        // Start the receiver
  //irsend.begin();

  // Set up what we want to send.
  // See state_t, opmode_t, fanspeed_t, swingv_t, & swingh_t in IRsend.h for
  // all the various options.
  //ac.next.protocol = decode_type_t::COOLIX;      // Set a protocol to use.
  // ac.next.model = 2;                              // Some A/Cs have different models. Try just the first.
  // ac.next.mode = stdAc::opmode_t::kCool;          // Run in cool mode initially.
  // ac.next.celsius = true;                         // Use Celsius for temp units. False = Fahrenheit
  // ac.next.degrees = 28;                           // 25 degrees.
  // ac.next.fanspeed = stdAc::fanspeed_t::kMedium;  // Start the fan at medium.
  // ac.next.swingv = stdAc::swingv_t::kOff;         // Don't swing the fan up or down.
  // ac.next.swingh = stdAc::swingh_t::kOff;         // Don't swing the fan left or right.
  // ac.next.light = false;                          // Turn off any LED/Lights/Display that we can.
  // ac.next.beep = false;                           // Turn off any beep from the A/C if we can.
  // ac.next.econo = false;                          // Turn off any economy modes if we can.
  // ac.next.filter = false;                         // Turn off any Ion/Mold/Health filters if we can.
  // ac.next.turbo = false;                          // Don't use any turbo/powerful/etc modes.
  // ac.next.quiet = false;                          // Don't use any quiet/silent/etc modes.
  // ac.next.sleep = -1;                             // Don't set any sleep time or modes.
  // ac.next.clean = false;                          // Turn off any Cleaning options if we can.
  // ac.next.clock = -1;                             // Don't set any current time if we can avoid it.
  ac.next.power = true;  // Initially start with the unit off.

  Serial.println("Try to turn on & off every supported A/C type ...");
}

void loop() {
  // If packet received...
  int packetSize = UDP.parsePacket();
  if (packetSize) {
    Serial.print("Received packet! Size: ");
    Serial.println(packetSize);

    int len = UDP.read(packet, sizeof(packet) - 1);  // Leave space for null terminator
    packet[len] = '\0';                              // Null-terminate the received data
    Serial.println(String(packet));
    handleMessage(packet);
  };
  // handleMessage("RECEIVE");
  delay(500);
}

void startUdpServer() {
  UDP.stop();
  UDP.begin(UDP_PORT);
  Serial.print("Listening on UDP port: ");
  Serial.println(UDP_PORT);
}

void handleMessage(const char* message) {
  // Parse the received JSON object
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, message);

  // Access the command from the JSON object
  const char* command = doc["command"];
  const char* device_id = doc["device_id"];
  const char* button_id = doc["button_id"];

  if (strcmp(command, "ESP-ACK") == 0) {
    // Close old UDP socket server and begin new
    UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
    UDP.print(WiFi.localIP());
    UDP.endPacket();
  } else if (strcmp(command, "CANCEL") == 0) {
    startUdpServer();  // Close old UDP socket server and begin new
  } else if (strcmp(command, "SEND") == 0) {
    /*int remoteNameLen = UDP.read(packet, 255);
        if (remoteNameLen > 0)
        {
            packet[remoteNameLen] = '\0';
            String remoteName = String(packet);
            Serial.print("Received remote name: ");
            Serial.println(remoteName);*/

    while (true) {
      int codeLen = UDP.read(packet, sizeof(packet) - 1);
      if (codeLen > 0) {
        packet[codeLen] = '\0';
        String hexCode = String(packet);
        Serial.print("Received hex code for ");
        //Serial.print(remoteName);
        Serial.print(": ");
        Serial.println(hexCode);

        if (hexCode == "CLOSE") {
          break;  // Break while loop if "CLOSE" received
        }
        if (strcmp(packet, "ONAC") == 0) {
          ac.next.power = true;  // We want to turn on the A/C unit.
          Serial.println("Sending a message to turn ON the A/C unit.");
          ac.sendAc();  // Have the IRac class create and send a message.
          delay(5000);
        }
        if (hexCode == "OFFAC") {
          ac.next.power = false;  // We want to turn on the A/C unit.
          Serial.println("Sending a message to turn ON the A/C unit.");
          ac.sendAc();  // Have the IRac class create and send a message.
          delay(5000);
        }
      }

      /*if (remoteName == "SONY")
                {
                    unsigned long irCode = strtoul(hexCode.c_str(), NULL, 16);
                    //irsend.sendSony(irCode, hexCode.length() * 4);
                }*/
    }
    //}
  } else if (strcmp(command, "RECEIVE") == 0) {
    String decodedProtocol = "";
    while (decodedProtocol == "" || decodedProtocol == "UNKNOWN") {
      decode_results results;
      if (irrecv.decode(&results)) {
        decodedProtocol = typeToString(results.decode_type);
        Serial.print("Received IR signal, protocol: ");
        Serial.println(decodedProtocol);
        serialPrintUint64(results.value, HEX);
        Serial.println("");

        UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
        UDP.print(decodedProtocol);
        UDP.endPacket();
        irrecv.resume();

        if (ac.isProtocolSupported(results.decode_type)) {
          Serial.println("Protocol " + String(results.decode_type) + " / " + decodedProtocol + " is supported.");
          ac.next.protocol = results.decode_type;  // Change the protocol used.
          delay(100);
        };
      } else {
        // No IR data received yet, wait and try again
        UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
        UDP.print("WAIT");
        UDP.endPacket();
        delay(100);
      };
    }
  } else if (strcmp(command, "ONAC") == 0) {
    ac.next.power = true;  // We want to turn on the A/C unit.
    Serial.println("Sending a message to turn ON the A/C unit.");
    ac.sendAc();  // Have the IRac class create and send a message.
    //delay(5000);
  } else if (strcmp(command, "OFFAC") == 0) {
    ac.next.power = false;  // We want to turn on the A/C unit.
    Serial.println("Sending a message to turn ON the A/C unit.");
    ac.sendAc();  // Have the IRac class create and send a message.
    //delay(5000);
  } else if (strcmp(command, "LEARN") == 0) {
    if (device_id && button_id) {  // Ensure both values are present
      Serial.println("Received LEARN command.");
      Serial.print("Device ID: ");
      Serial.println(device_id);
      Serial.print("Button ID: ");
      Serial.println(button_id);
      receiveRaw(device_id, button_id);
    } else {
      Serial.println("Error: Missing device_id or button_id in LEARN command.");
    }
  } else {
    // Unknown message type, handle appropriately (if needed)
    // ...
  }
}

bool callAPI(const char* buttonId, const char* deviceId, uint16_t* rawData, uint16_t length) {
  // Convert raw data to JSON string
  DynamicJsonDocument doc(1024);  // Adjust capacity if needed
  JsonArray rawDataArray = doc.createNestedArray("rawdata");
  for (uint16_t i = 0; i < length; i++) {
    rawDataArray.add(rawData[i]);
  }
  String rawDataJson;
  serializeJson(doc, rawDataJson);

  // Send data to API
  HTTPClient http;
  String route = "/api/signal/learns/new";
  http.begin(apiBaseURL + route);
  http.addHeader("Content-Type", "application/json");

  String payload = "{";
  payload += "\"button_id\":\"" + String(buttonId) + "\",";
  payload += "\"device_id\":\"" + String(deviceId) + "\",";
  payload += "\"rawdata_length\":" + String(length) + ",";
  payload += "\"rawdata\":" + rawDataJson;
  payload += "}";

  int httpResponseCode = http.POST(payload);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
    return true;
  } else {
    Serial.printf("Error sending POST: %d\n", httpResponseCode);
    return false;
  }
  http.end();
}

void receiveRaw(const char* device_id, const char* button_id) {
  static int signalCounter = 0;
  decode_results results;
  if (irrecv.decode(&results)) {
    signalCounter++;
    Serial.println();
    if (signalCounter == 1) {
      // Convert raw data to JSON string
      uint16_t* rawData = resultToRawData(&results);
      const uint16_t length = getCorrectedRawLength(&results);
      DynamicJsonDocument doc(1024);  // Adjust capacity if needed
      JsonArray rawDataArray = doc.createNestedArray("rawdata");
      for (uint16_t i = 0; i < length; i++) {
        rawDataArray.add(rawData[i]);
      }
      String rawDataJson;
      serializeJson(doc, rawDataJson);

      Serial.printf("rawData[%d] = %s\n", (int)length, rawDataJson.c_str());
      if (ac.isProtocolSupported(results.decode_type)) {
        UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
        UDP.print("PRO");
        UDP.endPacket();
        bool success = callAPI(button_id, device_id, rawData, length);
        if (success) {
          UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
          UDP.print("SUC-PRO");
          UDP.endPacket();
        } else {
          UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
          UDP.print("NETWORK-ERR");
          UDP.endPacket();
        }
      } else if (length > 1000) {
        UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
        UDP.print("LEARN-FAIL");
        UDP.endPacket();
      } else {
        UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
        UDP.print("SUC-NOPRO");
        UDP.endPacket();
        bool success = callAPI(button_id, device_id, rawData, length);
        if (success) {
          UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
          UDP.print("SUC-NOPRO");
          UDP.endPacket();
        } else {
          UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
          UDP.print("NETWORK-ERR");
          UDP.endPacket();
        }
      }

      irrecv.resume();

      delete[] rawData;
      signalCounter = 0;
    }
    Serial.println();
    yield();
  }
}