@echo off
title SmartCampus ESP32 Flasher
echo ===================================================
echo   SmartCampus ESP32 Firmware Flasher ^& Monitor
echo ===================================================
echo Flashing binaries to COM6...
python -m esptool --chip esp32 -p COM6 -b 460800 --before default-reset --after hard-reset write-flash --flash-mode dio --flash-size 2MB --flash-freq 40m 0x1000 "%~dp0build\bootloader\bootloader.bin" 0x8000 "%~dp0build\partition_table\partition-table.bin" 0x10000 "%~dp0build\SmartCampus-Firmware.bin"
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Flashing failed! Check COM port connection.
    pause
    exit /b %ERRORLEVEL%
)
echo.
echo [SUCCESS] Flashing complete! Starting serial monitor (Press Ctrl+] or Ctrl+C to exit)...
echo ---------------------------------------------------
python -m serial.tools.miniterm COM6 115200 --raw
pause
