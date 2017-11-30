#!/bin/sh
stty < /dev/ttyUSB0 speed 9600
cat /dev/ttyUSB0 &
echo -ne "AT" >/dev/ttyUSB0 
sleep 1
echo
echo -ne "AT+BAUD7" >/dev/ttyUSB0 
sleep 1
echo
stty < /dev/ttyUSB0 speed 57600
echo -ne "AT+PIN0000" >/dev/ttyUSB0 
sleep 1
echo
echo -ne "AT+NAMEBlueV" >/dev/ttyUSB0 
sleep 1
echo
hcitool scan
