///////////////////////////////////////////////////////////////////////////////////////
// Author: Rawat S. (Department of Electrical & Computer Engineering, KMUTNB, Thailand
// Date: 2017-11-29
// Description:
//    This sketch is written for ESP8266. It shows how to 
//    - program the ESP8266 to operate in deep-sleep mode (60-second sleep interval), 
//    - use the SoftwareSerial library to create a soft serial port 
//      in order to communicate with the Xbee module (baudrate 9600),
//    - read the data from the BME280 sensor module via the I2C bus,
//    - and read the battery voltage level from the A0 pin.
// 
// Note:
//    - The XBee module is programmed with the XBee router AT firmware.
//    - In order to receive data, another XBee module is needed and must be programmed
//      to operate as the XBee Coordinator (AT firmware) and all XBee modules 
//      must have the same PAN ID and use the same channel.
// Software: Arduino IDE v1.8.2 + ESP8266 v2.3.0
// Hardware: WeMos D1 mini, Battery Shield, BME280 module, XBee S2 module
//
///////////////////////////////////////////////////////////////////////////////////////

#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <Wire.h>

extern "C" {
  #include <user_interface.h> // https://github.com/esp8266/Arduino actually tools/sdk/include
}

#include "BME280.h"

// Define your the name of your XBee device
#define XBEE_DEV_NAME  "router2"

#define DEBUG

// If your battery shield for the Wemos D1 mini board provides the battery voltage input 
// to the A0 pin, you can uncomment the following line.
#define READ_VBATT

#define ONE_USEC        (1000000UL)
#define ONE_MSEC        (1000)

#define SLEEP_SEC       (60 * ONE_USEC) 

#define BME280_ADDR     (0x76)

#define SDA_PIN         (D4)    // D4 pin (GPIO-2)
#define SCL_PIN         (D3)    // D3 pin (GPIO-0)

// If you want to have a SLEEP request pin for XBee (for XBee end device)
//#define XBEE_SLEEP_PIN  (D7)   // D7 pin (GPIO-13)

#define TX_PIN          (D1)   // D1 pin (GPIO-5)
#define RX_PIN          (D2)   // D2 pin (GPIO-4)

// see: https://www.arduino.cc/en/Reference/SoftwareSerial
SoftwareSerial xbeeSerial( RX_PIN, TX_PIN );

char sbuf[128];
String str;

BME280 bme;

// WeMos D1 battery shield has a 130k Ohm series resistance
// WeMod D1 mini board has a voltage divider attached to the A0 pin: 100k (low side), 220k (high side)

/*
   A0 >----
          |
        220K
          |---> ADC pin (ESP8266)
        100K
          |
         GND
*/

#define VDIV_R_HIGH  (220+130+25)    // the value of the low-side resistor (kOhm)of the voltage divider
#define VDIV_R_LOW   (100)           // the value of the high-side resistor (kOhm) of the voltage divider

void wifi_off() {
  WiFi.mode( WIFI_STA );
  WiFi.disconnect(); 
  WiFi.mode( WIFI_OFF );
  WiFi.forceSleepBegin();
  delay(1);
}

#ifdef READ_VBATT

float vbatt;

void read_vbatt() {
  int value = analogRead(A0);
  vbatt = value / 1023.0;
  vbatt = vbatt * (VDIV_R_HIGH+VDIV_R_LOW) / VDIV_R_LOW;
}
#endif

float humid, temp, pres;

void read_sensor() {
  temp  = bme.readTemperature();
  humid = bme.readHumidity();
  pres  = bme.readPressure() /100.0f; // hPa
}

void send_data() {
  str = "";
  sprintf( sbuf, "{\"id\":\"%s\",", XBEE_DEV_NAME );
  str += sbuf;
  dtostrf( temp, 3, 1, sbuf );
  str += "\"temp\":";
  str += sbuf;
  dtostrf( humid, 3, 1, sbuf );
  str += ",\"humid\":";
  str += sbuf;
  dtostrf( pres, 5, 1, sbuf );
  str += ",\"press\":";
  str += sbuf;

#ifdef READ_VBATT
  dtostrf( vbatt, 4, 3, sbuf );
  str += ",\"vbatt\":";
  str += sbuf;
#endif 
 
  str += "}";
  xbeeSerial.println( str.c_str() );
  xbeeSerial.flush();
}

void process() {
#ifdef XBEE_SLEEP_PIN
   digitalWrite( XBEE_SLEEP_PIN, LOW );
#endif

#ifdef READ_VBATT
   read_vbatt();
#endif

   read_sensor();
   send_data();
   delay(400);

#ifdef XBEE_SLEEP_PIN
   digitalWrite( XBEE_SLEEP_PIN, HIGH ); // send a SLEEP request to XBee
#endif 
}

void setup() {  
  wifi_off(); // disable WiFi

#ifdef XBEE_SLEEP_PIN  
  pinMode( XBEE_SLEEP_PIN, OUTPUT );
  digitalWrite( XBEE_SLEEP_PIN, LOW );
#endif 

#ifdef DEBUG
  Serial.begin( 115200 );
  Serial.println( "\n\n\n" );
  Serial.flush();
#endif

  xbeeSerial.begin( 9600 );
  xbeeSerial.flush();
  delay(10);
  
  struct rst_info * info = ESP.getResetInfoPtr();
  if ( info->reason != REASON_DEEP_SLEEP_AWAKE ) {
     for ( int i=10; i > 0; i-- ) {
#ifdef DEBUG
        Serial.printf( "Count down: %d\n", i );
#endif
        delay(1000);
     }
  }

  if ( bme.begin( BME280_ADDR, SDA_PIN, SCL_PIN, 400000 ) ) {
     if ( info->reason != REASON_DEEP_SLEEP_AWAKE ) {
#ifdef DEBUG
        Serial.println( F("BME280 OK") );
        Serial.flush();
#endif
        read_sensor();
        delay(10);
     }
     process();
  } 
  else {
#ifdef DEBUG
    Serial.println( F("BME280 FAILED !!!") );
    Serial.flush();
#endif
  }
  delay(10);

  ESP.deepSleep( SLEEP_SEC );  // enter deep-sleep mode 
                               // don't forget to connect the D0 (WAKE) pin to the /RST pin.
  delay(1);
}

void loop() {
  // ...
}
////////////////////////////////////////////////////////////////////////////////

