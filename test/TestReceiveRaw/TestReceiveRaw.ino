#include <Arduino.h>
#include <assert.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRac.h>
#include <IRtext.h>
#include <IRutils.h>
#include <HTTPClient.h>

const char *ssid = "";
const char *password = "";

#ifdef ARDUINO_ESP32C3_DEV
const uint16_t kRecvPin = 10;  // 14 on a ESP32-C3 causes a boot loop.
#else                          // ARDUINO_ESP32C3_DEV
const uint16_t kRecvPin = 14;
#endif                         // ARDUINO_ESP32C3_DEV

const uint32_t kBaudRate = 115200;

const uint16_t kCaptureBufferSize = 1024;

#if DECODE_AC
// Some A/C units have gaps in their protocols of ~40ms. e.g. Kelvinator
// A value this large may swallow repeats of some protocols
const uint8_t kTimeout = 50;
#else   // DECODE_AC
// Suits most messages, while not swallowing many repeats.
const uint8_t kTimeout = 15;
#endif  // DECODE_AC

const uint16_t kMinUnknownSize = 12;

const uint8_t kTolerancePercentage = kTolerance;  // kTolerance is normally 25%

// Legacy (No longer supported!)
//
// Change to `true` if you miss/need the old "Raw Timing[]" display.
#define LEGACY_TIMING_INFO false
// ==================== end of TUNEABLE PARAMETERS ====================

// Use turn on the save buffer feature for more complete capture coverage.
IRrecv irrecv(15, kCaptureBufferSize, kTimeout, true);
decode_results results;  // Somewhere to store the results

//Raw To String
String rawDataToString(uint16_t* rawData, uint16_t length) {
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

void callAPI(const String& baseURL, const String& button_id, const String& device_id, int rawdata_length, const String& rawdata) {
  HTTPClient http;

  String route = "/api/signal/learns/new";

  http.begin(baseURL + route); 
  http.addHeader("Content-Type", "application/json");

  // Create the JSON payload
  String payload = "{";
  payload += "\"button_id\":\"" + button_id + "\",";
  payload += "\"device_id\":\"" + device_id + "\",";
  payload += "\"rawdata_length\":" + String(rawdata_length) + ",";
  payload += "\"rawdata\":\"" + rawdata + "\"";
  payload += "}";

  int httpResponseCode = http.POST(payload);  // Send the POST request

  if (httpResponseCode>0) {
    String response = http.getString();
    Serial.println(httpResponseCode);   // Print the response code
    Serial.println(response);           // Print the response body
  } else {
    Serial.printf("Error on sending POST to route %s: ", route.c_str());
    Serial.println(httpResponseCode);
  }

  http.end();
}

// This section of code runs only once at start-up.
void setup() {
#if defined(ESP8266)
  Serial.begin(kBaudRate, SERIAL_8N1, SERIAL_TX_ONLY);
#else              // ESP8266
  Serial.begin(kBaudRate, SERIAL_8N1);
#endif             // ESP8266
  while (!Serial)  // Wait for the serial connection to be establised.
    delay(50);
  // Perform a low level sanity checks that the compiler performs bit field
  // packing as we expect and Endianness is as we expect.
  assert(irutils::lowLevelSanityCheck() == 0);

  Serial.printf("\n" D_STR_IRRECVDUMP_STARTUP "\n", kRecvPin);
#if DECODE_HASH
  // Ignore messages with less than minimum on or off pulses.
  irrecv.setUnknownThreshold(kMinUnknownSize);
#endif                                        // DECODE_HASH
  irrecv.setTolerance(kTolerancePercentage);  // Override the default tolerance.
  irrecv.enableIRIn();                        // Start the receiver
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  { // Check for the connection
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  Serial.println("Connected to the WiFi network");
}

// The repeating section of the code
void loop() {
  // Check if the IR code has been received.
  static int signalCounter = 0;  // Counter for received signals
  if (irrecv.decode(&results)) {
     signalCounter++;
    Serial.println();
    if (signalCounter == 2) {
      uint16_t* rawData = resultToRawData(&results);
      const uint16_t length = getCorrectedRawLength(&results);
      String str = rawDataToString(rawData, length);
      Serial.printf("rawData[%d] = %s\n", (int)length, str.c_str());  // Print the raw data
      callAPI("http://192.168.100.39:3001", "power", "myairphucdu", (int)length, str);
      delete[] rawData;

      signalCounter = 0;  // Reset the counter
    }
    Serial.println();  // Blank line between entries
    yield();           // Feed the WDT (again)
  }
}