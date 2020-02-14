/*
 * INCLUDES
 */

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

 /*
 * DEFINITIONS
 */

#include "config.h"

#define MAX_UPDATE_INTERVAL 30000
#define LDR_THRESHOLD    200

int sensorPin = A0; // select the input pin for LDR

/*
 * DECLARATIONS
 */
 
unsigned long lastPulse; // time of last detected pulse 
unsigned long lastSent; // last MQTT message published
unsigned long lastRun; // last MQTT message published
int pulsesSinceSent = 0; // number of pulses since last msg was published
int wasAboveThreshold = false; // to detect pulse border

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, MQTT_SERVER, MQTT_SERVERPORT, MQTT_USERNAME, MQTT_KEY);

// Setup a feed called 'photocell' for publishing.
Adafruit_MQTT_Publish photocell = Adafruit_MQTT_Publish(&mqtt, MQTT_CHANNEL);

/*
 * AUXILIARY FUNCTIONS
 */

void blink_ok() {
    digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)
    delay(50);                       // wait for a second
    digitalWrite(LED_BUILTIN, HIGH);    // turn the LED off by making the voltage LOW
    delay(50);                       // wait for a second
    digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)
    delay(50);                       // wait for a second
    digitalWrite(LED_BUILTIN, HIGH);    // turn the LED off by making the voltage LOW
    delay(50);                       // wait for a second
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
  blink_ok();
}

/*
 * SETUP
 */

void setupOTA() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPort(OTA_PORT);
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWD);

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
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();  
}

void setupWifi() {
  // Connect to WiFi network
  Serial.println();
  Serial.print( "Connecting to " );
  Serial.println( WIFI_SSID );
 
  WiFi.mode( WIFI_STA );
  WiFi.begin( WIFI_SSID, WIFI_PASSWD );
 
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  //WiFi.printDiag(Serial);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);    // turn the LED off by making the voltage LOW

  Serial.begin(115200);
  delay(10);

  setupWifi();
  
  setupOTA();
  
  blink_ok();

}

/*
 * MAIN LOOP
 */

void loop() {
  char strData[10]; // consumption string
  unsigned long now; // variable to store now
  unsigned long energyConsumption; // variable to store current energy consumption
  int sensorValue = 0; // variable to store the value coming from the sensor

  // Reboots if time counter restarted, just in case
  if ( millis() < lastRun ) {
     ESP.restart();
  } else {
     lastRun = millis();
  }


  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition further below.
  MQTT_connect();

  sensorValue = analogRead(sensorPin); // read the value from the sensor
  //Serial.print(F("LDR Reading: "));
  //Serial.println(sensorValue); //prints the values coming from the sensor on the screen

  if ( sensorValue > LDR_THRESHOLD ) {
    if ( wasAboveThreshold == false ) { //only act on rising 
      Serial.print("Pulse detected ");
      Serial.println(sensorValue);
      now = millis();
      
      if ( lastSent == 0 ) {
        lastSent = now;
      } else {
        pulsesSinceSent++;
      }

      if ( ( lastPulse > 0 ) && ( now > ( lastSent + MAX_UPDATE_INTERVAL ) ) ) {
        energyConsumption = pulsesSinceSent * 3600000 / (now - lastSent) ;

        sprintf( strData, "%u", energyConsumption );
        Serial.print(F("\nSending energy consumption val "));
        Serial.print(strData);
        Serial.print(" kWh...");

        if (! photocell.publish(strData)) {
          Serial.println(F("Failed"));
        } else {
          lastSent = now;
          pulsesSinceSent = 0;
          Serial.println(F("OK!"));
          blink_ok();
        }
      }
      lastPulse = now;
    }
    wasAboveThreshold = true;
  } else {
    wasAboveThreshold = false;
  }

  delay(20);

  ArduinoOTA.handle();

}
