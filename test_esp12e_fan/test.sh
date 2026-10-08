#!/bin/bash
# ==============================================================================
# Script chạy công cụ kiểm tra quạt qua Serial từ WSL2
# ==============================================================================
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PORT_ARG="$1"

if [ -n "$PORT_ARG" ]; then
    PORT="$PORT_ARG"
else
    # Tự động dò cổng COM của bo mạch
    DETECTED_PORT=$(powershell.exe -Command "((Get-CimInstance Win32_PnPEntity | Where-Object { \$_.Name -match 'CH340|CP210|FTDI|UART' -and \$_.Name -match 'COM\d+' } | ForEach-Object { if (\$_.Name -match '(COM\d+)') { \$matches[1] } }) | Select-Object -First 1)" 2>/dev/null | tr -d '\r\n')
    if [ -n "$DETECTED_PORT" ]; then
        PORT="$DETECTED_PORT"
    else
        PORT="COM8"
    fi
fi

mkdir -p /mnt/c/temp/esp8266
cp "$SCRIPT_DIR/test_serial_fan.py" /mnt/c/temp/esp8266/test_serial_fan.py

echo "=========================================================="
echo "⚡ Khởi động công cụ kiểm tra quạt ESP-12E ($PORT)"
echo "Nhấn Ctrl+C để thoát"
echo "=========================================================="

powershell.exe -Command "python C:\temp\esp8266\test_serial_fan.py $PORT"
