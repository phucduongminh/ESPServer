#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <IRremoteESP8266.h>
#include <IRac.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <RTClib.h>
#include <EEPROM.h>
#include <Wire.h>
#include <PubSubClient.h>

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
IRrecv irrecv(34, kCaptureBufferSize, kTimeout, true);
IRac ac(23);
IRsend irsend(19);

char packet[255];
const char *apiBaseURL = "http://192.168.1.8:3001";

//const char *ap_ssid = "ESP32_AP";
//const char *ap_password = "12345678";
WebServer server(80);
bool serverRunning = true;

RTC_DS3231 rtc;

struct ScheduledCommand {
  int hour;
  int minute;
  char commandJson[200];  // Ensure this size is enough for your JSON commands
};

ScheduledCommand schedules[2];  // Buffer to hold up to 2 schedules
uint8_t currentIndex = 0;
// MQTT Constants
//const char *mqtt_server = "broker.emqx.io";
//const int mqtt_port = 1883;
//const char *mqtt_username = "haitacdc00";
//const char *mqtt_password = "SiucapvipprO#10";

// Button Pin Definitions
//const int button1Pin = 18;  // Adjust these pins to match your actual wiring
//const int button2Pin = 5;

// Communication Mode 1.UDP 2.MQTT
int connectionMode = 1;

WiFiClient espClient;
PubSubClient mqttClient(espClient);  // Create MQTT client

// Function prototypes
void startUdpServer();
void handleMessage(const char *message);
bool callLearnAPI(const char *buttonId, const char *deviceId, uint16_t *rawData, uint16_t length);
bool callLearnAPIWithProtocol(const char *deviceId, const char *Protocol);
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
void publishMessage(const char *topic, String payload, boolean retained);
void reconnectMQTT();
void mqttCallback(char *topic, byte *payload, unsigned int length);

void setup() {
  Serial.begin(115200);
  delay(1000);
  setupAPAndConnectToWiFi();
  // WiFi.begin("VNPT-NGOCANH", "tueminhvolg");
  // int attempts = 0;
  // while (WiFi.status() != WL_CONNECTED && attempts < 20) {
  //   delay(500);
  //   Serial.print(".");
  //   attempts++;
  // }
  // Serial.println();

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
  //irrecv.enableIRIn();
  irsend.begin();

  ac.next.model = 1;                              // Some A/Cs have different models. Try just the first.
  ac.next.mode = stdAc::opmode_t::kCool;          // Run in cool mode initially.
  ac.next.celsius = true;                         // Use Celsius for temp units. False = Fahrenheit
  //ac.next.degrees = 27;                           // 25 degrees.
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

  pinMode(18, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);

  mqttClient.setServer("broker.emqx.io", 1883);
  mqttClient.setCallback(mqttCallback);

  Wire.begin(21, 22);  // SDA, SCL
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
  // Button Handling
  int button1State = digitalRead(18);
  int button2State = digitalRead(5);
  // for (int i = 0; i < 2; i++) {
  //   readScheduleFromEEPROM(i, schedules[i]);
  // }
  //beta way. i need to buy a new battery for clock
  // if (rtc.lostPower()) {
  //   //test
  //   rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  // }
  checkScheduledCommands();
  if (button1State == LOW) {
    connectionMode = 1;  // Switch to UDP mode
    mqttClient.disconnect();
    startUdpServer();
    Serial.println("UDP mode");
  } else if (button2State == LOW) {
    connectionMode = 2;  // Switch to MQTT mode
    UDP.stop();
    Serial.println("MQTT mode");
  }

  if (connectionMode == 2) {
    if (!mqttClient.connected()) {
      reconnectMQTT();
    }
    mqttClient.loop();
  } else {
    int packetSize = UDP.parsePacket();
    if (packetSize) {
      Serial.print("Received packet! Size: ");
      Serial.println(packetSize);

      int len = UDP.read(packet, sizeof(packet) - 1);
      packet[len] = '\0';
      Serial.println(String(packet));
      handleMessage(packet);
    }
  }

  delay(1000);
}

// MQTT Reconnect
void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect("ESP32-01", "haitacdc00", "SiucapvipprO#10")) {
      Serial.println("Connected");
      mqttClient.subscribe("esp32/request");  // Topic to listen for commands
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.write(payload, length);
  Serial.println();

  // 1. Create a Temporary Buffer
  char messageBuffer[length + 1];  // +1 for null terminator

  // 2. Copy Payload to the Buffer
  memcpy(messageBuffer, payload, length);

  // 3. Add Null Terminator
  messageBuffer[length] = '\0';

  DynamicJsonDocument doc(1024);  // Adjust size if needed
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  }

  // Check for client_id and command fields
  if (!doc["client_id"] || !doc["command"] || strcmp(doc["client_id"], "ESP32-01") == 0) {
    Serial.println("Missing field or wrong reveiced");
    return;
  }

  const char *command = doc["command"];
  if (command) {
    Serial.print("Command received: ");
    Serial.println(command);

    // Acknowledge command receipt
    //publishMessage("esp32/response", "{\"client_id\":\"ESP32-01\",\"message\":\"RECEIVE\"}", false);
    //Pass the Buffer to handleMessage
    handleMessage(messageBuffer);
  }
}

void publishMessage(const char *topic, String payload, boolean retained) {
  if (mqttClient.publish(topic, payload.c_str(), retained))
    Serial.println("Message Responsed!");
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
  // Serial.print("Current time: ");
  // Serial.print(now.hour());
  // Serial.print(":");
  // Serial.println(now.minute());

  for (int i = 0; i < 2; i++) {
    if (schedules[i].hour == now.hour() && schedules[i].minute == now.minute()) {
      Serial.print("JSON Length: ");
      Serial.println(strlen(schedules[i].commandJson));
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
  WiFi.softAP("ESP32_AP", "12345678");
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
      Serial.println();

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
  if (doc["Protocol"]){
    const char *Protocol = doc["Protocol"];
    ac.next.protocol = stringToType(Protocol);
  }
  int degree;
  if (doc["degree"]) {
    degree = doc["degree"];
  }

  if (doc["hour"]) {
    // Remove hour and minute from the JSON document
    int hour = doc["hour"];
    int minute;
    doc.remove("hour");
    if (doc["minute"]) {
      minute = doc["minute"];
      doc.remove("minute");
    } else {
      minute = 0;
    }

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

    if (connectionMode == 1) {
      UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
      UDP.print("DONE-SET-TIME");
      UDP.endPacket();
    } else {
      publishMessage("esp32/response", "{\"client_id\":\"ESP32-01\",\"message\":\"DONE-SET-TIME\"}", false);
    }
    return;
  }

  if (strcmp(command, "ESP-ACK") == 0) {
    UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
    UDP.print(WiFi.localIP());
    UDP.endPacket();
  } else if (strcmp(command, "CANCEL") == 0) {
    startUdpServer();
  } else if (strcmp(command, "RECEIVE") == 0) {
    irrecv.enableIRIn();
    delay(500);
    String decodedProtocol = "";
    int unknownCount = 0;
    while (decodedProtocol == "" || decodedProtocol == "UNKNOWN") {
      decode_results results;
      if (irrecv.decode(&results)) {
        irrecv.pause();
        decodedProtocol = typeToString(results.decode_type);
        Serial.print("Received IR signal, protocol: ");
        Serial.println(decodedProtocol);
        serialPrintUint64(results.value, HEX);
        Serial.println("");
        if (connectionMode == 1) {
          UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
          UDP.print(decodedProtocol);
          UDP.endPacket();
        } else {
          publishMessage("esp32/response", "{\"client_id\":\"ESP32-01\",\"message\":\"" + decodedProtocol + "\"}", false);
        }
        irrecv.resume();

        if (ac.isProtocolSupported(results.decode_type)) {
          Serial.println("Protocol " + String(results.decode_type) + " / " + decodedProtocol + " is supported.");
          ac.next.protocol = results.decode_type;  // Change the protocol used.
          unknownCount = 0;
          irrecv.disableIRIn();
          break;
        } else {
          unknownCount++;  // Increment counter if protocol is "UNKNOWN"
          if (unknownCount >= 2) {
            // Send "UNSUPPORTED" after two consecutive "UNKNOWN" protocols
            if (connectionMode == 1) {
              UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
              UDP.print("UNSUPPORTED");
              UDP.endPacket();
            } else {
              publishMessage("esp32/response", "{\"client_id\":\"ESP32-01\",\"message\":\"UNSUPPORTED\"}", false);
            }
            irrecv.disableIRIn();
            break;  // Exit the loop
          }
        }
      } else {
        // No IR data received yet, wait and try again
        if (connectionMode == 1) {
          UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
          UDP.print("WAIT");
          UDP.endPacket();
        } else {
          publishMessage("esp32/response", "{\"client_id\":\"ESP32-01\",\"message\":\"WAIT\"}", false);
        }
        delay(1000);
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
      if (connectionMode == 1) {
        UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
        UDP.print("RECEIVE");
        UDP.endPacket();
      } else {
        publishMessage("esp32/response", "{\"client_id\":\"ESP32-01\",\"message\":\"RECEIVE\"}", false);
      }
      ac.next.degrees = degree;
      ac.next.power = true;  // We want to turn on the A/C unit.
      Serial.println("Sending a message to turn ON the A/C unit.");
      ac.sendAc();  // Have the IRac class create and send a message.
    }
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
      if (connectionMode == 1) {
        UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
        UDP.print("RECEIVE");
        UDP.endPacket();
      } else {
        publishMessage("esp32/response", "{\"client_id\":\"ESP32-01\",\"message\":\"RECEIVE\"}", false);
      }
      ac.next.degrees = degree;
      ac.next.power = false;  // We want to turn on the A/C unit.
      Serial.println("Sending a message to turn OFF the A/C unit.");
      ac.sendAc();  // Have the IRac class create and send a message.
    }
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
    if (connectionMode == 1) {
        UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
        UDP.print("INVALID-CMD");
        UDP.endPacket();
      } else {
        publishMessage("esp32/response", "{\"client_id\":\"ESP32-01\",\"message\":\"INVALID-CMD\"}", false);
    }
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
bool callLearnAPI(const char *buttonId, const char *deviceId, uint16_t *rawData, uint16_t length) {
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

  if (httpResponseCode > 0 && httpResponseCode == 200) {
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

// Call API to save Protocol data to database
bool callLearnAPIWithProtocol(const char *deviceId, const char *Protocol) {
  HTTPClient http;
  String route = "/api/device/updateprotocol";
  http.begin(apiBaseURL + route);
  http.addHeader("Content-Type", "application/json");

  String payload = "{";
  payload += "\"device_id\":\"" + String(deviceId) + "\",";
  payload += "\"Protocol\":\"" + String(Protocol) + "\"";
  payload += "}";

  int httpResponseCode = http.PUT(payload);

  if (httpResponseCode > 0 && httpResponseCode == 200) {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
    return true;
  } else {
    Serial.printf("Error sending PUT to call LearnAPIWithProtocol): %d\n", httpResponseCode);
    return false;
  }
  http.end();
}

// receive raw data from IR receiver and send to API
void receiveRaw(const char *device_id, const char *button_id) {
  irrecv.enableIRIn();
  delay(500);
  static int signalCounter = 0;
  decode_results results;

  while (true) {  // Loop until a signal is received
    if (irrecv.decode(&results)) {
      irrecv.pause();
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
          if (connectionMode == 1) {
            UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
            UDP.print("PRO");
            UDP.endPacket();
          }
          //bool success = callLearnAPI(button_id, device_id, rawData, length);
          bool success = callLearnAPIWithProtocol(device_id, typeToString(results.decode_type).c_str());
          if (success) {
            if (connectionMode == 1) {
              UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
              UDP.print("SUC-PRO");
              UDP.endPacket();
            } else {
              publishMessage("esp32/response", "{\"client_id\":\"ESP32-01\",\"message\":\"SUC-PRO\"}", false);
            }
          } else {
            if (connectionMode == 1) {
              UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
              UDP.print("NETWORK-ERR");
              UDP.endPacket();
            } else {
              publishMessage("esp32/response", "{\"client_id\":\"ESP32-01\",\"message\":\"NETWORK-ERR\"}", false);
            }
          }
        } else if (length > 1000) {
          if (connectionMode == 1) {
            UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
            UDP.print("LEARN-FAIL");
            UDP.endPacket();
          } else {
            publishMessage("esp32/response", "{\"client_id\":\"ESP32-01\",\"message\":\"LEARN-FAIL\"}", false);
          }
        } else {
          if (connectionMode == 1) {
            UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
            UDP.print("NOPRO");
            UDP.endPacket();
          }
          bool success = callLearnAPI(button_id, device_id, rawData, length);
          if (success) {
            if (connectionMode == 1) {
              UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
              UDP.print("SUC-NOPRO");
              UDP.endPacket();
            } else {
              publishMessage("esp32/response", "{\"client_id\":\"ESP32-01\",\"message\":\"SUC-NOPRO\"}", false);
            }
          } else {
            if (connectionMode == 1) {
              UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
              UDP.print("NETWORK-ERR");
              UDP.endPacket();
            } else {
              publishMessage("esp32/response", "{\"client_id\":\"ESP32-01\",\"message\":\"NETWORK-ERR\"}", false);
            }
          }
        }

        irrecv.resume();
        irrecv.disableIRIn();

        delete[] rawData;
        signalCounter = 0;
      }
      Serial.println();
      yield();
      break;  // Break the loop after successfully processing the signal
    }
  }
}

// control device by learned data
void sendRaw(const char *device_id, const char *button_id) {
  int length;
  uint16_t *rawData;
  Serial.println("Start Fetch");
  bool success = getSignalData(apiBaseURL, String(device_id), String(button_id), length, rawData);

  if (success) {
    if (connectionMode == 1) {
      UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
      UDP.print("SUC-SEND");
      UDP.endPacket();
    } else {
      publishMessage("esp32/response", "{\"client_id\":\"ESP32-01\",\"message\":\"SUC-SEND\"}", false);
    }
    Serial.println("Raw from TestReceiveRaw");
    irsend.sendRaw(rawData, length, 38);
    Serial.println("Sent");
  } else {
    if (connectionMode == 1) {
      UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
      UDP.print("NETWORK-ERR");
      UDP.endPacket();
    } else {
      publishMessage("esp32/response", "{\"client_id\":\"ESP32-01\",\"message\":\"NETWORK-ERR\"}", false);
    }
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

    if (httpCode > 0 && httpCode == 200) {  // Check for the returning code
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
    if (connectionMode == 1) {
      UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
      UDP.print("SUC-SEND");
      UDP.endPacket();
    } else {
      publishMessage("esp32/response", "{\"client_id\":\"ESP32-01\",\"message\":\"SUC-SEND\"}", false);
    }
    Serial.println("Raw from TestReceiveRaw");
    irsend.sendRaw(rawData, length, 38);
    Serial.println("Sent");
  } else {
    if (connectionMode == 1) {
      UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
      UDP.print("NETWORK-ERR");
      UDP.endPacket();
    } else {
      publishMessage("esp32/response", "{\"client_id\":\"ESP32-01\",\"message\":\"NETWORK-ERR\"}", false);
    }
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

    if (httpCode > 0 && httpCode == 200) {  // Check for the returning code
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