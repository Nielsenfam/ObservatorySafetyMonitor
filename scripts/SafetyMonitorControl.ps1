# --- Observatory Safety Monitor & Roof Control Script ---
$EspIp = "192.168.3.158"

function Show-Menu {
    Clear-Host
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "   Observatory Safety Monitor Control   " -ForegroundColor Cyan
    Write-Host "   ESP32 Target: $EspIp                 " -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host " [1] Set System to ACTIVE (/active)" -ForegroundColor Green
    Write-Host " [2] Set System to IDLE (/idle)" -ForegroundColor DarkYellow
    Write-Host " [3] Open Roof (/open)" -ForegroundColor Green
    Write-Host " [4] Close Roof (/close)" -ForegroundColor Yellow
    Write-Host " [5] Check Status & Diagnostics" -ForegroundColor White
    Write-Host " [6] Exit" -ForegroundColor Gray
    Write-Host "----------------------------------------" -ForegroundColor Cyan
}

do {
    Show-Menu
    $choice = Read-Host "Select an option (1-6)"
    
    switch ($choice) {
        '1' {
            Write-Host "`n[COMMAND] Setting system to ACTIVE mode..." -ForegroundColor Green
            try {
                $response = Invoke-RestMethod -Uri "http://$EspIp/active" -Method Get -TimeoutSec 5
                Write-Host "Result: $response" -ForegroundColor Green
            } catch {
                Write-Error "Failed to communicate with ESP32: $_"
            }
        }
        '2' {
            Write-Host "`n[COMMAND] Setting system to IDLE state..." -ForegroundColor DarkYellow
            try {
                $response = Invoke-RestMethod -Uri "http://$EspIp/idle" -Method Get -TimeoutSec 5
                Write-Host "Result: $response" -ForegroundColor DarkYellow
            } catch {
                Write-Error "Failed to communicate with ESP32: $_"
            }
        }
        '3' {
            Write-Host "`n[COMMAND] Sending OPEN command to ESP32..." -ForegroundColor Green
            try {
                $response = Invoke-RestMethod -Uri "http://$EspIp/open" -Method Get -TimeoutSec 5
                Write-Host "Result: $response" -ForegroundColor Green
            } catch {
                Write-Error "Failed to communicate with ESP32: $_"
            }
        }
        '4' {
            Write-Host "`n[COMMAND] Sending CLOSE command to ESP32..." -ForegroundColor Yellow
            try {
                $response = Invoke-RestMethod -Uri "http://$EspIp/close" -Method Get -TimeoutSec 5
                Write-Host "Result: $response" -ForegroundColor Yellow
            } catch {
                Write-Error "Failed to communicate with ESP32: $_"
            }
        }
        '5' {
            Write-Host "`n[STATUS] Querying current ESP32 system parameters..." -ForegroundColor Cyan
            try {
                $status = Invoke-RestMethod -Uri "http://$EspIp/status" -Method Get -TimeoutSec 5
                Write-Host "----------------------------------------" -ForegroundColor DarkGray
                Write-Host "  System State     : $($status.state)" -ForegroundColor White
                Write-Host "  Emergency Active : $($status.emergency)" -ForegroundColor $(if($status.emergency){"Red"}else{"Green"})
                Write-Host "  Emergency Reason : $($status.emergency_reason)" -ForegroundColor Yellow
                Write-Host "  Roof Closed      : $($status.roof_closed)" -ForegroundColor White
                Write-Host "  Rain Sensor      : $($status.rain_sensor)" -ForegroundColor White
                Write-Host "  AC Power Detect  : $($status.ac_detect)" -ForegroundColor White
                Write-Host "----------------------------------------" -ForegroundColor DarkGray
            } catch {
                Write-Error "Failed to fetch status: $_"
            }
        }
        '6' {
            Write-Host "Exiting control panel..." -ForegroundColor Gray
        }
        default {
            Write-Warning "Invalid selection. Please choose an option from 1 to 6."
        }
    }

    if ($choice -ne '6') {
        Write-Host "`nPress any key to return to the menu..." -ForegroundColor DarkGray
        $null =$Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
    }
} while ($choice -ne '6')
