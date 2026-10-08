@echo off
title ESP-12E Fan Controller Serial Test
echo ========================================================
echo  KHOI DONG TEST DIEU KHIEN QUAT ESP-12E QUA SERIAL
echo ========================================================
python "%~dp0test_serial_fan.py" %*
pause
