
/***********************************************/
/*                 Libraries                   */
/***********************************************/
#include <DHT.h>
#include <FS.h>                 //ESP8266 core
#include <ESP8266WiFi.h>        //ESP8266 core
#include <ESP8266WebServer.h>   //ESP8266 core
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>      //https://github.com/bblanchon/ArduinoJson

#if ARDUINOJSON_VERSION_MAJOR > 5
#error "Requires ArduinoJson 5.13.5 or lower"
#endif

#include <PubSubClient.h>       //https://github.com/knolleary/pubsubclient

#include "init.h"
#include "a_root_webpage.h"
#include "b_wifi_webpage.h"
#include "c_mqtt_webpage.h"
#include "d_about_webpage.h"
#include "css.h"
#include "functions.h"


/***********************************************/
/*              Initialization                 */
/***********************************************/
#define DHTPIN D4
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);      //initialise dht sensor
WiFiClient espClient;              //initialise a wifi client
PubSubClient client(espClient);    //creates a partially initialised client instance for MQTT
ESP8266WebServer server(80);    //ESP8266 core

unsigned long previousMillis = 0;


/***********************************************/
/*               MQTT Topics                   */
/***********************************************/
#define t_ip_pub  "dht/1/ip"
#define t_hum_pub "dht/1/hum"
#define t_tmp_pub "dht/1/tmp"
#define t_error_pub "dht/1/error"



/***********************************************/
/*               Main Functions                */
/***********************************************/

/**************** setup ****************/
void setup() {
  Serial.begin(115200);
  Serial.println();
  // addyour setup configuration here

  SPIFFS.begin();
  // for debugging purposes
  //formatFS();

  //check a WiFi config file in the FS
  if (loadConfig(wifiNames, wifiValues, wifi_path, NR_WIFI_PARAM)) {
    StartWiFi();
    ArduinoOTA.onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else { // U_FS
        type = "filesystem";
      }

      // NOTE: if updating FS this would be the place to unmount FS using FS.end()
      Serial.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        client.publish(t_error_pub, "Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        client.publish(t_error_pub, "Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        client.publish(t_error_pub, "Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        client.publish(t_error_pub, "Receive Failed");
      } else if (error == OTA_END_ERROR) {
        client.publish(t_error_pub, "End Failed");
      }
    });
    ArduinoOTA.begin();
  }
  else startAP();
  //check an MQTT config file in the FS
  if (STAStart && loadConfig(mqttNames, mqttValues, mqtt_path, NR_MQTT_PARAM)) {
    startMQTT();

  }
  else {
    Serial.println("no MQTT config File or AP mode");
  }
  startServer();
  dht.begin();

}//setup


/**************** loop ****************/
void loop() {
  server.handleClient();
  if (mqttInit) nonBlockingMQTTConnection();
  ArduinoOTA.handle();
  if ((unsigned long) (millis() - previousMillis) >= 60000) {
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (isnan(h) || isnan(t))
    {
      client.publish(t_error_pub, "Failed reading from DHT sensor");
      return;
    }
    client.publish(t_hum_pub, String(h).c_str());
    client.publish(t_tmp_pub, String(t).c_str());
    previousMillis = millis();
  }
  /* your code here below*/
}//loop


/**************** reconnect ****************/
boolean reconnect() {
  Serial.println("Attempting MQTT connection...");
  String clientId = "ESP8266Client-";
  clientId += "DHT_1";
  // Attempt to connect
  if (client.connect(clientId.c_str(), mqttValues[3], mqttValues[4])) {
    Serial.printf("\nCONGRATS!!! U'r CONNECTED TO MQTT BROKER!\nstart making your things talk!!!");
    /*** subscribe here below ***/
    client.publish(t_ip_pub, myIP.c_str());
    client.publish(t_error_pub, "OTA begin");
  }
  return client.connected();
}


/**************** callback ****************/
void callback(char* p_topic, byte* p_payload, unsigned int p_length) {
  char msg_buff[100];
  for (int i = 0; i < p_length; i++) {
    msg_buff[i] = p_payload[i];
  }
  msg_buff[p_length] = '\0';
  String payload = String(msg_buff);
  String topic = String(p_topic);
  /***
      here you recieve the payload on the topics you've been subscribed to.
      choose what to do here
   ***/
}

/******************************** END OF FILE ********************************/