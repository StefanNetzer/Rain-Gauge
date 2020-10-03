/*
   Rain Gauge

   Copyright 2019 Stefan Netzer

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files
  (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge,
  publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
  subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
  FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
char* version = "0.3";
/*
   Version History
   0.1 Initial Version
   0.2 Several Improvements in Reading of HX711
   0.3 added OTA
*/
/**

   HX711 library for Arduino - example file
   https://github.com/bogde/HX711

   MIT License
   (c) 2018 Bogdan Necula

**/
#include "HX711.h"
// HX711 circuit wiring
const int LOADCELL_DOUT_PIN = D1;
const int LOADCELL_SCK_PIN = D2;
// RainSensor Variables
unsigned long StartTime;
unsigned long CurTime;
float Reading = 0;
float LastReading = 0;
float Total_d = 0;
float Total_h = 0;
float Total_m = 0;
int Readings = 0;
float RR[11] {0};
long RRStart;

HX711 scale;

/*
  MQTT
  https://pubsubclient.knolleary.net/

  This sketch demonstrates the capabilities of the pubsub library in combination
  with the ESP8266 board/library.

  It connects to an MQTT server then:
  - publishes "hello world" to the topic mqtt_topic every two seconds
  - subscribes to the topic "inTopic", printing out any messages
    it receives. NB - it assumes the received payloads are strings not binary
  - If the first character of the topic "inTopic" is an 1, switch ON the ESP Led,
    else switch it off

  It will reconnect to the server if the connection is lost using a blocking
  reconnect function. See the 'mqtt_reconnect_nonblocking' example for how to
  achieve the same result without blocking the main loop.

*/

#include <ESP8266WiFi.h>   // https://github.com/ekstrand/ESP8266wifi/blob/master/LICENSE
#include <PubSubClient.h>  // https://github.com/knolleary/pubsubclient

// Update these with values suitable for your network.

const char* ssid = "SSID";
const char* password = "Password";
const char* mqtt_server = "MQTT Server";
const char* mqtt_topic = " /Rain/1/";

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

#include <NTPClient.h>  // https://github.com/arduino-libraries/NTPClient
#include <WiFiUdp.h>    // https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/WiFiUdp.h


WiFiUDP ntpUDP;

// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionaly you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
NTPClient timeClient(ntpUDP, "de.pool.ntp.org", 3600, 60000); // CET Winter time

// OTA https://github.com/jandrassy/ArduinoOTA
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

//#ifndef STASSID
//#define STASSID "your-ssid"
//#define STAPSK  "your-password"
//#endif
//

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.print ("Version: ");
  Serial.println ( version);

  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  timeClient.begin();
  timeClient.update();
  RRStart = timeClient.getEpochTime();
  Serial.println(RRStart);
  SetupHX711();
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

void loop() {
  int Reading_INT = 0;
  float DeltaReading = 0;
  float AvgReading = 0;
  char buf[20];
  char buffer[9];
  String timestr;

  ArduinoOTA.handle();

  Reading = roundf(scale.get_units() * 10) / 10; // round to 1 digit
  delay(100); // give some time to stabilize
  if ( Reading < 0) { // Tare if a negative Reading is observed
    scale.tare();
    Reading = 0;
    LastReading = 0;
    DeltaReading = 0;
    Serial.print ( "Case 1 " );
  }
  else if ( Reading > LastReading + 0.1 ) {
    DeltaReading = Reading - LastReading;
    LastReading = Reading;
    Serial.print ( "Case 2 " );
  }
  else if ( LastReading > Reading )  {
    LastReading = Reading;  // If the LastReading is greater than the current reading
    DeltaReading = 0;
    Serial.print ( "Case 3 " );
  }
  else Serial.print ( "       " );
  //                                                       the tipping bucket turned over
  timestr = timeClient.getFormattedTime();
  timestr.toCharArray(buffer, sizeof(buffer));
  //buffer = timeClient.getFormattedTime();
  int hour = atoi(strtok(buffer, ":"));
  int minute = atoi(strtok(NULL, ":"));
  int second = atoi(strtok(NULL, ":"));

  //Serial.print(timeClient.getFormattedTime());
  Serial.print(hour);
  Serial.print(":");
  Serial.print(minute);
  Serial.print(":");
  Serial.print(second);
  Serial.print("\t");

  Serial.print(Reading, 3);
  //Serial.print("\t");
  Serial.print("\tLast: ");
  Serial.print(LastReading, 3);
  if ( second == 0) Total_m = 0;
  if ( minute == 0) Total_h = 0;
  if ( hour == 0) Total_d = 0;
  Total_d = Total_d + DeltaReading; // add the positive Delta to the total, this avoids unnecessary taring
  Total_h = Total_h + DeltaReading; // add the positive Delta to the total, this avoids unnecessary taring
  Total_m = Total_m + DeltaReading; // add the positive Delta to the total, this avoids unnecessary taring
  Serial.print("\tDiff: ");
  Serial.print(DeltaReading, 3);
  Serial.print("\tTotal Day: ");
  Serial.print(Total_d, 3);
  Serial.print("\tTotal Hour: ");
  Serial.print(Total_h, 3);
  Serial.print("\tTotal Minute: ");
  Serial.print(Total_m, 3);
  Serial.print("\n");
  //scale.power_down();			        // put the ADC in sleep mode
  //scale.power_up();
  if (!client.connected()) {
    reconnect();
  }
  if ( DeltaReading > 0 || second == 00) { // when difference in weight or every minute
    //  dtostrf(RR[1]-RR[2], 6, 2, buf);
    //  client.publish(" / Rain / 1 / RainRate", buf);
    dtostrf(Reading, 6, 2, buf);
    client.publish("/Rain/1/Volume", buf);
    dtostrf(Total_d, 6, 2, buf);
    client.publish("/Rain/1/Total_d", buf);
    dtostrf(Total_h, 6, 2, buf);
    client.publish("/Rain/1/Total_h", buf);
    dtostrf(Total_m, 6, 2, buf);
    client.publish("/Rain/1/Total_m", buf);
  }
  delay(500);

}



void SetupHX711 ()
{
  Serial.println("HX711 MQTT Client");
  Serial.println("Initializing the scale");
  // Initialize library with data output pin, clock input pin and gain factor.
  // Channel selection is made by passing the appropriate gain:
  // - With a gain factor of 64 or 128, channel A is selected
  // - With a gain factor of 32, channel B is selected
  // By omitting the gain factor parameter, the library
  // default "128" (Channel A) is used here.
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  Serial.println("Before setting up the scale: ");
  Serial.print("read: \t\t");
  Serial.println(scale.read());      // print a raw reading from the ADC
  Serial.print("read average: \t\t");
  Serial.println(scale.read_average(20));   // print the average of 20 readings from the ADC
  Serial.print("get value: \t\t");
  Serial.println(scale.get_value(5));   // print the average of 5 readings from the ADC minus the tare weight (not set yet)
  Serial.print("get units: \t\t");
  Serial.println(scale.get_units(5), 1);  // print the average of 5 readings from the ADC minus tare weight (not set) divided
  // by the SCALE parameter (not set yet)
  scale.set_scale(12050);                      // this value is obtained by calibrating the scale with known weights; see the README for details
  scale.tare();               // reset the scale to 0
  Serial.println("After setting up the scale: ");
  Serial.print("read: \t\t");
  Serial.println(scale.read());                 // print a raw reading from the ADC
  Serial.print("read average: \t\t");
  Serial.println(scale.read_average(20));       // print the average of 20 readings from the ADC
  Serial.print("get value: \t\t");
  Serial.println(scale.get_value(5));   // print the average of 5 readings from the ADC minus the tare weight, set with tare()
  Serial.print("get units: \t\t");
  Serial.println(scale.get_units(5), 1);        // print the average of 5 readings from the ADC minus tare weight, divided
  // by the SCALE parameter set with set_scale
  Serial.println("Readings: ");
}


void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}



void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client_Rain";
    //clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(mqtt_topic, "Connected");
      // ... and resubscribe
      //client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc = ");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
