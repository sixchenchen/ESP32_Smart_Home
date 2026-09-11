@echo off
REM ============================================================
REM  ESP32 Smart Home - One-command OTA from ceiling device
REM  Usage: ota.bat 1.0.10
REM  Preconditions:
REM    - ESP-IDF v6.0.2 PowerShell / cmd environment
REM    - Device on same LAN, HTTP server running on device
REM    - Computer Python available (for static file server)
REM ============================================================

setlocal enabledelayedexpansion

REM --- Config (change these!) ---
set DEVICE_IP=192.168.124.7
set HTTP_HOST=192.168.124.6
set HTTP_PORT=8000
set FIRMWARE_URL=http://%HTTP_HOST%:%HTTP_PORT%/sample_project.bin

if "%1"=="" (
    echo Usage: ota.bat ^<new_version^>
    echo Example: ota.bat 1.0.10
    exit /b 1
)
set NEW_VERSION=%1

echo ============================================================
echo  ESP32 OTA Upgrade
echo  Target version : %NEW_VERSION%
echo  Device IP      : %DEVICE_IP%
echo  Firmware URL   : %FIRMWARE_URL%
echo ============================================================
echo.

REM Step 1: bump version in CMakeLists.txt
echo [1/4] Setting version to %NEW_VERSION% ...
powershell -Command "(Get-Content CMakeLists.txt) -replace 'project\(sample_project VERSION [0-9.]+\)', 'project(sample_project VERSION %NEW_VERSION%)' | Set-Content CMakeLists.txt"
if errorlevel 1 (
    echo FAILED to edit CMakeLists.txt
    exit /b 1
)

REM Step 2: build
echo [2/4] Building ...
call cmake --build build
if errorlevel 1 (
    echo FAILED to build
    exit /b 1
)

REM Step 3: ensure HTTP server running (后台启, 如果已在运行会有端口冲突但没关系)
echo [3/4] Starting HTTP file server on port %HTTP_PORT% ...
cd build
start "ESP32 Firmware Server" /B python -m http.server %HTTP_PORT% --bind 0.0.0.0 >nul 2>&1
cd ..

REM wait a bit for server
ping -n 3 127.0.0.1 >nul

REM verify server
powershell -Command "try { $r = Invoke-WebRequest -Uri '%FIRMWARE_URL%' -Method Head -TimeoutSec 2 -UseBasicParsing; Write-Host '       Server OK:' [math]::Round($r.Headers['Content-Length']/1MB, 2) 'MB' } catch { Write-Host '       Server NOT reachable - firmware URL wrong?' }"

REM Step 4: trigger OTA on device
echo [4/4] Triggering OTA on device %DEVICE_IP% ...
set OTA_URL=http://%DEVICE_IP%/api/ota
powershell -Command "$body = '{\"url\":\"%FIRMWARE_URL%\",\"version\":\"%NEW_VERSION%\"}'; try { $r = Invoke-WebRequest -Uri '%OTA_URL%' -Method POST -ContentType 'application/json' -Body $body -TimeoutSec 10 -UseBasicParsing; Write-Host '       Device response:' $($r.Content) } catch { Write-Host '       FAILED -' $_.Exception.Message }"

echo.
echo ============================================================
echo  OTA triggered! Device will download (~30s) then reboot.
echo  Monitor device status: curl http://%DEVICE_IP%/ota_status
echo  (Don't close this window - HTTP server needs to stay running)
echo ============================================================
endlocal
