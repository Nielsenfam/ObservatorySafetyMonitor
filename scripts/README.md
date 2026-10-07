# Observatory Safety Monitor & Roof Control

A collection of PowerShell automation and management scripts designed to interface with an ESP32-based observatory safety monitor and motorized roof controller over a local network.

Run these two scripts on the main observing computer. In my observatory there is a mele quiter 4c mini PC connected to each scope where the controlling software NINA is running. I have these two scripts autostart on login to this PC in a new window to give me quick access to controlling the roof and safety monitor as well as immediate feedback on status.  

---

## Scripts Overview

### 1. Interactive Control Panel (`control-panel.ps1`)
An interactive, menu-driven CLI utility that allows you to manually send commands to your ESP32 controller and query live diagnostic metrics.

* **Features:**
  * **System Mode Control:** Toggle the system state between `ACTIVE` (`/active`) and `IDLE` (`/idle`).
  * **Roof Automation:** Manually trigger open (`/open`) and close (`/close`) sequences.
  * **Live Diagnostics:** Query the full status endpoint (`/status`) to view system state, emergency triggers, and hardware sensor readings.
  * **Visual Feedback:** Color-coded terminal interface for quick status recognition (e.g., green for active/safe, red/yellow for alerts).

### 2. Status Poller & Heartbeat Client (`heartbeat-client.ps1`)
A background monitoring script that runs continuously to poll system status and provide a periodic heartbeat link to keep the ESP32 connection alive.

* **Features:**
  * **Periodic Polling:** Queries the ESP32 status endpoint every 10 seconds (configurable).
  * **Heartbeat Mechanism:** Sends a keep-alive ping to the ESP32 every 60 seconds.
  * **Emergency & Fault Detection:** Automatically flags and highlights critical system warnings such as active emergencies, failure reasons, or max-retry fatal aborts.

---

## Configuration

Both scripts target a default ESP32 IP address of `192.168.3.158`. If your ESP32 has a different IP address assigned on your local network, update the `$EspIp` variable at the top of either script:

```powershell
$EspIp = "192.168.3.158"
```

---

## Requirements

* **Operating System:** Windows PowerShell 5.1+ or PowerShell Core 6+ / 7+.
* **Network:** Local network connectivity between your PC running the script and the ESP32 hardware.

---

## Usage

1. Open PowerShell.
2. Navigate to the directory containing the scripts.
3. Run the desired script:
   * **For manual operation:** `./[scriptname.ps1]`
