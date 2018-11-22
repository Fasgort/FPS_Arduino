#include <doxygen.h>
#include <ESP8266.h>
#include "SoftwareSerial.h"
#include <FPS_GT511C3.h>

// SoftwareSerial for ESP8266
SoftwareSerial esp_serial(A0, A1); // RX, TX
ESP8266 *esp = new ESP8266(esp_serial, 9600);

// need a serial port to communicate with the GT-511C3
FPS_GT511C3 fps(8, 7, esp); // Arduino RX (GT TX), Arduino TX (GT RX)
// the Arduino TX pin needs a voltage divider, see wiring diagram at:
// http://startingelectronics.com/articles/GT-511C3-fingerprint-scanner-hardware/

/* 16x2 LCD circuit:

 * LCD RS pin to digital pin 12
 * LCD Enable pin to digital pin 11
 * LCD D4 pin to digital pin 5
 * LCD D5 pin to digital pin 4
 * LCD D6 pin to digital pin 3
 * LCD D7 pin to digital pin 2
 * LCD R/W pin to ground
 * LCD VSS pin to ground
 * LCD VCC pin to 5V
 * 10K resistor:
 * ends to +5V and ground
 * wiper to LCD VO pin (pin 3)

*/

// Buzzer
const int buzzer = 9; //buzzer to arduino pin 9

void setup() {

  // Buzzer
  pinMode(buzzer, OUTPUT); // Set buzzer - pin 9 as an output

  // FPS
  Serial.begin(9600);
  fps.Open(); //send serial command to initialize fps
  //fps.DeleteAll(); // Clear the DB

}

void Buzz(){
  tone(buzzer, 1000); // Send 1KHz sound signal...
  delay(100);
  noTone(buzzer);     // Stop sound...
}

void tcp_test() {

  Serial.println("St1");
  esp->restart();
  while(!esp->kick()) delay(100);
  Serial.println("St2");
  Serial.println(esp->joinAP("$WIFI_SSID$", "$WIFI_PASS$"));
  Serial.println(esp->getLocalIP());
  Serial.println(esp->createTCP("10.0.0.2",40444));
  //esp.leaveAP();
  //if(!esp.joinAP("$WIFI_SSID$", "$WIFI_PASS$")) return 0;
  //Serial.println(esp.getLocalIP());
  //if(!esp.createTCP("10.0.0.2",40444)) return 0;

  //if(!fps.GetTemplate(0)) return;
  Serial.println(esp->releaseTCP());
  Serial.println(esp->leaveAP());
  
  //uint8_t test[8] = {0,1,2,3,4,5,6,7};
  //Serial.println(esp.send(test, 8));
  Serial.println("All done.");
  return;
}

void loop() {
  tcp_test();
}
