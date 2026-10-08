# ESP32 Observatory Safety Monitor

An automated safety monitoring system built on an **ESP32-S3** microcontroller. Designed specifically for astronomical observatories, this firmware monitors local weather sensors, AC power, internet connectivity, and a host PC heartbeat to automatically trigger roof closure in emergency scenarios.

---

## 🌟 Key Features

* **Multi-Layered Safety Triggering:** Automatically commands roof closure via relay pulse under several dangerous conditions:
    * Hardwired Optical Rain Sensor activation.
    * AC Mains Power Loss detection.
    * Observatory PC Heartbeat timeout (monitors connection with telescope control PC typically running NINA).
    * External internet connection failure (fallback for network degradation).
    * Precipitation detected via local **Ecowitt** weather station HTTP POST pushes.
    * Precipitation reported by nearby **Weather Underground (WU)** PWS stations via API polling.


* **Web Control Interface & API:** Built-in web server providing endpoints for status monitoring (`/status`), manual overrides (`/open`, `/close`), system state toggling (`/active`, `/idle`), and heartbeats (`/ping`).
    * **mDNS Support:** Easily access your device on your local network via `[http://safetymonitor.local/cmd]`.
    * **State Machine & Fault Protection:**
    * Features an **IDLE/ACTIVE** state system to prevent unwanted closures during maintenance.
    * Automatically attempts up to 5 closure pulses before triggering a **Fatal Abort** state to protect physical roof motors.
    * Automatically resets and returns to normal monitoring once weather and safety conditions clear.


* **Visual Status Feedback:** Uses the ESP32-S3's native onboard RGB LED to display system status at a glance (Booting, Idle, Active, Warning, Emergency, or Aborted).

---

## 📌 Pin Configuration (ESP32-S3 Layout)

| Pin | Function | Description |
| --- | --- | --- |
| **GPIO 21** | `PIN_RELAY` | Triggers the gate/roof opener relay cycle (Active HIGH pulse) |
| **GPIO 4** | `PIN_RAIN` | Hardwired optical rain sensor (`LOW` = Rain detected) |
| **GPIO 16** | `PIN_AC_DETECT` | AC Power loss detector (`LOW` = Power lost) |
| **GPIO 18** | `PIN_ROOF_OPEN` | Reed switch: Roof fully open (Reserved for future use) |
| **GPIO 19** | `PIN_ROOF_CLOSED` | Reed switch: Roof fully closed (`LOW` = Magnet active / Closed) |
| **GPIO 48** | `RGB_BUILTIN` | Onboard RGB LED for status indication |

---
## 💡 LED Status Indicator Reference

The ESP32-S3 onboard RGB LED provides real-time visual feedback regarding the system's operational and safety status:

| Color | State / Meaning |
| --- | --- |
| **Blue** (Solid/Dim) | Booting up / connecting to Wi-Fi, or system is secure/roof closed during an emergency. |
| **Dim Amber / Yellow** | System is powered on and currently in **IDLE** mode (background safety triggers bypassed). |
| **Yellow / Amber** (Warning) | Wi-Fi connection failure during setup, or external internet check warning (consecutive download failures). |
| **Green** | System is **ACTIVE** and operating normally under safe conditions. |
| **Solid Red** | Active emergency detected; roof closure attempt in progress. |
| **Magenta / Purple** | Fatal Abort state reached (maximum close attempts exceeded; motor halted for protection). |

---

## ⚙️ Configuration & Setup

1. **Prerequisites:**
* Install the [Arduino IDE](https://www.google.com/search?q=https://www.arduino.cc/) with support for ESP32 boards.
* Install the following required library:
* **ArduinoJson** (by Benoit Blanchon)




2. **Network Credentials:**
Open `ObservatorySafetyMonitor.ino` and update your Wi-Fi credentials and static IP configuration to match your observatory subnet:
```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

IPAddress local_IP(192, 168, 50, 153);   
IPAddress gateway(192, 168, 50, 1);      
IPAddress subnet(255, 255, 255, 0);     

```


3. **Weather Underground (Optional):**
If you wish to poll nearby Weather Underground stations, insert your API key and station IDs:
```cpp
const String WU_API_KEY = "YOUR_WU_API_KEY";
String stationIds[] = {"YOUR_STATION_ID_1", "YOUR_STATION_ID_2"}; 

```


4. **Compile and Flash:** Select your ESP32-S3 board in the Arduino IDE and flash the code.

---

## 🌐 Web Server Endpoints

Once connected to your network, you can interact with the safety monitor via its IP address or `[http://safetymonitor.local]/cmd`:

* `/status` — Returns a JSON payload outlining current system states, emergency triggers, and sensor readings.
* `/active` — Switches the system into **ACTIVE** monitoring mode.
* `/idle` — Switches the system to **IDLE** mode (bypasses background safety triggers).
* `/open` — Manually triggers the roof open sequence (requires ACTIVE mode and closed-roof verification).
* `/close` — Manually triggers an emergency roof close sequence.
* `/ping` — Resets the observatory PC heartbeat watchdog timer.

## POST Endpoints:
* `POST /ecowitt` — Endpoint designed to receive local Ecowitt weather station push updates. 
