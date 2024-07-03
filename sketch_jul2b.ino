#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SPI.h>
#include <MFRC522.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <FS.h>

// MFRC522 configuration
#define RST_PIN D1
#define SS_PIN D2
#define BUZZER_PIN D3  // Define the pin for the buzzer

MFRC522 mfrc522(SS_PIN, RST_PIN);
ESP8266WebServer server(80);

// WiFi and MySQL
char ssid[32];
char password[32];
char apPassword[32];

// MySQL configuration
char db_host[32]; // MySQL host
char db_user[32]; // MySQL user
char db_password[32]; // MySQL password
char db_name[32]; // MySQL database name
int db_port = 3306; // Default MySQL port (adjust if needed)

WiFiClient client;
MySQL_Connection conn(&client);

// NTP Client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "asia.pool.ntp.org", 5.5 * 3600, 60000);

// Time ranges
int startHour1;
int endHour1;
int startHour2;
int endHour2;

// JSON configuration file path
const char* configFilePath = "/config.json";

// Function prototypes
void handleRoot();
void handleConfig();
void handleSave();
void connectToWiFi();
void setupWebServer();
void loadConfig();
void saveConfig();
void startAPMode();
void fallbackToAPMode();
void ensureMySQLConnection();
void beepBuzzer(int duration);
String readNFCString();

void setup() {
  delay(1000);
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  loadConfig();
  connectToWiFi();
  timeClient.begin();
  setupWebServer();
  ensureMySQLConnection();
}

void loop() {
  server.handleClient();
  if(WiFi.status() == WL_CONNECTED){
    timeClient.update();

    int currentHour = timeClient.getHours();

    if ((currentHour >= startHour1 && currentHour < endHour1) ||
        (currentHour >= startHour2 && currentHour < endHour2)) {

      if (!mfrc522.PICC_IsNewCardPresent()) {
        delay(50);
        return;
      }

      if (!mfrc522.PICC_ReadCardSerial()) {
        delay(50);
        return;
      }

      // Read string data from the card
      String cardData = readNFCString();

      if (cardData != "") {
        Serial.println("Card Data: " + cardData);
        beepBuzzer(100); // Beep the buzzer for 100ms
        
        // Prepare SQL query
        char query[256];
        sprintf(query, "INSERT INTO test (name) VALUES ('%s')",  cardData.c_str());

        // Execute SQL query
        ensureMySQLConnection();
        if (conn.connected()) {
          MySQL_Cursor* cursor = new MySQL_Cursor(&conn);
          cursor->execute(query);
          delete cursor;
          Serial.println("Entry added to database.");
        } else {
          Serial.println("Not connected to MySQL server.");
        }

        delay(5000); // 5-second delay after a card is read
      } else {
        Serial.println("No data read from card.");
      }

      // Halt the PICC and stop encryption
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
    } else {
      Serial.println("Out of time range.");
    }
  }
}

void connectToWiFi() {
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();

  // Wait for connection, 10 seconds timeout
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Failed to connect to WiFi. Starting AP mode...");
    fallbackToAPMode();
  }
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/config", HTTP_GET, handleConfig);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
  Serial.println("HTTP server started");
}

void handleRoot() {
  String html = "<h1>ESP8266 NFC Reader</h1>";
  html += "<p>WiFi Connected: " + String(WiFi.status() == WL_CONNECTED ? "Yes" : "No") + "</p>";
  html += "<p>MySQL Connected: " + String(conn.connected() ? "Yes" : "No") + "</p>";
  html += "<p>Current Time: " + timeClient.getFormattedTime() + "</p>";
  html += "<a href=\"/config\">Configure Settings</a><br>";
  server.send(200, "text/html", html);
}

void handleConfig() {
  String html = "<h1>Configure Settings</h1>";
  html += "<form action=\"/save\" method=\"post\">";
  html += "WiFi SSID: <input type=\"text\" name=\"ssid\" value=\"" + String(ssid) + "\"><br>";
  html += "WiFi Password: <input type=\"text\" name=\"password\" value=\"" + String(password) + "\"><br>";
  html += "AP Password: <input type=\"text\" name=\"apPassword\" value=\"" + String(apPassword) + "\"><br>";
  html += "DB Host: <input type=\"text\" name=\"db_host\" value=\"" + String(db_host) + "\"><br>";
  html += "DB User: <input type=\"text\" name=\"db_user\" value=\"" + String(db_user) + "\"><br>";
  html += "DB Password: <input type=\"text\" name=\"db_password\" value=\"" + String(db_password) + "\"><br>";
  html += "DB Name: <input type=\"text\" name=\"db_name\" value=\"" + String(db_name) + "\"><br>";
  html += "DB Port: <input type=\"number\" name=\"db_port\" value=\"" + String(db_port) + "\"><br>";
  html += "Start Hour 1: <input type=\"number\" name=\"start1\" min=\"0\" max=\"23\" value=\"" + String(startHour1) + "\"><br>";
  html += "End Hour 1: <input type=\"number\" name=\"end1\" min=\"0\" max=\"23\" value=\"" + String(endHour1) + "\"><br>";
  html += "Start Hour 2: <input type=\"number\" name=\"start2\" min=\"0\" max=\"23\" value=\"" + String(startHour2) + "\"><br>";
  html += "End Hour 2: <input type=\"number\" name=\"end2\" min=\"0\" max=\"23\" value=\"" + String(endHour2) + "\"><br>";
  html += "<input type=\"submit\" value=\"Save\">";
  html += "</form>";
  server.send(200, "text/html", html);
}

void handleSave() {
  if (server.hasArg("ssid") && server.hasArg("password") &&
      server.hasArg("apPassword") &&
      server.hasArg("db_host") && server.hasArg("db_user") &&
      server.hasArg("db_password") && server.hasArg("db_name") && server.hasArg("db_port") &&
      server.hasArg("start1") && server.hasArg("end1") &&
      server.hasArg("start2") && server.hasArg("end2")) {

    String newSSID = server.arg("ssid");
    String newPassword = server.arg("password");
    String newApPassword = server.arg("apPassword");
    String newDbHost = server.arg("db_host");
    String newDbUser = server.arg("db_user");
    String newDbPassword = server.arg("db_password");
    String newDbName = server.arg("db_name");
    int newDbPort = server.arg("db_port").toInt();
    startHour1 = server.arg("start1").toInt();
    endHour1 = server.arg("end1").toInt();
    startHour2 = server.arg("start2").toInt();
    endHour2 = server.arg("end2").toInt();

    newSSID.toCharArray(ssid, sizeof(ssid));
    newPassword.toCharArray(password, sizeof(password));
    newApPassword.toCharArray(apPassword, sizeof(apPassword));
    newDbHost.toCharArray(db_host, sizeof(db_host));
    newDbUser.toCharArray(db_user, sizeof(db_user));
    newDbPassword.toCharArray(db_password, sizeof(db_password));
    newDbName.toCharArray(db_name, sizeof(db_name));
    db_port = newDbPort;

    saveConfig();
    server.send(200, "text/html", "<h1>Settings Saved</h1><a href=\"/\">Back to Home</a>");

    delay(1000);
    ESP.restart();
  } else {
    server.send(400, "text/html", "Invalid input");
  }
}

void loadConfig() {
  File configFile = SPIFFS.open(configFilePath, "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    fallbackToAPMode();
    return;
  }

  StaticJsonDocument<512> jsonConfig;
  DeserializationError error = deserializeJson(jsonConfig, configFile);

  if (error) {
    Serial.println("Failed to read config file");
    fallbackToAPMode();
    return;
  }

  strlcpy(ssid, jsonConfig["ssid"] | "", sizeof(ssid));
  strlcpy(password, jsonConfig["password"] | "", sizeof(password));
  strlcpy(apPassword, jsonConfig["apPassword"] | "", sizeof(apPassword));
  strlcpy(db_host, jsonConfig["db_host"] | "", sizeof(db_host));
  strlcpy(db_user, jsonConfig["db_user"] | "", sizeof(db_user));
  strlcpy(db_password, jsonConfig["db_password"] | "", sizeof(db_password));
  strlcpy(db_name, jsonConfig["db_name"] | "", sizeof(db_name));
  db_port = jsonConfig["db_port"] | 3306; // Default to 3306 if not provided

  startHour1 = jsonConfig["startHour1"] | 8; // Default to 8:00 AM
  endHour1 = jsonConfig["endHour1"] | 17;   // Default to 5:00 PM
  startHour2 = jsonConfig["startHour2"] | 20; // Default to 8:00 PM
  endHour2 = jsonConfig["endHour2"] | 23;   // Default to 11:00 PM

  configFile.close();
}

void saveConfig() {
  StaticJsonDocument<512> jsonConfig;
  jsonConfig["ssid"] = ssid;
  jsonConfig["password"] = password;
  jsonConfig["apPassword"] = apPassword;
  jsonConfig["db_host"] = db_host;
  jsonConfig["db_user"] = db_user;
  jsonConfig["db_password"] = db_password;
  jsonConfig["db_name"] = db_name;
  jsonConfig["db_port"] = db_port;
  jsonConfig["startHour1"] = startHour1;
  jsonConfig["endHour1"] = endHour1;
  jsonConfig["startHour2"] = startHour2;
  jsonConfig["endHour2"] = endHour2;

  File configFile = SPIFFS.open(configFilePath, "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return;
  }

  serializeJson(jsonConfig, configFile);
  configFile.close();
}

void startAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, apPassword);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
}

void fallbackToAPMode() {
  Serial.println("Falling back to AP mode...");
  strcpy(ssid, "ESP8266-AP");
  strcpy(apPassword, "password");
  startAPMode();
}

void ensureMySQLConnection() {
  if (conn.connected()) {
    return;
  }

  IPAddress ip;
  if (!ip.fromString(db_host)) {
    Serial.println("Invalid MySQL server address");
    return;
  }

  Serial.print("Connecting to MySQL Server...");
  if (conn.connect(ip, db_port, db_user, db_password, db_name)) {
    Serial.println("Connected to MySQL server");
  } else {
    Serial.println("MySQL connection failed");
  }
}

void beepBuzzer(int duration) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(duration);
  digitalWrite(BUZZER_PIN, LOW);
}

String readNFCString() {
  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF; // Default key for authentication
  
  byte sector = 1; // Reading from the second sector (Sector 1)
  byte block = sector * 4; // Block number to read from (first block of Sector 1)
  byte buffer[18]; // Buffer to store the read data
  byte size = sizeof(buffer);

  // Authenticate using the default key for the sector
  MFRC522::StatusCode status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print("PCD_Authenticate() failed: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return ""; // Return an empty string indicating failure
  }

  // Read the block from the card
  status = mfrc522.MIFARE_Read(block, buffer, &size);
  if (status != MFRC522::STATUS_OK) {
    Serial.print("MIFARE_Read() failed: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return ""; // Return an empty string indicating failure
  }

  // Convert the buffer to a string
  String cardData = "";
  for (byte i = 0; i < 16; i++) {
    if (buffer[i] != 0) { // Ignore trailing null bytes
      cardData += (char)buffer[i];
    }
  }

  return cardData; // Return the card data string
}
