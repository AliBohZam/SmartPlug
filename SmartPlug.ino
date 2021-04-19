/***********************************************************************
Copyright (c) 2020-2021 Ali Bohloolizamani

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 ***********************************************************************/
#include <FS.h>               // https://github.com/esp8266/Arduino/blob/master/cores/esp8266/FS.h
#include <WiFiManager.h>      // https://github.com/tzapu/WiFiManager
#include <ESP8266mDNS.h>      // https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266mDNS
#include <ESP8266WebServer.h> // https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WebServer 
#include <ACS712.h>           // https://github.com/RobTillaart/ACS712

#define DEBUG

#ifdef DEBUG
#define debugbegin(x) Serial.begin(x)
#define debug(x)    Serial.println(x)
#else
#define debugbegin(x)
#define debug(x)
#endif

#define LED     D0
#define RELAY   D1
#define BUTTON  D2
#define MINILED D4
#define OFF     HIGH
#define ON      LOW

const long interval = 5000;
unsigned long debounceCnt = 0;
unsigned long previousMillis = 0;
unsigned calibrationCnt = 0;
bool calibrationDone = 0;
bool debounceFlag = 0;
bool sample = 0;
bool state = OFF;
bool writeDone = 0;

ESP8266WebServer server(80); // Create a webserver object that listens for HTTP requests on port 80
ACS712 ACS(A0, 3.3, 1023, 185); // Create a current sensor object

inline void setPlug() { // Turns the plug on/off
  digitalWrite(RELAY, state);
  digitalWrite(LED, state);
  digitalWrite(MINILED, state);
  debug("New state ");
  debug(state);
  if (state == ON) {
    calibrationDone = 0;
  }
}

bool handleFileRead(String path) { // Handles HTTP file read requests
  if (path.endsWith("/cmd/on")) {
    state = ON;
    setPlug();
    debug("Command On!");
    server.send(404, "text/plain", "Turned On!");
    return true;
  } else if (path.endsWith("/cmd/off")) {
    state = OFF;
    setPlug();
    debug("Command Off!");
    server.send(404, "text/plain", "Turned Off!");
    return true;
  } else if (SPIFFS.exists(path)) {
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, "text/plain");
    file.close();
    debug(String("\tSent file: ") + path);
    return true;
  }
  debug(String("\tFile Not Found: ") + path);
  return false;
}

void startServer() { // Starts an HTTP server with a file read handler
  server.onNotFound([]() {
    if (!handleFileRead(server.uri())) {
      server.send(404, "text/plain", "404: Not Found");
    }
  });
  server.begin();
  debug("HTTP server started");
}

void setupSpiffs() {  // Initializes the SPI Flash File System
  debug("Mounting FS...");
  if (SPIFFS.begin()) {
    debug("FS mounted");
    if (SPIFFS.exists("/current.log")) {
      SPIFFS.remove("/current.log");
    }
  } else {
    debug("Failed to mount FS!");
    ESP.restart();
    delay(5000);
  }
}

String getId() { // Returns a unique identifier made out of MAC adrs.
  String out = "";
  byte mac[6];
  WiFi.macAddress(mac);
  for (byte i = 0; i < 6; ++i) {
    char buf[3];
    sprintf(buf, "%02X", mac[i]);
    out += buf;
  }
  out += "SP";
  return out;
}

void setup() {
  debugbegin(115200);
  debug("");
  pinMode(RELAY, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(MINILED, OUTPUT);
  setPlug();
  pinMode(BUTTON, INPUT);
  WiFi.mode(WIFI_STA); // Explicitly set Wi-Fi mode, ESP defaults to STA+AP
  setupSpiffs();
  WiFiManager wm; // Local initialization - Once its magic is done, there is no need to keep it around
  if (!wm.autoConnect(/* SSID */ getId().c_str(), /* Pass */ "12345678")) { // Let the Wi-Fi manager does its magic
    debug("Failed to set up Wi-Fi!");
    ESP.restart();
    delay(5000);
  }
  digitalWrite(LED, ON);
  digitalWrite(MINILED, ON);
  debug("Wi-Fi Connected!");
  debug("Local IP");
  debug(WiFi.localIP());
  debug(WiFi.gatewayIP());
  debug(WiFi.subnetMask());
  if (!MDNS.begin(getId())) { // Start the mDNS responder for .local
    debug("Failed to set up mDNS responder!");
    ESP.restart();
    delay(5000);
  }
  debug("mDNS responder started");
  startServer();
  MDNS.addService("http", "tcp", 80);
  ACS.autoMidPoint(50); // Auto calibrates current sensor - Setting the AC voltage frequency in Hz
  digitalWrite(LED, OFF);
  digitalWrite(MINILED, OFF);
}

void loop() {
  unsigned long currentMillis = millis();
  MDNS.update();
  server.handleClient();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    sample = 1;
    if (calibrationDone == 0 && state == OFF) { // Auto calibrates current sensor once the plug has been off for a while
      if (calibrationCnt == 6) {
        ACS.autoMidPoint(50);
        calibrationDone = 1;
        calibrationCnt = 0;
      } else {
        calibrationCnt++;
      }
    }
  }
  if (debounceFlag == 1) { // Handles touch button digital signal in a responsive and sensible manner
    debounceCnt++;
    if (debounceCnt > 0x5A5A) {
      debounceFlag = 0;
      debounceCnt = 0;
    }
  } else if (digitalRead(BUTTON) == HIGH) {
    debounceFlag = 1;
    debounceCnt = 0;
    state = !state;
    setPlug();
  }
  if (sample == 1) { // Sample and log raw current sensor value - 2 bytes per sample
    int mA = ACS.mA_AC(50);
    debug("mA: " + mA);
    debug("Watt: " + String((220.0 * mA) / 1000.0));
    if (writeDone == 0) {
      File current = SPIFFS.open("/current.log", "a");
      if (current.size() == (512 * 1024)) {
        writeDone = 1;
      } else {
        current.write((byte)(mA & 0xff));
        current.write((byte)((mA & 0xff00) >> 8));
      }
      current.close();
    }
    sample = 0;
  }
}
