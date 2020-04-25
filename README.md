# This is a Wemos D1 Mini sketch that reads the flashes emitted by an energy meter and uploads realtime energy consumption to a MQTT server

1) Assemble the hardware according to https://maker.pro/arduino/tutorial/how-to-use-an-ldr-sensor-with-arduino
2) Copy config.h.sample to config.h
3) Edit your newly created config.h to add your wifi, MQTT, OTA credentials, etc
4) Upload to your board and wait for the energy consumption to be published to your MQTT (in KWh) everytime a flash is sensed by the LDR

Obs: The board must be running continuously to read the flashes so a mains power adapter is a better choice than a battery, which could be depleted quickly.

