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
#include <TimeLib.h>
#include <Adafruit_NeoPixel.h>

#define LED_PIN D4
#define LED_COUNT 4



// MFRC522 configuration
#define RST_PIN D1
#define SS_PIN D2
#define BUZZER_PIN D0  // Define the pin for the buzzer

MFRC522 mfrc522(SS_PIN, RST_PIN);

Adafruit_NeoPixel strip = Adafruit_NeoPixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

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
void setup();
void loop();
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
void setLEDColor(int ledIndex, uint32_t color);
void setLEDColorAll(uint32_t color);
void blinkLED(int ledIndex, uint32_t color, int duration);


void setup() {
  delay(500);
  pinMode(D1, OUTPUT);
  digitalWrite(D1, HIGH); // Ensure D1 (GPIO5) is high
  
  pinMode(D2, OUTPUT);
  digitalWrite(D2, LOW); // Ensure D2 (GPIO4) is low
  
  pinMode(D0, OUTPUT);
  digitalWrite(D0, LOW); // Initial state for buzzer 
  
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();

  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  strip.begin();
  strip.show(); 
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

       

    if ((currentHour >= startHour1 && currentHour < endHour1) || (currentHour >= startHour2 && currentHour < endHour2)) {
      
      if(!conn.connected()){
        setLEDColor(1, strip.Color(255, 0, 0));
        ensureMySQLConnection();
      }
      if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
        setLEDColor(2, strip.Color(100, 0, 0)); // No card present
        setLEDColor(3, strip.Color(100, 0, 0)); // No card present
        delay(50);
        return;
      }

      // Read string data from the card
      String cardData = readNFCString();

      if (cardData != "") {
        Serial.println("Card Data: " + cardData);
        beepBuzzer(100); // Beep the buzzer for 100ms

        time_t rawTime = timeClient.getEpochTime();
        char buffer[20];
        sprintf(buffer, "%04d-%02d-%02d", year(rawTime), month(rawTime), day(rawTime));
        String date_value = buffer;
        String place_time_value = timeClient.getFormattedTime();
        
        // Prepare SQL query
        char query[256];
        sprintf(query, "INSERT INTO orders (employee_id, date, place_time, taken) VALUES ('%s', '%s', '%s', 'Not Taken')",
                cardData.c_str(), date_value.c_str(), place_time_value.c_str());

        // Execute SQL query
        
        if(!conn.connected()){
          setLEDColor(1, strip.Color(255, 0, 0));
          ensureMySQLConnection();
        }
       
        if (conn.connected()) {
          MySQL_Cursor* cursor = new MySQL_Cursor(&conn);
          cursor->execute(query);
          delete cursor;
          Serial.println("Entry added to database.");
          setLEDColor(2, strip.Color(0, 100, 0)); // Card present, data saved (green)
          setLEDColor(3, strip.Color(0, 100, 0)); // Card present, data saved (green)
        } else {
          Serial.println("Not connected to MySQL server.");
          setLEDColor(1, strip.Color(100, 0, 0));
          ensureMySQLConnection();
          setLEDColor(2, strip.Color(100, 0, 0)); // Card present, data not saved 
          setLEDColor(3, strip.Color(0, 100, 0)); // Card present, data not saved 
        }

        delay(5000); // 5-second delay after a card is read
      } else {
        Serial.println("No data read from card.");
        setLEDColor(2, strip.Color(100, 0, 0)); // Card present, data not saved 
        setLEDColor(3, strip.Color(0, 100, 0)); // Card present, data not saved 
      }

      // Halt the PICC and stop encryption
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
    } else {
      setLEDColor(2, strip.Color(100, 0, 0)); // Out of time range (red)
      setLEDColor(3, strip.Color(0, 0, 0)); // Out of time range (red)
    }
  }else{
    connectToWiFi();
  }
}

void connectToWiFi() {
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();

  // Wait for connection, 60 seconds timeout
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 60000) {
    delay(500);
    Serial.print(".");
    blinkLED(0, strip.Color(0, 100, 0), 500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    setLEDColor(0, strip.Color(0, 0, 100)); // Solid blue
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
      server.hasArg("apPassword") && server.hasArg("db_host") &&
      server.hasArg("db_user") && server.hasArg("db_password") && server.hasArg("db_name") &&
      server.hasArg("start1") && server.hasArg("end1") &&
      server.hasArg("start2") && server.hasArg("end2")) {
    server.arg("ssid").toCharArray(ssid, sizeof(ssid));
    server.arg("password").toCharArray(password, sizeof(password));
    server.arg("apPassword").toCharArray(apPassword, sizeof(apPassword));
    server.arg("db_host").toCharArray(db_host, sizeof(db_host));
    server.arg("db_user").toCharArray(db_user, sizeof(db_user));
    server.arg("db_password").toCharArray(db_password, sizeof(db_password));
    server.arg("db_name").toCharArray(db_name, sizeof(db_name));
    db_port = server.arg("db_port").toInt();
    startHour1 = server.arg("start1").toInt();
    endHour1 = server.arg("end1").toInt();
    startHour2 = server.arg("start2").toInt();
    endHour2 = server.arg("end2").toInt();

    saveConfig();
    server.send(200, "text/html", "<p>Configuration saved! Restarting...</p>");
    delay(2000);
    ESP.restart();
  } else {
    server.send(400, "text/html", "<p>Missing required parameters</p>");
  }
}

void loadConfig() {
  if (SPIFFS.exists(configFilePath)) {
    File configFile = SPIFFS.open(configFilePath, "r");
    if (configFile) {
      size_t size = configFile.size();
      std::unique_ptr<char[]> buf(new char[size]);
      configFile.readBytes(buf.get(), size);
      configFile.close();

      StaticJsonDocument<512> json;
      DeserializationError error = deserializeJson(json, buf.get());
      if (!error) {
        strcpy(ssid, json["ssid"]);
        strcpy(password, json["password"]);
        strcpy(apPassword, json["apPassword"]);
        strcpy(db_host, json["db_host"]);
        strcpy(db_user, json["db_user"]);
        strcpy(db_password, json["db_password"]);
        strcpy(db_name, json["db_name"]);
        db_port = json["db_port"];
        startHour1 = json["start1"];
        endHour1 = json["end1"];
        startHour2 = json["start2"];
        endHour2 = json["end2"];
      } else {
        Serial.println("Failed to parse config file");
      }
    }
  }
}

void saveConfig() {
  StaticJsonDocument<512> json;
  json["ssid"] = ssid;
  json["password"] = password;
  json["apPassword"] = apPassword;
  json["db_host"] = db_host;
  json["db_user"] = db_user;
  json["db_password"] = db_password;
  json["db_name"] = db_name;
  json["db_port"] = db_port;
  json["start1"] = startHour1;
  json["end1"] = endHour1;
  json["start2"] = startHour2;
  json["end2"] = endHour2;

  File configFile = SPIFFS.open(configFilePath, "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return;
  }
  serializeJson(json, configFile);
  configFile.close();
}

void startAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("NFC_v1.0.1_Config", apPassword);
  Serial.println("AP mode started");
  Serial.println("IP address: ");
  Serial.println(WiFi.softAPIP());
  setupWebServer();

  unsigned long startTime = millis();
  while (millis() - startTime < 2 * 60 * 1000) {
    // Keep the AP mode running
    WiFi.softAP("ESP8266_Config_AP", apPassword);
    yield(); // Allow the device to respond to incoming connections or requests
  }
  
}

void fallbackToAPMode() {
  startAPMode();
}



void ensureMySQLConnection() {
  if (!conn.connected()) {
    Serial.println("Connecting to MySQL...");
    IPAddress ip;
    if (!ip.fromString(db_host)) {
      Serial.println("Invalid MySQL server address");
      return;
    }

    // Use a timeout variable to avoid infinite loop
    unsigned long timeout = millis() + 10000; // 10 seconds
    while (!conn.connect(ip, db_port, db_user, db_password, db_name)) {
      blinkLED(1, strip.Color(0, 100, 0), 300); // green blinking
      if (millis() > timeout) {
        Serial.println("MySQL connection timed out.");
        setLEDColor(1, strip.Color(100, 0, 0)); // Solid red
        return;
      }
    }
    Serial.println("MySQL connection successful.");
    setLEDColor(1, strip.Color(0, 0, 100)); // Solid blue
  } else {
    Serial.println("MySQL already connected.");
    setLEDColor(1, strip.Color(0, 0, 100)); // Solid blue
  }
}


void setLEDColor(int ledIndex, uint32_t color) {
  strip.setPixelColor(ledIndex, color);
  strip.show();
}

void setLEDColorAll(uint32_t color) {
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

void blinkLED(int ledIndex, uint32_t color, int duration) {
  setLEDColor(ledIndex, color);
  delay(duration);
  setLEDColor(ledIndex, 0); // Turn off the LED
  delay(duration);
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