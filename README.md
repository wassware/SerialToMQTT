# SerialToMQTT
ESP32 Serial2 interface to MQTT. Mostly general purpose, but customised for the topics used for an ESP32 based heating controller so can be remotely suervised.
Serial2 talks to the heating control's Serial2 (or its Bluetooth interface by another module at the moment).
Will get streamlines somewhat when update the heating control code.
Uses wifisecure (as using Hive MQTT) so needs a certificate. This is loaded from a file in SPIFFS. The one checked in is the ISRG root1 used by Hive for their MQTT.
Configuration in a JSON file as per example.
Both these files are put in a /data/ directory and uploaded to SPIFFS. So need to install the 'ESP32 sketch data upload' into Arduino
