#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>

// --- Network Credentials ---
const char* ssid = "WIFI_SSID";
const char* password = "WIFI_PASSWORD";

// --- Static IP Configuration ---
IPAddress local_IP(192, 168, 50, 153);   
IPAddress gateway(192, 168, 50, 1);      
IPAddress subnet(255, 255, 255, 0);     
IPAddress primaryDNS(8, 8, 8, 8);       
IPAddress secondaryDNS(8, 8, 4, 4);     

// --- Weather Underground Credentials ---
const String WU_API_KEY = "WU_API_KEY";
String stationIds[] = {"STAID1", "STAID2"}; 
const int numStations = 2;
unsigned long lastWUCheck = 0;
const unsigned long WU_CHECK_INTERVAL = 600000; // 10 minutes

// --- External Internet Connectivity Check Variables ---
unsigned long lastInternetCheck = 0;
const unsigned long INTERNET_CHECK_INTERVAL = 120000; // 2 minutes
int internetFailureCount = 0;
const int MAX_INTERNET_FAILURES = 2;                  

// --- Pin Definitions (ESP32-S3 Layout) ---
const int PIN_RELAY        = 21; // Triggers gate opener cycle
const int PIN_RAIN         = 4;  // Hardwired optical rain sensor (LOW = Rain)
const int PIN_AC_DETECT    = 16; // AC Power loss detector (LOW = Power Lost)
const int PIN_ROOF_OPEN    = 18; // Reed switch: Roof fully open (Kept for future use)
const int PIN_ROOF_CLOSED  = 19; // Reed switch: Roof fully closed (LOW = Magnet active)

// --- Timing & Watchdog Variables ---
unsigned long lastHeartbeat = 0;
const unsigned long HEARTBEAT_TIMEOUT = 300000; // 5 minutes
bool networkConnected = false;
bool heartbeatEnabled = true; // Toggle for visual observation mode

// --- System Operating State (Powers up in IDLE) ---
bool systemActive = false; 

// --- Emergency & Abort State Variables ---
bool emergencyTriggered = false;
bool roofCloseAborted = false;
int closeAttemptCount = 0;
const int MAX_CLOSE_ATTEMPTS = 5;
bool wuRainActive = false;      // Persistent state flag for Weather Underground stations
bool ecowittRainActive = false; // Persistent state flag for local Ecowitt station pushes
String emergencyReason = "None"; // Tracks the cause of the emergency

// --- Thresholds ---
const float ECOWITT_RAIN_THRESHOLD = 0.1; 

WebServer server(80);

// Forward Declarations
void triggerCloseRoof(String reason);
void checkMultipleWeatherStations();
bool checkInternetConnection();
bool areAllConditionsSafe();
void setLedStatus(uint8_t r, uint8_t g, uint8_t b);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize Onboard RGB LED (Native to ESP32-S3 DevKits)
  #ifndef RGB_BUILTIN
  #define RGB_BUILTIN 48 
  #endif
  
  #ifdef RGB_PWR
  pinMode(RGB_PWR, OUTPUT);
  digitalWrite(RGB_PWR, HIGH);
  #endif

  setLedStatus(0, 0, 50); // Blue: Booting / Connecting

  Serial.println("\n\n==========================================");
  Serial.println("   ESP32 Observatory Safety Monitor Boot  ");
  Serial.println("==========================================");
  Serial.println("[INFO] System booted into IDLE state.");

  // Initialize Pins
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, LOW);
  
  pinMode(PIN_RAIN, INPUT_PULLUP);
  pinMode(PIN_AC_DETECT, INPUT_PULLDOWN); 
  pinMode(PIN_ROOF_OPEN, INPUT_PULLUP);
  pinMode(PIN_ROOF_CLOSED, INPUT_PULLUP);

  // Configure Static IP
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("[ERROR] Failed to configure Static IP!");
  }

  // Connect to Wi-Fi
  Serial.print("Connecting to Wi-Fi: " + String(ssid));
  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    networkConnected = true;
    lastHeartbeat = millis();
    Serial.println("[INFO] Wi-Fi Connected Successfully!");
    Serial.println("[INFO] Static IP Address: " + WiFi.localIP().toString());
    
    if (!MDNS.begin("safetymonitor")) {
      Serial.println("[ERROR] Error setting up MDNS responder!");
    } else {
      Serial.println("[INFO] mDNS responder started: http://safetymonitor.local");
    }

    setLedStatus(20, 10, 0); // Dim Amber/Yellow: Powered on and Idle
  } else {
    Serial.println("[WARNING] Wi-Fi Connection Failed during setup. Restarting ESP32 in 1 min.");
    setLedStatus(40, 40, 0); // Yellow/Amber: Offline Warning
    delay(60000);
    ESP.restart();
  }

  // --- Web Server Endpoints ---
  
  // 1. Power State & System Control Endpoints
  server.on("/active", HTTP_GET, []() {
    systemActive = true;
    lastHeartbeat = millis();
    Serial.println("[CONTROL] System switched to ACTIVE monitoring mode via web command.");
    setLedStatus(0, 40, 0); // Green: Active & Normal
    server.send(200, "text/plain", "System set to ACTIVE");
  });

  server.on("/idle", HTTP_GET, []() {
    systemActive = false;
    emergencyTriggered = false;
    roofCloseAborted = false;
    emergencyReason = "None"; // Clear emergency reason on idle
    Serial.println("[CONTROL] System switched to IDLE state via web command.");
    setLedStatus(20, 10, 0); // Dim Amber: Idle
    server.send(200, "text/plain", "System set to IDLE");
  });

  server.on("/restart", HTTP_GET, []() {
    Serial.println("[CONTROL] RESTART command received via web server. Rebooting ESP32...");
    server.send(200, "text/plain", "Restarting ESP32 now...");
    delay(200);
    ESP.restart();
  });

  // 2. Status Inquiry Endpoint (Includes emergency reason tracking)
  server.on("/status", HTTP_GET, []() {
    JsonDocument doc;
    doc["state"] = systemActive ? "active" : "idle";
    doc["emergency"] = emergencyTriggered;
    doc["emergency_reason"] = emergencyReason; // <--- Tracks cause of emergency
    doc["aborted"] = roofCloseAborted;
    doc["roof_closed"] = (digitalRead(PIN_ROOF_CLOSED) == LOW);
    doc["rain_sensor"] = (digitalRead(PIN_RAIN) == LOW) ? "wet" : "dry";
    doc["ac_detect"] = (digitalRead(PIN_AC_DETECT) == LOW) ? "lost" : "normal";
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  });

  // 3. Control & Utility Endpoints
  server.on("/ping", HTTP_GET, []() {
    lastHeartbeat = millis();
    Serial.println("[PING] Heartbeat received from Observatory PC. Timer reset.");
    server.send(200, "text/plain", "OK");
  });
  
  // Option B: /close requires active system mode
  server.on("/close", HTTP_GET, []() {
    if (!systemActive) {
      server.send(400, "text/plain", "Error: System is IDLE. Switch to /active first.");
      return;
    }
    Serial.println("[COMMAND] Manual roof close endpoint triggered via web request.");
    triggerCloseRoof("Manual Web Endpoint (/close)");
    server.send(200, "text/plain", "Roof Close Triggered");
  });

  // Fully functional /open endpoint
  server.on("/open", HTTP_GET, []() {
    if (!systemActive) {
      server.send(400, "text/plain", "Error: System is IDLE. Switch to /active first.");
      return;
    }

    // Verify roof is currently closed before attempting to open (LOW = Closed)
    bool isClosed = (digitalRead(PIN_ROOF_CLOSED) == LOW);

    if (isClosed) {
      Serial.println("[COMMAND] Manual roof open endpoint triggered. Pulsing relay...");
      
      // Pulse relay for 1 second
      digitalWrite(PIN_RELAY, HIGH);
      delay(1000); 
      digitalWrite(PIN_RELAY, LOW);

      emergencyTriggered = false; // Clear active emergency state
      emergencyReason = "None";     // Reset emergency reason string
      
      server.send(200, "text/plain", "OK: Roof open command sent.");
    } else {
      server.send(200, "text/plain", "Notice: Roof is already open or reed switch reports open state.");
    }
  });

  server.on("/heartbeat/off", HTTP_GET, []() {
    heartbeatEnabled = false;
    Serial.println("[CONTROL] PC Heartbeat check DISABLED via web command.");
    server.send(200, "text/plain", "Heartbeat Check Disabled");
  });

  server.on("/heartbeat/on", HTTP_GET, []() {
    heartbeatEnabled = true;
    lastHeartbeat = millis();
    Serial.println("[CONTROL] PC Heartbeat check ENABLED via web command.");
    server.send(200, "text/plain", "Heartbeat Check Enabled");
  });

  // 4. Ecowitt Handler
  server.on("/ecowitt", HTTP_POST, []() {
    if (!systemActive) {
      Serial.println("[ECOWITT] Weather push ignored because system is in IDLE state.");
      server.send(200, "text/plain", "OK (Ignored - Idle)");
      return;
    }

    Serial.println("\n[ECOWITT] Local weather push received via HTTP POST.");
    
    int argsCount = server.args();
    for (int i = 0; i < argsCount; i++) {
      Serial.println("  Arg [" + server.argName(i) + "] = " + server.arg(i));
    }

    float rainRate = -1.0;

    if (server.hasArg("rainratein")) {
      rainRate = server.arg("rainratein").toFloat();
    } else if (server.hasArg("rainrate")) {
      rainRate = server.arg("rainrate").toFloat();
    } else if (server.hasArg("rratein")) {
      rainRate = server.arg("rratein").toFloat();
    } else if (server.hasArg("eventrainin")) {
      rainRate = server.arg("eventrainin").toFloat();
    }

    if (rainRate < 0.0 && server.hasArg("plain")) {
      String body = server.arg("plain");
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, body);
      if (!error) {
        if (doc["rainratein"].is<float>()) rainRate = doc["rainratein"];
        else if (doc["rainrate"].is<float>()) rainRate = doc["rainrate"];
        else if (doc["eventrainin"].is<float>()) rainRate = doc["eventrainin"];
      }
    }

    if (rainRate >= 0.0) {
      Serial.println("[ECOWITT] Parsed Rain Rate: " + String(rainRate) + " in/hr");
      if (rainRate > ECOWITT_RAIN_THRESHOLD) {
        ecowittRainActive = true;
        Serial.println("[ALERT] Ecowitt rain rate exceeds threshold! Triggering emergency closure.");
        triggerCloseRoof("Ecowitt Rain Rate Threshold Exceeded (" + String(rainRate) + " in/hr)");
      } else {
        if (ecowittRainActive) {
          Serial.println("[ECOWITT] Rain rate dropped below threshold. Ecowitt rain condition cleared.");
        }
        ecowittRainActive = false;
      }
    } else {
      Serial.println("[WARNING] Ecowitt push received, but no recognized rain rate key found!");
      if (server.hasArg("test") || argsCount == 0) {
        ecowittRainActive = true;
        Serial.println("[ALERT] Ecowitt test payload detected. Forcing emergency close for safety.");
        triggerCloseRoof("Ecowitt Test Push Trigger");
      }
    }

    server.send(200, "text/plain", "OK");
  });

  server.begin();
  Serial.println("[INFO] HTTP Web Server started on port 80.");
  Serial.println("==========================================\n");
}

void loop() {
  server.handleClient();

  // --- IDLE STATE BYPASS ---
  if (!systemActive) {
    setLedStatus(20, 10, 0); // Keep LED Dim Amber while idling
    delay(200);
    return; // Skip all background polling and safety checks when idle
  }

  // --- AUTO-RESET WHEN CONDITIONS RETURN TO SAFE ---
  if ((emergencyTriggered || roofCloseAborted) && areAllConditionsSafe()) {
    Serial.println("\n[INFO] All error conditions have returned to safe. System fully reset to normal monitoring.");
    emergencyTriggered = false;
    roofCloseAborted = false;
    closeAttemptCount = 0;
    emergencyReason = "None"; // Clear reason on auto-reset
    setLedStatus(0, 40, 0); // Green: Normal Operation
  }

  // --- ABORTED STATE HANDLING ---
  if (roofCloseAborted) {
    setLedStatus(50, 0, 50); // Magenta/Purple: Fatal Abort State
    delay(1000);
    return;
  }

  // --- EMERGENCY CLOSING LOOP ---
  if (emergencyTriggered) {
    int roofClosedState = digitalRead(PIN_ROOF_CLOSED);
    
    if (roofClosedState == LOW) {
      closeAttemptCount = 0; 
      setLedStatus(0, 0, 40); // Dim Blue for Secure/Closed
    } else {
      if (closeAttemptCount < MAX_CLOSE_ATTEMPTS) {
        closeAttemptCount++;
        setLedStatus(50, 0, 0); // Solid Red during closure attempt
        
        Serial.println("\n******************************************");
        Serial.println("[ACTION] Closing Roof - Attempt " + String(closeAttemptCount) + " of " + String(MAX_CLOSE_ATTEMPTS));
        Serial.println("[ACTION] Pulsing relay (PIN_RELAY HIGH) for 1 second...");
        
        digitalWrite(PIN_RELAY, HIGH);
        delay(1000);
        digitalWrite(PIN_RELAY, LOW);
        
        Serial.println("[ACTION] Relay released. Entering 22-second motion cooldown.");
        delay(22000); 
      } else {
        roofCloseAborted = true;
        Serial.println("\n******************************************");
        Serial.println("[FATAL ABORT] Maximum close attempts (" + String(MAX_CLOSE_ATTEMPTS) + ") reached!");
        Serial.println("[FATAL ABORT] Roof failed to close. Halting automated closure attempts to protect motor.");
        Serial.println("******************************************\n");
      }
      return; 
    }
  }

  // 1. Check Wi-Fi Local Association & Optional PC Heartbeat Timeout
  if (networkConnected) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[ALERT] Local Wi-Fi connection lost!");
      triggerCloseRoof("Local Wi-Fi Connection Lost");
      networkConnected = false;
    } else if (heartbeatEnabled) {
      unsigned long timeSinceHeartbeat = millis() - lastHeartbeat;
      if (timeSinceHeartbeat > HEARTBEAT_TIMEOUT) {
        Serial.println("[ALERT] Heartbeat timeout reached! No ping for " + String(timeSinceHeartbeat / 1000) + " seconds.");
        triggerCloseRoof("Observatory PC Heartbeat Timeout");
      }
    }
  }

  // 2. Check External Internet Connectivity via Unreliable Downlink
  if (networkConnected && (millis() - lastInternetCheck > INTERNET_CHECK_INTERVAL)) {
    lastInternetCheck = millis();
    
    if (!checkInternetConnection()) {
      internetFailureCount++;
      Serial.println("[WARNING] External internet check failed! Consecutive failures: " + String(internetFailureCount) + "/" + String(MAX_INTERNET_FAILURES));
      setLedStatus(50, 30, 0); // Amber warning indicator
      
      if (internetFailureCount >= MAX_INTERNET_FAILURES) {
        Serial.println("[ALERT] Maximum internet connectivity failures reached!");
        triggerCloseRoof("External Internet Connectivity Lost (Remote Downlink Down)");
      }
    } else {
      if (internetFailureCount > 0) {
        Serial.println("[INFO] External internet connectivity restored.");
      }
      internetFailureCount = 0; 
      setLedStatus(0, 40, 0); // Back to Green
    }
  }

  // 3. Check AC Mains Power Loss
  if (digitalRead(PIN_AC_DETECT) == LOW) {
    Serial.println("[ALERT] AC Mains Power Loss detected on PIN_AC_DETECT!");
    triggerCloseRoof("AC Mains Power Loss");
  }

  // 4. Check Hardwired Optical Rain Sensor
  if (digitalRead(PIN_RAIN) == LOW) {
    Serial.println("[ALERT] Hardwired Optical Rain Sensor triggered (LOW)!");
    triggerCloseRoof("Hardwired Optical Rain Sensor");
  }

  // 5. Check Weather Underground Nearby Stations
  checkMultipleWeatherStations();

  delay(500);
}

bool areAllConditionsSafe() {
  bool rainSafe = (digitalRead(PIN_RAIN) == HIGH);
  bool acSafe = (digitalRead(PIN_AC_DETECT) == HIGH);
  bool heartbeatSafe = !heartbeatEnabled || (millis() - lastHeartbeat <= HEARTBEAT_TIMEOUT);
  bool internetSafe = (internetFailureCount == 0);
  bool wuSafe = !wuRainActive;
  bool ecowittSafe = !ecowittRainActive;
  
  return rainSafe && acSafe && heartbeatSafe && internetSafe && wuSafe && ecowittSafe;
}

void setLedStatus(uint8_t r, uint8_t g, uint8_t b) {
  #ifdef RGB_BUILTIN
  rgbLedWrite(RGB_BUILTIN, r, g, b);
  #endif
}

bool checkInternetConnection() {
  Serial.println("[NET] Checking external internet connectivity...");
  
  WiFiClientSecure secureClient;
  secureClient.setInsecure(); 

  HTTPClient http;
  http.begin(secureClient, "https://www.google.com");
  http.setTimeout(4000); 
  
  int httpCode = http.GET();
  http.end();

  if (httpCode > 0) {
    Serial.println("[NET] Internet check passed (HTTP Code: " + String(httpCode) + ")");
    return true;
  } else {
    Serial.println("[NET] Internet check failed, error: " + http.errorToString(httpCode));
    return false;
  }
}

void checkMultipleWeatherStations() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (millis() - lastWUCheck < WU_CHECK_INTERVAL) return;
  lastWUCheck = millis();

  Serial.println("\n[WU] Starting scheduled check of nearby Weather Underground stations...");

  WiFiClientSecure secureClient;
  secureClient.setInsecure(); 

  HTTPClient http;
  bool precipitationDetectedAnywhere = false;

  for (int i = 0; i < numStations; i++) {
    String url = "https://api.weather.com/v2/pws/observations/current?stationId=" + stationIds[i] + 
                 "&format=json&units=e&apiKey=" + WU_API_KEY;

    http.begin(secureClient, url);
    http.setTimeout(5000);
    int httpCode = http.GET();

    Serial.println("[WU] Querying station: " + stationIds[i] + " | HTTP Code: " + String(httpCode));

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        float precipRate = doc["observations"][0]["imperial"]["precipRate"];
        Serial.println("[WU] Station " + stationIds[i] + " Precip Rate: " + String(precipRate) + " in/hr");
        
        if (precipRate > 0.0) {
          precipitationDetectedAnywhere = true;
          Serial.println("[ALERT] Precipitation detected at neighboring station " + stationIds[i] + "!");
        }
      } else {
        Serial.println("[ERROR] Failed to parse JSON for station " + stationIds[i] + ": " + String(error.c_str()));
      }
    } else {
      Serial.println("[ERROR] HTTP request failed for station " + stationIds[i] + ", error: " + http.errorToString(httpCode));
    }
    http.end();
  }

  if (precipitationDetectedAnywhere) {
    wuRainActive = true;
    triggerCloseRoof("Weather Underground Station(s) reported precipitation");
  } else {
    if (wuRainActive) {
      Serial.println("[WU] All monitored weather stations now report 0.0 precip. Remote rain condition cleared.");
    }
    wuRainActive = false;
  }
  
  Serial.println("[WU] Stations check complete. WU Rain Active Flag: " + String(wuRainActive ? "TRUE" : "FALSE") + "\n");
}

void triggerCloseRoof(String reason) {
  if (roofCloseAborted) return; 
  
  if (!emergencyTriggered) {
    Serial.println("\n******************************************");
    Serial.println("[SAFETY TRIGGER] EMERGENCY DETECTED!");
    Serial.println("[REASON]: " + reason);
    Serial.println("******************************************");
    emergencyTriggered = true;
    emergencyReason = reason; // Store reason string globally
    closeAttemptCount = 0; 
  }
}