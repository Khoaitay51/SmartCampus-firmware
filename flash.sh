#!/bin/bash
# Script nạp firmware ESP32 từ môi trường WSL2 ra cổng COM của Windows
set -e

PORT=${1:-COM6}
BAUD=${2:-460800}

echo "=========================================================="
echo "⚡ SmartCampus Build & Flasher (WSL2 -> Windows COM)"
echo "Cổng: $PORT | Tốc độ: $BAUD"
echo "=========================================================="

echo ">> 1. Biên dịch code..."
if [ -z "$IDF_PATH" ] && [ -f "$HOME/.espressif/v6.1/esp-idf/export.sh" ]; then
    . "$HOME/.espressif/v6.1/esp-idf/export.sh" >/dev/null 2>&1
fi
ninja -C build

mkdir -p /mnt/c/temp/esp32
cp build/bootloader/bootloader.bin /mnt/c/temp/esp32/
cp build/partition_table/partition-table.bin /mnt/c/temp/esp32/
cp build/SmartCampus-Firmware.bin /mnt/c/temp/esp32/

echo ">> 2. Đang nạp firmware vào ESP32..."
powershell.exe -Command "python -m esptool --chip esp32 -p $PORT -b $BAUD --before default-reset --after hard-reset write-flash --flash-mode dio --flash-freq 40m --flash-size detect 0x1000 C:\temp\esp32\bootloader.bin 0x8000 C:\temp\esp32\partition-table.bin 0x10000 C:\temp\esp32\SmartCampus-Firmware.bin"

echo "=========================================================="
echo "✅ NẠP FIRMWARE THÀNH CÔNG!"
echo "Để xem log bo mạch, chạy: ./monitor.sh"
echo "=========================================================="
