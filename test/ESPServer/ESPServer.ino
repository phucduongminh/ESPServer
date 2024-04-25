#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#define UDP_PORT 12345
#define SSID ""
#define PASSWD ""
#define SOCK_PORT 124

WiFiUDP UDP;

char packet[255];
char reply[] = "Packet received!";


void setup(){
    Serial.begin(115200);
    delay(1000);
    WiFi.begin(SSID,PASSWD);
    while (WiFi.status() != WL_CONNECTED){delay(100);}

    Serial.println("");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    UDP.begin(UDP_PORT);
    Serial.print("Listening on UDP port: ");
    Serial.println(UDP_PORT);
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
    // Handle CANCEL message (if needed)
    // ...
  } else if (strcmp(message, "SEND") == 0) {
    // Handle SEND message (if needed)
    // ...
  } else if (strcmp(message, "RECEIVE") == 0) {
    // Handle RECEIVE message (if needed)
    // ...
  } else {
    // Unknown message type, handle appropriately (if needed)
    // ...
  }
}

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
