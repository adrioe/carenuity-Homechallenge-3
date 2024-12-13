//Password for Setup-Interface: 12345678

#include <FS.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <WebServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <Wire.h>
#include <Preferences.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Preferences prefs;
WebServer server(80);
String ssid = "";
String password = "";

const String baseApiUrl = "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=usd";

void setup() {
  Serial.begin(115200);
  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    for (;;);
  }

  display.clearDisplay();
  display.display();

  prefs.begin("wifi-config", false);
  ssid = prefs.getString("ssid", "");
  password = prefs.getString("password", "");

  if (ssid == "" || password == "") {
    WiFi.softAP("ESP32-Setup", "12345678");
    IPAddress ip = WiFi.softAPIP();

    setupWebServer();
    server.begin();

    display.setTextSize(1);
    display.setTextColor(WHITE);

    int offset = 0;
    while (true) {
      display.clearDisplay();
      display.setCursor(35, 20);
      display.println("Connect to:");
      display.println("ESP32-Setup");
      display.setCursor(35 - offset, 40);
      display.print("IP: ");
      display.print(ip.toString());
      display.display();

      offset = (offset + 1) % (ip.toString().length() * 6 + SCREEN_WIDTH - 35);
      delay(100);

      server.handleClient();
    }
  } else {
    WiFi.begin(ssid.c_str(), password.c_str());

    display.clearDisplay();
    display.setCursor(35, 20);
    display.println("Connecting to WiFi...");
    display.display();

    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Connecting...");
    }

    display.clearDisplay();
    display.setCursor(35, 20);
    display.println("WiFi connected");
    display.setCursor(35, 40);
    display.print(WiFi.localIP());
    display.display();
  }
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    String html = "<html><body><h2>WiFi Configuration</h2>";
    html += "<form method='POST' action='/saveConfig'>";
    html += "SSID: <input type='text' name='ssid'><br>";
    html += "Password: <input type='password' name='password'><br>";
    html += "<input type='submit' value='Save Configuration'>";
    html += "</form></body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/saveConfig", HTTP_POST, []() {
    if (server.hasArg("ssid") && server.hasArg("password")) {
      ssid = server.arg("ssid");
      password = server.arg("password");

      prefs.putString("ssid", ssid);
      prefs.putString("password", password);

      String message = "<html><body><h2>Configuration Saved!</h2><p>Device will restart now.</p></body></html>";
      server.send(200, "text/html", message);

      delay(2000);
      ESP.restart();
    } else {
      server.send(400, "text/html", "Invalid data received.");
    }
  });
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String bitcoinPrice = "Loading...";

    http.begin(baseApiUrl);
    int httpResponseCode = http.GET();

    if (httpResponseCode == 200) {
      String response = http.getString();
      bitcoinPrice = extractBitcoinPrice(response);
    } else {
      bitcoinPrice = "Error: " + String(httpResponseCode);
    }

    http.end();

    int offset = 0;
    int textLength = ("USD: " + bitcoinPrice).length() * 6;

    while (true) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);

      display.setCursor(35, 20);
      display.println("Bitcoin"); 
      display.setCursor(35, 30);
      display.println("Price:");
      display.setCursor(35 - offset, 40);
      display.print("USD: ");
      display.print(bitcoinPrice);

      display.display();
      offset = (offset + 1) % (textLength + 50 - 35);
      if (offset == 0) delay(300); // Short delay when restarting scroll
      delay(50); // Faster scroll speed
    }
  }
}

String extractBitcoinPrice(String response) {
  int priceIndex = response.indexOf("\"usd\":");
  if (priceIndex != -1) {
    int start = priceIndex + 6;
    int end = response.indexOf(',', start);
    if (end == -1) {
      end = response.indexOf('}', start);
    }
    if (end != -1) {
      return response.substring(start, end);
    }
  }
  return "Error parsing data";
}

