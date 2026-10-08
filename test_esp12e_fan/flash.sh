#!/bin/bash
# ==============================================================================
# Script biên dịch và nạp firmware ESP-12E (ESP8266) từ WSL2 ra cổng COM Windows
# ==============================================================================
set -e

# Xác định thư mục dự án
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

PORT_ARG="$1"
BAUD=${2:-115200}

if [ -n "$PORT_ARG" ]; then
    PORT="$PORT_ARG"
else
    DETECTED_PORT=$(powershell.exe -Command "((Get-CimInstance Win32_PnPEntity | Where-Object { \$_.Name -match 'CH340|CP210|FTDI|UART' -and \$_.Name -match 'COM\d+' } | ForEach-Object { if (\$_.Name -match '(COM\d+)') { \$matches[1] } }) | Select-Object -First 1)" 2>/dev/null | tr -d '\r\n')
    if [ -n "$DETECTED_PORT" ]; then
        PORT="$DETECTED_PORT"
        echo ">> 🔎 Tự động nhận diện bo mạch tại cổng: $PORT"
    else
        PORT="COM8"
    fi
fi

# Tìm PlatformIO CLI
PIO_BIN="$HOME/.local/bin/pio"
if ! command -v "$PIO_BIN" >/dev/null 2>&1; then
    if command -v pio >/dev/null 2>&1; then
        PIO_BIN="pio"
    else
        echo "❌ Không tìm thấy PlatformIO Core CLI! Đang thử cài đặt tự động..."
        /usr/bin/python3 -m pip install --user --break-system-packages platformio
        PIO_BIN="$HOME/.local/bin/pio"
    fi
fi

echo "=========================================================="
echo "⚡ SmartCampus - ESP-12E Fan Test Flasher (WSL2 -> COM)"
echo "Thư mục code : $SCRIPT_DIR"
echo "Cổng COM     : $PORT"
echo "Tốc độ nạp   : $BAUD"
echo "Bo mạch      : ESP-12E / NodeMCU ESP8266"
echo "=========================================================="

echo ">> 1. Biên dịch mã nguồn với PlatformIO..."
"$PIO_BIN" run -e esp12e

BIN_PATH="$SCRIPT_DIR/.pio/build/esp12e/firmware.bin"
if [ ! -f "$BIN_PATH" ]; then
    echo "❌ Lỗi: Không tìm thấy file $BIN_PATH sau khi build!"
    exit 1
fi

echo ">> 2. Sao chép file nhị phân sang Windows temp (C:\\temp\\esp8266)..."
mkdir -p /mnt/c/temp/esp8266
cp "$BIN_PATH" /mnt/c/temp/esp8266/firmware.bin

echo ">> 3. Đang nạp firmware vào ESP-12E qua cổng $PORT (Tốc độ $BAUD)..."
powershell.exe -Command "python -m esptool --chip esp8266 -p $PORT -b $BAUD --before default-reset --after hard-reset write-flash --flash-mode dio --flash-size detect 0x0 C:\temp\esp8266\firmware.bin"

echo "=========================================================="
echo "✅ NẠP FIRMWARE ESP-12E THÀNH CÔNG!"
echo "Để kiểm tra và điều khiển quạt qua Serial, bạn có thể chạy:"
echo "  python test_serial_fan.py $PORT"
echo "Hoặc mở file run_test.bat trên Windows."
echo "=========================================================="
