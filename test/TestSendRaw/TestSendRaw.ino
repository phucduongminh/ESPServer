#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char *ssid = "Tue Minh Vlog";
const char *password = "wifichongtrom";
String baseURL = "http://192.168.1.39:3001";
String device_id = "myairphucdu";
String button_id = "power-off";

const uint16_t kIrLed = 4; // ESP8266 GPIO pin to use. Recommended: 4 (D2).

IRsend irsend(2); // Set the GPIO to be used to sending the message.

uint16_t *stringToRawData(String str, int length)
{
  uint16_t *rawData = new uint16_t[length];
  char *token = strtok((char *)str.c_str(), ", ");
  for (int i = 0; token != NULL && i < length; i++)
  {
    rawData[i] = (uint16_t)atoi(token);
    token = strtok(NULL, ", ");
  }
  return rawData;
}

void getSignalData(const String& baseURL, const String& device_id, const String& button_id, int& length, uint16_t*& rawData) {
  if ((WiFi.status() == WL_CONNECTED)) { //Check the current connection status
    HTTPClient http;

    String url = baseURL + "/api/signal/learns/getbyid?device_id=" + device_id + "&button_id=" + button_id;
    //String url = "https://http.cat/100";
    http.begin(url.c_str()); //Specify the URL
    int httpCode = http.GET();

    // Print the HTTP status code
    Serial.print("HTTP response status code: ");
    Serial.println(httpCode);

    if (httpCode>0) { //Check for the returning code
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
        return;
      }

      // Get the values from the JSON document
      length = doc["signal"]["rawdata_length"];
      String rawDataString = doc["signal"]["rawdata"];

      // Remove the curly braces from the rawDataString
      rawDataString.remove(0, 1); // remove the first character
      rawDataString.remove(rawDataString.length() - 1); // remove the last character

      // Convert the rawDataString to an array of uint16_t
      rawData = stringToRawData(rawDataString, length);
    } else {
      Serial.println("Error on HTTP request");
    }

    http.end(); //Free the resources
  }
}

void setup()
{
  irsend.begin();
#if ESP8266
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
#else  // ESP8266
  Serial.begin(115200, SERIAL_8N1);
#endif // ESP8266
  Serial.begin(115200);
  delay(4000); // Delay needed before starting the WiFi

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  { // Check for the connection
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  Serial.println("Connected to the WiFi network");
}

void loop()
{
  int length;
  uint16_t* rawData;

  getSignalData(baseURL, device_id, button_id, length, rawData);

  delay(1000);
  Serial.println("Raw from TestReceiveRaw");
  irsend.sendRaw(rawData, length, 38); // Send a raw data capture at 38kHz.  //Raw work )))))bu lol kkkk
  // irsend.sendElectraAC(state); //State worked
  //delay(3000);
  //irsend.sendRaw(rawOff, 179, 38);
  delay(10000);
}
