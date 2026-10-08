# Bill of Materials (BOM) - ESP32 Observatory Safety Monitor

This document lists the required hardware, sensors, and enclosures for building the ESP32 Observatory Safety Monitor project.

| **Item Description** | **Component / Part Type** | **Purpose / Function in Project** | **Quantity** | **Link / Reference** | 
| --- | --- | --- | --- | --- |
| **Microcontroller** | ESP32-S3 and Development Board (with Native RGB LED support), external ant | Main controller handling Wi-Fi, web server, and safety logic. | 1 | [Amazon Link](https://a.co/d/04ZsoGaS) | 
| **Enclosure / Housing** | Weatherproof Junction Box / Project Box | Protects the ESP32-S3 and electronics from outdoor weather and elements. | 1 | [Amazon Link](https://www.amazon.com/dp/B08KWFYQQR) | 
| **Rain Sensor** | Hydreon RG-11 Optical Rain Sensor | Connected to `PIN_RAIN` (GPIO 4) to instantly detect moisture and rain (`LOW` state). | 1 | [Hydreon Store](https://store.hydreon.com/shop/rain-sensor/RG-11.html) | 
| **AC Power Loss Detector** | 3.3V Power Supply serving as Power Loss Detector | Connected to `PIN_AC_DETECT` (GPIO 16); detects high-voltage AC mains to signal power loss (`LOW` when power is lost). | 1 | [Amazon Link](https://www.amazon.com/dp/B00HQ1F2OA) | 
| **Relay Module** | 1-Channel Relay (or optoisolated relay board) | Triggered via `PIN_RELAY` (GPIO 21) to cycle the gate/roof opener. | 1 | [Amazon Link](https://a.co/d/071V0OTj) | 
| **Position / Contact Sensor** | Magnetic Reed Switch / Sensor | Used for monitoring roof position (such as `PIN_ROOF_CLOSED` on GPIO 19 to detect when the roof is fully closed). | 1 | [Amazon Link](https://www.amazon.com/dp/B0F2F99Q2C) | 
| **Power Supply** | 5V DC Power Supply / USB-C Wall Adapter | 1. Powers the ESP32-S3 via USBC. 2. Powers relay module. Seperate for isolation purposes | 2 | Standard 5V PSU |
| **Power Supply** | 12V DC Power Supply / 12V Wall Adapter | Powers the RG-11 Rain Sensor. | 1 | Standard 12V PSU |
| **Weather Integration** *(Optional)* | Local WS3901 Ecowitt Weather Station & Weather Underground (WU) API | External hardware/services providing local weather telemetry and rain rate pushes via HTTP. | 1 System | [Amazon Link](https://a.co/d/04VKQMmq) | 
