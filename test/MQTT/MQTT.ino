#include <ArduinoJson.h>
#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "";     // Wifi connect
const char* password = "";  // Password

const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;                     // Change to 1883 for non-secure connection
const char* mqtt_username = "";       // User
const char* mqtt_password = "";  // Password

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientID = "ESP32-01";
    if (client.connect(clientID.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("connected");
      client.subscribe("esp32/connect");  // subscribe to the topic
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.write(payload, length);
  Serial.println();

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

  const char* command = doc["command"];
  if (command) {
    Serial.print("Command received: ");
    Serial.println(command);

    // Acknowledge command receipt
    publishMessage("esp32/acks", "{\"client_id\":\"ESP32-01\",\"message\":\"RECEIVE\"}", true);
  }
}

void publishMessage(const char* topic, String payload, boolean retained) {
  if (client.publish(topic, payload.c_str(), retained))
    Serial.println("Message published [" + String(topic) + "]: " + payload);
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
