#!/usr/bin/bash

# This script sets up the Arduino IDE with all the correct board options (as found in the IDE "Tools" menu)

# Board spec for "ESP32S2 Dev Module"
BOARDSPEC=esp32:esp32:esp32s2:CDCOnBoot=cdc,CPUFreq=240,FlashFreq=80,FlashMode=qio,FlashSize=4M,PartitionScheme=min_spiffs,DebugLevel=none

# Fill in your COM port here
COMPORT=/dev/ttyUSB0

# Get the full directory name of the currently running script
\cd `dirname $0`
MYPATH=`pwd`
\cd - > /dev/null

# Launch the Arduino IDE with the specified board options
. "${MYPATH}/../extras/Scripts/ArduinoIdeEnv.sh"
