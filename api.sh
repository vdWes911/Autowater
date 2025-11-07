#!/bin/bash

ESP32_IP=192.168.0.26

curl http://192.168.0.26/api/status

# Turn relay 0 ON
#curl http://$ESP32_IP/api/relay?id=0&action=on
#
## Turn relay 1 OFF
#curl http://$ESP32_IP/api/relay?id=1&action=off
#
## Toggle relay 2
#curl http://$ESP32_IP/api/relay?id=2&action=toggle