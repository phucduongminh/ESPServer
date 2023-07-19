#include <ESP8266WiFi.h>

#define SSID "Tue Minh Vlog"
#define PASSWD "19941996"
#define SOCK_PORT 124

WiFiServer sockServer(SOCK_PORT);

void setup(){
    Serial.begin(115200);
    delay(1000);
    WiFi.begin(SSID,PASSWD);
    while (WiFi.status() != WL_CONNECTED){delay(100);}

    Serial.println("");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    sockServer.begin();
}

void loop(){
    WiFiClient client = sockServer.available();
    if (client){
        //Serial.println("Iniciou uma conexao!");
        unsigned int count = 1000;
        unsigned int countDevice = 100000;
        while (client.connected() && --count > 0){
            delay(200);
            String treeWayBuffer = String();
            bool hasData = false;
            while (client.available() > 0 && --countDevice > 0){
              //if (treeWayBuffer.length() < 255) {
                treeWayBuffer.concat((char)client.read());
              //}
              hasData = true;
            }
            if (hasData) {
              Serial.println(treeWayBuffer);
              if (treeWayBuffer.equals("SYN")) {
                client.print("SYN-ACK");
              }
            }
        }
        //client.flush();
        client.stop();
        delay(10);
    }
}