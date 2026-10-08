#!/bin/bash
# Script nạp firmware ESP32 từ môi trường WSL2 ra cổng COM của Windows
set -e

PORT=${1:-COM6}
ARG2=${2:-}
ARG3=${3:-}

# Mặc định tốc độ nạp ổn định 115200 (chống lỗi stub flasher / cáp kém)
BAUD=115200
ROLE=""

# Phân tích tham số thông minh:
# Cách 1: ./flash.sh 1                 -> PORT=COM6, ROLE=1
# Cách 2: ./flash.sh COM6 1            -> PORT=COM6, ROLE=1
# Cách 3: ./flash.sh COM6 115200 1     -> PORT=COM6, BAUD=115200, ROLE=1
# Cách 4: ./flash.sh                   -> PORT=COM6, giữ vai trò hiện tại
if [ "$1" = "1" ] || [ "$1" = "2" ] || [ "$1" = "3" ] || [ "$1" = "room1" ] || [ "$1" = "room2" ] || [ "$1" = "corridor" ]; then
    ROLE="$1"
    PORT=${2:-COM6}
    if [ -n "$3" ]; then
        BAUD="$3"
    fi
elif [ "$ARG2" = "1" ] || [ "$ARG2" = "2" ] || [ "$ARG2" = "3" ] || [ "$ARG2" = "room1" ] || [ "$ARG2" = "room2" ] || [ "$ARG2" = "corridor" ]; then
    ROLE="$ARG2"
    if [ -n "$ARG3" ]; then
        BAUD="$ARG3"
    fi
elif [ -n "$ARG2" ]; then
    BAUD="$ARG2"
    ROLE="$ARG3"
fi

# Tự động chuyển đổi CURRENT_NODE_ROLE trong app_config.h
if [ "$ROLE" = "1" ] || [ "$ROLE" = "room1" ] || [ "$ROLE" = "room_1" ]; then
    echo ">> [CONFIG] Chọn vai trò: ROOM NODE 1 (Phòng 1)"
    sed -i 's/#define CURRENT_NODE_ROLE.*/#define CURRENT_NODE_ROLE       ROLE_ROOM_NODE_1/' include/app_config.h
elif [ "$ROLE" = "2" ] || [ "$ROLE" = "room2" ] || [ "$ROLE" = "room_2" ]; then
    echo ">> [CONFIG] Chọn vai trò: ROOM NODE 2 (Phòng 2)"
    sed -i 's/#define CURRENT_NODE_ROLE.*/#define CURRENT_NODE_ROLE       ROLE_ROOM_NODE_2/' include/app_config.h
elif [ "$ROLE" = "3" ] || [ "$ROLE" = "corridor" ]; then
    echo ">> [CONFIG] Chọn vai trò: CORRIDOR NODE (Hành lang)"
    sed -i 's/#define CURRENT_NODE_ROLE.*/#define CURRENT_NODE_ROLE       ROLE_CORRIDOR_NODE/' include/app_config.h
fi

ACTIVE_ROLE=$(grep -m 1 "define CURRENT_NODE_ROLE" include/app_config.h | awk '{print $3}')

echo "=========================================================="
echo "⚡ SmartCampus Build & Flasher (WSL2 -> Windows COM)"
echo "Cổng COM     : $PORT"
echo "Tốc độ Baud  : $BAUD"
echo "Vai trò Node : $ACTIVE_ROLE"
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

echo ">> 2. Đang nạp firmware vào ESP32 (Tốc độ $BAUD)..."
powershell.exe -Command "python -m esptool --chip esp32 -p $PORT -b $BAUD --before default-reset --after hard-reset write-flash --flash-mode dio --flash-freq 40m --flash-size detect 0x1000 C:\temp\esp32\bootloader.bin 0x8000 C:\temp\esp32\partition-table.bin 0x10000 C:\temp\esp32\SmartCampus-Firmware.bin"

echo "=========================================================="
echo "✅ NẠP FIRMWARE THÀNH CÔNG CHO: $ACTIVE_ROLE"
echo "Để xem log bo mạch, chạy: ./monitor.sh $PORT"
echo "=========================================================="
