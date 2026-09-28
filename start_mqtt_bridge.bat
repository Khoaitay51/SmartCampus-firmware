@echo off
title SmartCampus MQTT Bridge (Wi-Fi to WSL2 Docker)
echo Starting MQTT Proxy (0.0.0.0:1883 -^> 127.0.0.1:1883)...
python "%~dp0scripts\mqtt_bridge.py"
pause
