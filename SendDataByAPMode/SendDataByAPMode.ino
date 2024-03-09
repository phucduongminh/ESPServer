// Đây là file main.cpp cho ESP32
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// Khai báo tên và mật khẩu của wifi Access point
const char* ssid = "ESP32_AP";
const char* password = "12345678";

// Khởi tạo một đối tượng WebServer
WebServer server(80);

// Hàm xử lý khi có yêu cầu POST đến địa chỉ IP của ESP32
void handlePost() {
  // Kiểm tra xem có dữ liệu gửi đến không
  if (server.hasArg("plain")) {
    // Lấy dữ liệu dưới dạng chuỗi
    String data = server.arg("plain");
    // Tạo một đối tượng StaticJsonDocument để phân tích dữ liệu JSON
    StaticJsonDocument<200> doc;
    // Phân tích dữ liệu JSON
    DeserializationError error = deserializeJson(doc, data);
    // Nếu không có lỗi
    if (!error) {
      // Lấy giá trị của trường wifiname và password
      String wifiname = doc["wifiname"];
      String password = doc["password"];
      // In ra giá trị của wifiname và password
      Serial.println("Wifiname: " + wifiname);
      Serial.println("Password: " + password);
      // Gửi trả về mã trạng thái 200, tức là thành công
      server.send(200);
    } else {
      // Ngược lại, gửi trả về mã trạng thái 400, tức là lỗi
      server.send(400);
    }
  } else {
    // Nếu không có dữ liệu gửi đến, gửi trả về mã trạng thái 400, tức là lỗi
    server.send(400);
  }
}

void setup() {
  // Khởi tạo cổng Serial
  Serial.begin(115200);
  // Thiết lập chế độ Access point cho ESP32
  WiFi.softAP(ssid, password);
  delay(1000);
  // In ra địa chỉ IP của ESP32
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());
  // Đăng ký hàm xử lý khi có yêu cầu POST đến địa chỉ IP của ESP32
  server.on("/", HTTP_POST, handlePost);
  // Bắt đầu WebServer
  server.begin();
}

void loop() {
  // Xử lý các yêu cầu đến WebServer
  server.handleClient();
}
