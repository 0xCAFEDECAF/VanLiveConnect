@echo off

rem  This batch file sets up the Arduino IDE with all the correct board options (as found in the IDE "Tools" menu)

rem   Board spec for "ESP32S2 Dev Module"
set BOARDSPEC=esp32:esp32:esp32s2:CDCOnBoot=cdc,CPUFreq=240,FlashFreq=80,FlashMode=qio,FlashSize=4M,PartitionScheme=min_spiffs,DebugLevel=none

rem  Fill in your COM port here
set COMPORT=COM3

rem  Get the full directory name of the currently running script
set MYPATH=%~dp0

rem  Launch the Arduino IDE with the specified board options
call "%MYPATH%..\extras\Scripts\ArduinoIdeEnv.bat"
