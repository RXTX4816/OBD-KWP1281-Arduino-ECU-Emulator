# Arduino Mega KWP1281 ECU Emulator

This code is for the Arduino Mega with a 480x320 Non-Touch Color Display Shield (Optional)

![](assets/DEMO.gif)

Used to test [OBDisplay](https://github.com/RXTX4816/OBD-KKL-KW1281-ARDUINO-TFTLCD-TRIPCOMPUTER) when not having a real ECU around.

full_duplex not supported currently

Mimicks a KWP1281 ECU on address 0x17 with baud 10400.



Features:
- 5baud init
- sync bytes and device data
- acknowledge and group reading (only group 1, 2 and 3 implemented)
- automatic reconnect
- LCD display shows connection status

