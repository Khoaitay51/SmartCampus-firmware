#!/bin/bash
# Script mở Serial Monitor từ WSL2 ra cổng COM của Windows
PORT=${1:-COM6}
BAUD=${2:-115200}

echo "=========================================================="
echo "📺 SmartCampus Serial Monitor (WSL2 -> $PORT @ $BAUD)"
echo "Nhấn Ctrl+] để thoát Serial Monitor"
echo "=========================================================="

powershell.exe -Command "python -m serial.tools.miniterm $PORT $BAUD"
