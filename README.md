# home_esp_dht

## Project starting script, ESPloit:

 * provides a webserver for wifi and mqtt broker configuration
 * no subscription or publishing topics provided
 * AP mode on IP: 192.168.4.1
 * AP name: ESPLOIT AP as defined in "init.h"
    * #define deviceName "ESPLOIT AP"
 *   in-line comments
 *   for more information reference: https://github.com/theiothing/ESPloit
 
 ## This version
 modified by davide B (https://github.com/portbusy/)
 * added:
    * OTA support, 
    * DHT11 read/send data over mqtt
