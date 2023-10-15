#include <ESP8266WiFi.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>

// Define your WiFi credentials
const char* ssid = "";
const char* password = "";

// Define MySQL server settings
IPAddress server_ip(192, 168, 1, 100); // IP address of your MySQL server
int server_port = 3306;               // MySQL server port
char db_user[] = "root";              // MySQL user
char db_password[] = "DambaokhieM"; // MySQL password
char db_name[] = "esp_test";          // MySQL database name

WiFiClient client;
MySQL_Connection conn((Client *)&client);

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for serial port to connect (for debugging)

  // Connect to WiFi
  Serial.printf("\nConnecting to %s", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.print("My IP address is: ");
  Serial.println(WiFi.localIP());

  // Connect to MySQL
  Serial.print("Connecting to MySQL...  ");
  if (conn.connect(server_ip, server_port, db_user, db_password, db_name)) {
    Serial.println("OK.");
    createTable();
    insertData();
    conn.close();
  } else {
    Serial.println("FAILED.");
  }
}

void loop() {
}

void createTable() {
  Serial.println("Creating table...");
  char query[] = "CREATE TABLE IF NOT EXISTS esp_test (id INT AUTO_INCREMENT PRIMARY KEY, name VARCHAR(255))";

  MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
  cur_mem->execute(query);
  delete cur_mem;
  Serial.println("Table created.");
}

void insertData() {
  Serial.println("Inserting data...");
  char query[] = "INSERT INTO esp_test (name) VALUES ('adamn'), ('eva')";

  MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
  cur_mem->execute(query);
  delete cur_mem;
  Serial.println("Data inserted.");
}
