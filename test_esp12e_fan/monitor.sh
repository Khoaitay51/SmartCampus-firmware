#!/bin/bash
# ==============================================================================
# Script mở Serial Monitor ESP-12E từ WSL2
# ==============================================================================
PORT_ARG="$1"
BAUD=${2:-115200}

if [ -n "$PORT_ARG" ]; then
    PORT="$PORT_ARG"
else
    DETECTED_PORT=$(powershell.exe -Command "((Get-CimInstance Win32_PnPEntity | Where-Object { \$_.Name -match 'CH340|CP210|FTDI|UART' -and \$_.Name -match 'COM\d+' } | ForEach-Object { if (\$_.Name -match '(COM\d+)') { \$matches[1] } }) | Select-Object -First 1)" 2>/dev/null | tr -d '\r\n')
    if [ -n "$DETECTED_PORT" ]; then
        PORT="$DETECTED_PORT"
    else
        PORT="COM8"
    fi
fi

echo "=========================================================="
echo "📺 ESP-12E Serial Monitor (WSL2 -> $PORT @ $BAUD)"
echo "Nhấn Ctrl+] để thoát Serial Monitor"
echo "=========================================================="

powershell.exe -Command "python -m serial.tools.miniterm $PORT $BAUD"
