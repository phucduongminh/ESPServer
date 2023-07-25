#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>

#define UDP_PORT 12345
#define SSID "Tue Minh Vlog"
#define PASSWD "19941996"
#define SOCK_PORT 124

#define IR_RECV_PIN 14  // GPIO5 (D5) for IR receiver
#define IR_LED_PIN 4   // GPIO4 (D2) for IR LED

WiFiUDP UDP;
IRrecv irrecv(IR_RECV_PIN);
IRsend irsend(IR_LED_PIN);

char packet[255];
char reply[] = "Packet received!";


void setup() {
    Serial.begin(115200);
    delay(1000);
    WiFi.begin(SSID, PASSWD);
    while (WiFi.status() != WL_CONNECTED) { delay(100); }

    Serial.println("");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    UDP.begin(UDP_PORT);
    Serial.print("Listening on UDP port: ");
    Serial.println(UDP_PORT);

    irrecv.enableIRIn();
    irsend.begin();
}

void startUdpServer() {
    UDP.stop();
    UDP.begin(UDP_PORT);
    Serial.print("Listening on UDP port: ");
    Serial.println(UDP_PORT);
}

void handleMessage(const char* message) {
    if (strcmp(message, "ESP-ACK") == 0) {
        // Close old UDP socket server and begin new
        UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
        UDP.print(WiFi.localIP());
        UDP.endPacket();
    } else if (strcmp(message, "CANCEL") == 0) {
        startUdpServer(); // Close old UDP socket server and begin new
    } else if (strcmp(message, "SEND") == 0) {
        int remoteNameLen = UDP.read(packet, 255);
        if (remoteNameLen > 0) {
            packet[remoteNameLen] = '\0';
            String remoteName = String(packet);
            Serial.print("Received remote name: ");
            Serial.println(remoteName);

            while (true) {
                int codeLen = UDP.read(packet, 255);
                if (codeLen <= 0) {
                    break;
                }
                packet[codeLen] = '\0';
                String hexCode = String(packet);
                Serial.print("Received hex code for ");
                Serial.print(remoteName);
                Serial.print(": ");
                Serial.println(hexCode);

                if (hexCode == "CLOSE") {
                    break; // Break while loop if "CLOSE" received
                }

                if (remoteName == "SONY") {
                    unsigned long irCode = strtoul(hexCode.c_str(), NULL, 16);
                    irsend.sendSony(irCode, hexCode.length() * 4);
                }
            }
        }
    } else if (strcmp(message, "RECEIVE") == 0) {
        String decodedProtocol = "";
        while (decodedProtocol == "" || decodedProtocol == "UNKNOWN") {
            decode_results results;
            if (irrecv.decode(&results)) {
                decodedProtocol = typeToString(results.decode_type);
                Serial.print("Received IR signal, protocol: ");
                Serial.println(decodedProtocol);

                UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
                UDP.print(decodedProtocol);
                UDP.endPacket();
                irrecv.resume();
            }
            else {
                // No IR data received yet, wait and try again
                UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
                UDP.print("WAIT");
                UDP.endPacket();
                delay(100);
            }
        }
    } else {
        // Unknown message type, handle appropriately (if needed)
        // ...
    }
}

/*String typeToString(decode_type_t decodeType) {
    if (decodeType == NEC) {
        return "NEC";
    } else if (decodeType == SONY) {
        return "SONY";
    } else {
        return "Unsupported";
    }
}*/

void loop() {
    // If packet received...
    int packetSize = UDP.parsePacket();
    if (packetSize) {
        Serial.print("Received packet! Size: ");
        Serial.println(packetSize);

        int len = UDP.read(packet, sizeof(packet) - 1); // Leave space for null terminator
        packet[len] = '\0'; // Null-terminate the received data
        Serial.println(String(packet));
        handleMessage(packet);
    }
}
