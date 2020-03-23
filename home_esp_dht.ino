
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
#include <WEMOS_SHT3X.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>   // https://github.com/theiothing/Adafruit_SSD1306-esp8266-64x48

#if ARDUINOJSON_VERSION_MAJOR > 5
#error "Requires ArduinoJson 5.13.5 or lower"
#endif

#include <PubSubClient.h>       //https://github.com/knolleary/pubsubclient


WiFiClient espClient;              //initialise a wifi client
PubSubClient client(espClient);    //creates a partially initialised client instance for MQTT
ESP8266WebServer server(80);    //ESP8266 core

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

unsigned long previousMillis = 0;
unsigned long pollingFreq = 15000; //in milliseconds

// SCL GPIO5
// SDA GPIO4
#define OLED_RESET 0  // GPIO0
Adafruit_SSD1306 display(OLED_RESET);

#define NUMFLAKES 10
#define XPOS 0
#define YPOS 1
#define DELTAY 2


#define LOGO16_GLCD_HEIGHT 16
#define LOGO16_GLCD_WIDTH  16
static const unsigned char PROGMEM logo16_glcd_bmp[] =
{ B00000000, B11000000,
  B00000001, B11000000,
  B00000001, B11000000,
  B00000011, B11100000,
  B11110011, B11100000,
  B11111110, B11111000,
  B01111110, B11111111,
  B00110011, B10011111,
  B00011111, B11111100,
  B00001101, B01110000,
  B00011011, B10100000,
  B00111111, B11100000,
  B00111111, B11110000,
  B01111100, B11110000,
  B01110000, B01110000,
  B00000000, B00110000
};

#if (SSD1306_LCDHEIGHT != 48)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

/***********************************************/
/*               MQTT Topics                   */
/***********************************************/
#define t_ip_pub  "dht/2/ip"
#define t_hum_pub "dht/2/hum"
#define t_tmp_pub "dht/2/tmp"
#define t_error_pub "dht/2/error"
#define t_akn_pub "dht/2/akn"
#define t_cmd_sub "dht/1/command"


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

  /*************** BEGIN DISPLAY INIT ***************/
  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)
  // init done

  // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.
  display.display();
  delay(2000);

  // Clear the buffer.
  display.clearDisplay();

  // draw a single pixel
  display.drawPixel(10, 10, WHITE);
  // Show the display buffer on the hardware.
  // NOTE: You _must_ call display after making any drawing commands
  // to make them visible on the display hardware!
  display.display();
  delay(2000);
  display.clearDisplay();

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
    client.publish(t_ip_pub, myIP.c_str());
    client.publish(t_akn_pub, "Server started");
    client.subscribe(t_cmd_sub);

    client.publish(t_akn_pub, "OTA begin");
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
  if ((unsigned long) (millis() - previousMillis) >= pollingFreq) {
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (isnan(h) || isnan(t))
    {
      client.publish(t_error_pub, "Failed reading from DHT sensor");
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(display.width()/2, display.height()/2);
      display.clearDisplay();
      display.println("ERROR");
      previousMillis = millis();
      return;
    }
    client.publish(t_hum_pub, String(h).c_str());
    client.publish(t_tmp_pub, String(t).c_str());
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.clearDisplay();
    display.println("T: " + String(t) + " C");
    display.println();
    display.println();
    display.println();
    display.println("H: " + String(h) + " %");
    display.display();
    previousMillis = millis();
  }
  /* your code here below*/
}//loop


/**************** reconnect ****************/
boolean reconnect() {
  Serial.println("Attempting MQTT connection...");
  String clientId = "ESP8266Client-";
  clientId += "DHT_2";
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