#include <WiFi.h>
#include <WiFiUdp.h>
#include <IRremoteESP8266.h>
#include <IRac.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <RTClib.h>
#include <EEPROM.h>
#include <Wire.h>

#define UDP_PORT 12345
#define SOCK_PORT 124
#define EEPROM_SIZE 512

#define IR_LED_PIN 15
#ifdef ARDUINO_ESP32C3_DEV
const uint16_t kRecvPin = 10;
#else
const uint16_t kRecvPin = 14;
#endif
#if DECODE_AC
const uint8_t kTimeout = 50;
#else
const uint8_t kTimeout = 15;
#endif

const uint16_t kMinUnknownSize = 12;
const uint16_t kCaptureBufferSize = 1024;
const uint8_t kTolerancePercentage = kTolerance;

WiFiUDP UDP;
IRrecv irrecv(15, kCaptureBufferSize, kTimeout, true);
IRac ac(4);
IRsend irsend(4);

char packet[255];
const char *apiBaseURL = "http://192.168.1.11:3001";

const char *ap_ssid = "ESP32_AP";
const char *ap_password = "12345678";
WebServer server(80);
bool serverRunning = true;

RTC_DS3231 rtc;

struct ScheduledCommand {
  int hour;
  int minute;
  char commandJson[200];  // Ensure this size is enough for your JSON commands
};

ScheduledCommand schedules[2];  // Buffer to hold up to 2 schedules
int currentIndex = 0;

// Function prototypes
void startUdpServer();
void handleMessage(const char *message);
bool callAPI(const char *buttonId, const char *deviceId, uint16_t *rawData, uint16_t length);
void receiveRaw(const char *device_id, const char *button_id);
uint16_t *stringToRawData(String str, int length);
bool getSignalData(const String &baseURL, const String &device_id, const String &button_id, int &length, uint16_t *&rawData);
void sendRaw(const char *device_id, const char *button_id);
bool getSignalDataForVoice(const String &baseURL, int user_id, const String &button_id, int &length, uint16_t *&rawData);
void sendRawForVoice(int user_id, const char *button_id);
String rawDataToString(uint16_t *rawData, uint16_t length);
void setupAPAndConnectToWiFi();
void handlePost();
void checkScheduledCommands();
void writeScheduleToEEPROM(int index, ScheduledCommand &schedule);
void readScheduleFromEEPROM(int index, ScheduledCommand &schedule);

void setup() {
  Serial.begin(115200);
  delay(1000);
  setupAPAndConnectToWiFi();

  Serial.println("");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  UDP.begin(UDP_PORT);
  Serial.print("Listening on UDP port: ");
  Serial.println(UDP_PORT);

#if DECODE_HASH
  irrecv.setUnknownThreshold(kMinUnknownSize);
#endif
  irrecv.setTolerance(kTolerancePercentage);
  irrecv.enableIRIn();
  irsend.begin();

  ac.next.model = 1;                              // Some A/Cs have different models. Try just the first.
  ac.next.mode = stdAc::opmode_t::kCool;          // Run in cool mode initially.
  ac.next.celsius = true;                         // Use Celsius for temp units. False = Fahrenheit
  ac.next.degrees = 27;                           // 25 degrees.
  ac.next.fanspeed = stdAc::fanspeed_t::kMedium;  // Start the fan at medium.
  ac.next.swingv = stdAc::swingv_t::kOff;         // Don't swing the fan up or down.
  ac.next.swingh = stdAc::swingh_t::kOff;         // Don't swing the fan left or right.
  ac.next.light = false;                          // Turn off any LED/Lights/Display that we can.
  ac.next.beep = false;                           // Turn off any beep from the A/C if we can.
  ac.next.econo = false;                          // Turn off any economy modes if we can.
  ac.next.filter = false;                         // Turn off any Ion/Mold/Health filters if we can.
  ac.next.turbo = false;                          // Don't use any turbo/powerful/etc modes.
  ac.next.quiet = false;                          // Don't use any quiet/silent/etc modes.
  ac.next.sleep = -1;                             // Don't set any sleep time or modes.
  ac.next.clean = false;                          // Turn off any Cleaning options if we can.
  ac.next.clock = -1;                             // Don't set any current time if we can avoid it.

  Serial.println("Try to turn on & off every supported A/C type ...");

  Wire.begin(22, 23);  // SDA, SCL
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1)
      ;
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  EEPROM.begin(EEPROM_SIZE);
}

void loop() {
  for (int i = 0; i < 2; i++) {
    readScheduleFromEEPROM(i, schedules[i]);
  }
  //beta way. i need to buy a new battery for clock
  if (rtc.lostPower()) {
    //test
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  checkScheduledCommands();
  int packetSize = UDP.parsePacket();
  if (packetSize) {
    Serial.print("Received packet! Size: ");
    Serial.println(packetSize);

    int len = UDP.read(packet, sizeof(packet) - 1);
    packet[len] = '\0';
    Serial.println(String(packet));
    handleMessage(packet);
  }

  delay(1000);
}

void writeScheduleToEEPROM(int index, ScheduledCommand &schedule) {
  int addr = index * sizeof(ScheduledCommand);
  EEPROM.put(addr, schedule);
  EEPROM.commit();
  Serial.println("Write to EEPROM");
}

void readScheduleFromEEPROM(int index, ScheduledCommand &schedule) {
  int addr = index * sizeof(ScheduledCommand);
  EEPROM.get(addr, schedule);
}

void checkScheduledCommands() {
  DateTime now = rtc.now();
  Serial.print("Current time: ");
  Serial.print(now.hour());
  Serial.print(":");
  Serial.println(now.minute());

  for (int i = 0; i < 2; i++) {
    if (schedules[i].hour == now.hour() && schedules[i].minute == now.minute()) {
      handleMessage(schedules[i].commandJson);
      Serial.println("Do Script");
      // Clear the executed schedule
      schedules[i].hour = -1;
      schedules[i].minute = -1;
      strcpy(schedules[i].commandJson, "");
      writeScheduleToEEPROM(i, schedules[i]);
    }
  }
}

void setupAPAndConnectToWiFi() {
  WiFi.softAP(ap_ssid, ap_password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  server.on("/post", HTTP_POST, handlePost);
  server.begin();

  while (serverRunning) {
    server.handleClient();
    delay(10);
  }
}

void handlePost() {
  if (server.hasArg("plain")) {
    String data = server.arg("plain");
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, data);
    if (!error) {
      String wifiname = doc["wifiname"];
      String password = doc["password"];
      Serial.println("Wifiname: " + wifiname);
      Serial.println("Password: " + password);

      WiFi.begin(wifiname.c_str(), password.c_str());
      int attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
      }

      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());

        DynamicJsonDocument response(200);
        response["status"] = "success";
        response["local_ip"] = WiFi.localIP().toString();
        String responseJson;
        serializeJson(response, responseJson);
        server.send(200, "application/json", responseJson);
        server.stop();
        serverRunning = false;
        return;
      } else {
        server.send(400, "application/json", "{\"status\":\"failure\",\"message\":\"Failed to connect to WiFi\"}");
      }
    } else {
      server.send(400, "application/json", "{\"status\":\"failure\",\"message\":\"Invalid JSON Request\"}");
    }
  } else {
    server.send(500, "application/json", "{\"status\":\"failure\",\"message\":\"No data received\"}");
  }
}

void startUdpServer() {
  UDP.stop();
  UDP.begin(UDP_PORT);
  Serial.print("Listening on UDP port: ");
  Serial.println(UDP_PORT);
}

void handleMessage(const char *message) {
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, message);

  const char *command = doc["command"];
  const char *device_id = doc["device_id"];
  const char *button_id = doc["button_id"];
  int user_id = doc["user_id"];
  int ordinal = doc["ordinal"];
  const char *mode = doc["mode"];
  int hour = doc["hour"];
  int minute = doc["minute"];

  if (hour != 0) {
    // Remove hour and minute from the JSON document
    doc.remove("hour");
    doc.remove("minute");

    // Serialize the updated JSON back to a string
    char updatedMessage[1024];
    serializeJson(doc, updatedMessage, sizeof(updatedMessage));

    // Create a new ScheduledCommand with the updated JSON
    ScheduledCommand newCommand = { hour, minute, "" };
    strcpy(newCommand.commandJson, updatedMessage);
    schedules[currentIndex] = newCommand;
    writeScheduleToEEPROM(currentIndex, newCommand);

    currentIndex = (currentIndex + 1) % 2;  // Circular buffer logic

    Serial.println("Scheduled new command:");
    Serial.println(updatedMessage);
    UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
    UDP.print("DONE-SET-TIME");
    UDP.endPacket();
    return;
  }

  if (strcmp(command, "ESP-ACK") == 0) {
    UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
    UDP.print(WiFi.localIP());
    UDP.endPacket();
  } else if (strcmp(command, "CANCEL") == 0) {
    startUdpServer();
  } else if (strcmp(command, "SEND") == 0) {
    while (true) {
      int codeLen = UDP.read(packet, sizeof(packet) - 1);
      if (codeLen > 0) {
        packet[codeLen] = '\0';
        String hexCode = String(packet);
        Serial.print("Received hex code for ");
        Serial.print(": ");
        Serial.println(hexCode);

        if (hexCode == "CLOSE") {
          UDP.stop();
          break;
        }
        if (strcmp(packet, "ON-AC") == 0) {
          ac.next.power = true;  // We want to turn on the A/C unit.
          Serial.println("Sending a message to turn ON the A/C unit.");
          ac.sendAc();  // Have the IRac class create and send a message.
          delay(5000);
        }
        if (hexCode == "OFF-AC") {
          ac.next.power = false;  // We want to turn on the A/C unit.
          Serial.println("Sending a message to turn ON the A/C unit.");
          ac.sendAc();  // Have the IRac class create and send a message.
          delay(5000);
        }
      }
    }
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
          // ac.next.degrees = 28;
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
  } else if (strcmp(command, "ON-AC") == 0) {
    if (strcmp(mode, "1") == 0) {
      if (user_id) {  // Ensure both values are present
        Serial.println("Received ONAC command.");
        Serial.print("User ID: ");
        Serial.println(user_id);
        sendRawForVoice(user_id, "power", ordinal);
      } else {
        Serial.println("Error: Missing device_id or button_id in LEARN command.");
      }
    } else {
      //ac.next.protocol = decode_type_t::DAIKIN216;
      // ac.next.degrees = 28;
      ac.next.power = true;  // We want to turn on the A/C unit.
      Serial.println("Sending a message to turn ON the A/C unit.");
      ac.sendAc();  // Have the IRac class create and send a message.
    }
    // delay(5000);
  } else if (strcmp(command, "OFF-AC") == 0) {
    if (strcmp(mode, "1") == 0) {
      if (user_id) {  // Ensure both values are present
        Serial.println("Received OFFAC command.");
        Serial.print("User ID: ");
        Serial.println(user_id);
        sendRawForVoice(user_id, "power-off", ordinal);
      } else {
        Serial.println("Error: Missing device_id or button_id in LEARN command.");
      }
    } else {
      //ac.next.protocol = decode_type_t::DAIKIN216;
      // ac.next.degrees = 28;
      ac.next.power = false;  // We want to turn on the A/C unit.
      Serial.println("Sending a message to turn OFF the A/C unit.");
      ac.sendAc();  // Have the IRac class create and send a message.
    }
    // delay(5000);
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
  } else if (strcmp(command, "SEND-LEARN") == 0) {
    if (device_id && button_id) {  // Ensure both values are present
      Serial.println("Received LEARN command.");
      Serial.print("Device ID: ");
      Serial.println(device_id);
      Serial.print("Button ID: ");
      Serial.println(button_id);
      sendRaw(device_id, button_id);
    } else {
      Serial.println("Error: Missing device_id or button_id in LEARN command.");
    }
  } else {
    // Unknown message type, handle appropriately (if needed)
    // ...
  }
}

// Function to convert raw data to a string
String rawDataToString(uint16_t *rawData, uint16_t length) {
  String str = "{";
  for (uint16_t i = 0; i < length; i++) {
    str += String(rawData[i]);
    if (i < length - 1) {
      str += ", ";
    }
  }
  str += "}";
  return str;
}

// Call API to save learn data to database
bool callAPI(const char *buttonId, const char *deviceId, uint16_t *rawData, uint16_t length) {
  // Convert raw data to JSON string
  String str = rawDataToString(rawData, length);

  // Send data to API
  HTTPClient http;
  String route = "/api/signal/learns/new";
  http.begin(apiBaseURL + route);
  http.addHeader("Content-Type", "application/json");

  String payload = "{";
  payload += "\"button_id\":\"" + String(buttonId) + "\",";
  payload += "\"device_id\":\"" + String(deviceId) + "\",";
  payload += "\"rawdata_length\":" + String(length) + ",";
  payload += "\"rawdata\":\"" + str + "\"";
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

// receive raw data from IR receiver and send to API
void receiveRaw(const char *device_id, const char *button_id) {
  static int signalCounter = 0;
  decode_results results;
  if (irrecv.decode(&results)) {
    signalCounter++;
    Serial.println();
    if (signalCounter == 1) {
      // Convert raw data to JSON string
      uint16_t *rawData = resultToRawData(&results);
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

// control device by learned data
void sendRaw(const char *device_id, const char *button_id) {
  int length;
  uint16_t *rawData;
  Serial.println("Start Fetch");
  bool success = getSignalData(apiBaseURL, String(device_id), String(button_id), length, rawData);

  if (success) {
    UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
    UDP.print("SUC-SEND");
    UDP.endPacket();
    Serial.println("Raw from TestReceiveRaw");
    irsend.sendRaw(rawData, length, 38);
    Serial.println("Sent");
  } else {
    UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
    UDP.print("NETWORK-ERR");
    UDP.endPacket();
  }
  delete[] rawData;  // Free memory after sending
}

// Function to convert string to raw data
uint16_t *stringToRawData(String str, int length) {
  uint16_t *rawData = new uint16_t[length];
  char *token = strtok((char *)str.c_str(), ", ");
  for (int i = 0; token != NULL && i < length; i++) {
    rawData[i] = (uint16_t)atoi(token);
    token = strtok(NULL, ", ");
  }
  return rawData;
}

// Call API to get learned data from database
bool getSignalData(const String &baseURL, const String &device_id, const String &button_id, int &length, uint16_t *&rawData) {
  if ((WiFi.status() == WL_CONNECTED)) {  // Check the current connection status
    HTTPClient http;

    String url = baseURL + "/api/signal/learns/getbyid?device_id=" + device_id + "&button_id=" + button_id;
    http.begin(url.c_str());  // Specify the URL
    int httpCode = http.GET();

    // Print the HTTP status code
    Serial.print("HTTP response status code: ");
    Serial.println(httpCode);

    if (httpCode > 0) {  // Check for the returning code
      String payload = http.getString();
      Serial.println(payload);

      // Allocate the JsonDocument
      DynamicJsonDocument doc(4096);

      // Parse the JSON response
      DeserializationError error = deserializeJson(doc, payload);

      // Test if parsing succeeds.
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return false;
      }

      // Get the values from the JSON document
      length = doc["signal"]["rawdata_length"];
      String rawDataString = doc["signal"]["rawdata"];

      // Remove the curly braces from the rawDataString
      rawDataString.remove(0, 1);                        // remove the first character
      rawDataString.remove(rawDataString.length() - 1);  // remove the last character

      // Convert the rawDataString to an array of uint16_t
      rawData = stringToRawData(rawDataString, length);
      return true;
    } else {
      Serial.println("Error on HTTP request");
      return false;
    }

    http.end();  // Free the resources
  }
}

// Control device by learned data (voice mode)
void sendRawForVoice(int user_id, const char *button_id, int ordinal) {
  int length;
  uint16_t *rawData;
  Serial.println("Start Fetch");
  bool success = getSignalDataForVoice(apiBaseURL, user_id, String(button_id), length, rawData, ordinal);

  if (success) {
    UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
    UDP.print("SUC-SEND");
    UDP.endPacket();
    Serial.println("Raw from TestReceiveRaw");
    irsend.sendRaw(rawData, length, 38);
    Serial.println("Sent");
  } else {
    UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
    UDP.print("NETWORK-ERR");
    UDP.endPacket();
  }
  delete[] rawData;  // Free memory after sending
}

// Call API to get learned data from database (voice mode)
bool getSignalDataForVoice(const String &baseURL, int user_id, const String &button_id, int &length, uint16_t *&rawData, int ordinal) {
  if ((WiFi.status() == WL_CONNECTED)) {  // Check the current connection status
    HTTPClient http;

    String url = baseURL + "/api/signal/learns/voice/getbyuserid?user_id=" + user_id + "&type_id=" + 3 + "&button_id=" + button_id + "&ordinal=" + ordinal;
    http.begin(url.c_str());  // Specify the URL
    int httpCode = http.GET();

    // Print the HTTP status code
    Serial.print("HTTP response status code: ");
    Serial.println(httpCode);

    if (httpCode > 0) {  // Check for the returning code
      String payload = http.getString();
      Serial.println(payload);

      // Allocate the JsonDocument
      DynamicJsonDocument doc(4096);

      // Parse the JSON response
      DeserializationError error = deserializeJson(doc, payload);

      // Test if parsing succeeds.
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return false;
      }

      // Get the values from the JSON document
      length = doc["signal"]["rawdata_length"];
      String rawDataString = doc["signal"]["rawdata"];

      // Remove the curly braces from the rawDataString
      rawDataString.remove(0, 1);                        // remove the first character
      rawDataString.remove(rawDataString.length() - 1);  // remove the last character

      // Convert the rawDataString to an array of uint16_t
      rawData = stringToRawData(rawDataString, length);
      return true;
    } else {
      Serial.println("Error on HTTP request");
      return false;
    }

    http.end();  // Free the resources
  }
}