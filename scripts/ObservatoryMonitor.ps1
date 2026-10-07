# --- Configuration ---
$EspIp = "192.168.3.158"
$StatusUrl = "http://$EspIp/status"
$PingUrl = "http://$EspIp/ping"
$PollIntervalSeconds = 10       # Check status every 10 seconds
$PingIntervalSeconds = 60       # Send heartbeat ping every 60 seconds (well under the 5-min timeout)

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host "   Observatory Safety Monitor & Heartbeat Client  " -ForegroundColor Cyan
Write-Host "   Target ESP32 IP: $EspIp" -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan

$lastPingTime = [DateTime]::MinValue

while ($true) {
    $currentTime = Get-Date

    # 1. Send Heartbeat Ping if interval has passed
    if (($currentTime - $lastPingTime).TotalSeconds -ge $PingIntervalSeconds) {
        try {
            $pingResponse = Invoke-WebRequest -Uri $PingUrl -Method Get -TimeoutSec 3 -UseBasicParsing
            $timeString = $currentTime.ToString("HH:mm:ss")
            Write-Host "[$timeString] [HEARTBEAT] Sent successful ping to ESP32." -ForegroundColor DarkCyan
            $lastPingTime = $currentTime
        }
        catch {
            $timeString = $currentTime.ToString("HH:mm:ss")
            Write-Warning "[$timeString] [HEARTBEAT FAILED] Could not reach ESP32 to send ping! Error: $_"
        }
    }

    # 2. Check Observatory Status
    try {
        $response = Invoke-RestMethod -Uri $StatusUrl -Method Get -TimeoutSec 5
        
        $timeString = $currentTime.ToString("HH:mm:ss")
        $state = $response.state
        $isEmergency = $response.emergency
        $reason = $response.emergency_reason
        $isAborted = $response.aborted
        $roofClosed = $response.roof_closed
        $rainSensor = $response.rain_sensor
        $acDetect = $response.ac_detect

        if ($isEmergency -eq $true) {
            Write-Host "[$timeString] STATUS: EMERGENCY TRIGGERED!" -ForegroundColor Red -BackgroundColor Black
            Write-Host "  -> Reason       : $reason" -ForegroundColor Yellow
            Write-Host "  -> Roof Closed  : $roofClosed" -ForegroundColor Gray
            Write-Host "  -> Rain Sensor  : $rainSensor" -ForegroundColor Gray
            Write-Host "  -> AC Power     : $acDetect" -ForegroundColor Gray
            Write-Host "--------------------------------------------------" -ForegroundColor DarkGray
        }
        elseif ($isAborted -eq $true) {
            Write-Host "[$timeString] STATUS: FATAL ABORT (Max close attempts reached)!" -ForegroundColor Magenta
            Write-Host "  -> Check motor hardware immediately!" -ForegroundColor Magenta
            Write-Host "--------------------------------------------------" -ForegroundColor DarkGray
        }
        else {
            Write-Host "[$timeString] System: $state | Safe: OK | Roof Closed: $roofClosed | Rain: $rainSensor | Power: $acDetect" -ForegroundColor Green
        }
    }
    catch {
        $timeString = $currentTime.ToString("HH:mm:ss")
        Write-Warning "[$timeString] Failed to read status from ESP32 at $StatusUrl. Error: $_"
    }

    Start-Sleep -Seconds $PollIntervalSeconds
}
