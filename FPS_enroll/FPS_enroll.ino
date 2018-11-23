#include <doxygen.h>
#include <ESP8266.h>
#include "SoftwareSerial.h"
#include <FPS_GT511C3.h>

// SoftwareSerial for ESP8266
SoftwareSerial *esp_serial = new SoftwareSerial(A0, A1); // RX, TX
ESP8266 *esp = new ESP8266(*esp_serial, 9600);

// need a serial port to communicate with the GT-511C3
FPS_GT511C3 fps(8, 7, esp_serial, esp, 9600); // Arduino RX (GT TX), Arduino TX (GT RX)
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

// include the library code:
#include <LiquidCrystal.h>

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

void setup() {
  
  // LCD
  lcd.begin(16, 2);

  // Buzzer
  pinMode(buzzer, OUTPUT); // Set buzzer - pin 9 as an output

  // FPS
  fps.Open(); //send serial command to initialize fps
  fps.DeleteAll(); // Clear the DB

  // Do the enroll routine once. Restart to redo it.
  Enroll();

}

void Buzz(){
  tone(buzzer, 1000); // Send 1KHz sound signal...
  delay(100);
  noTone(buzzer);     // Stop sound...
}

bool syncFingerprint(int id) {

  // Get the ESP online with a clean state
  esp_serial->listen();
  while(esp_serial->available() > 0) esp_serial->read();
  while(!esp->kick()) delay(1000);
  esp->unregisterUDP();
  esp->leaveAP();
  esp->restart();
  while(!esp->kick()) delay(1000);

  // Connect to wifi and start a UDP connection
  esp->joinAP(F("$WIFI_SSID$", "$WIFI_PASS$"));
  esp->registerUDP(F("10.0.0.2"),40444);

  // Identify yourself to the server and declare intentions
  uint8_t enroller_code[5] = {1, 238, 0, 1, 17}; // 01 EE 00 01 11
  esp->send(enroller_code, 5);

  // Receive the reply
  uint8_t reply_buffer[5];
  esp->recv(reply_buffer, 5, 5000);

  // Send the data if the reply is okay
  bool sync_done = false;
  if(reply_buffer[0] == 1 && reply_buffer[1] == 219 && reply_buffer[4] == 170) {
    uint8_t sending_code[5] = {1, 238, 0, 1, 29}; // 01 EE 00 01 1D
    esp->send(sending_code, 5);
    
    sync_done = fps.GetTemplate(id); // FPS will get the fingerprint and send it to the ESP
  }

  // Try to leave the ESP offline with a clean state
  while(!esp->kick()) delay(1000);
  while(esp_serial->available() > 0) esp_serial->read();
  esp->unregisterUDP();
  esp->leaveAP();
  esp->restart();
  while(esp_serial->available() > 0) esp_serial->read();

  if(!sync_done) return true;
  else return false;
  
}

void Enroll() {

  fps.SetLED(true);   //turn on LED so fps can see fingerprint
  
  // find open enroll id
  int enrollid = 0;
  bool usedid = true;
  while (usedid == true)
  {
    usedid = fps.CheckEnrolled(enrollid);
    if (usedid==true) enrollid++;
  }
  fps.EnrollStart(enrollid);
  lcd.setCursor(0,0);
  lcd.print(F("Enrolling #"));
  lcd.print(enrollid);

  // Enroll
  Buzz();
  lcd.setCursor(0,1);
  lcd.print(F("  Press finger  "));
  while(fps.IsPressFinger() == false) delay(100);
  bool bret = fps.CaptureFinger(true);
  int iret = 0;
  if (bret != false)
  {
    Buzz();
    lcd.setCursor(0,1);
    lcd.print(F(" Remove finger  "));
    fps.Enroll1(); 
    while(fps.IsPressFinger() == true) delay(100);
    Buzz();
    lcd.setCursor(0,1);
    lcd.print(F("  Press finger  "));
    while(fps.IsPressFinger() == false) delay(100);
    bret = fps.CaptureFinger(true);
    if (bret != false)
    {
      Buzz();
      lcd.setCursor(0,1);
      lcd.print(F(" Remove finger  "));
      fps.Enroll2();
      while(fps.IsPressFinger() == true) delay(100);
      Buzz();
      lcd.setCursor(0,1);
      lcd.print(F("  Press finger  "));
      while(fps.IsPressFinger() == false) delay(100);
      bret = fps.CaptureFinger(true);
      if (bret != false)
      {
        Buzz();
        lcd.setCursor(0,1);
        lcd.print(F(" Remove finger  "));
       iret = fps.Enroll3();
        while(fps.IsPressFinger() == true) delay(100);
        if (iret == 0)
        {
          lcd.setCursor(0,1);
          lcd.print(F("   Successful   "));

          fps.SetLED(false);   //turn off LED
          delay(3000);
          Buzz();
          lcd.clear();
          
          // Synchronize the fingerprint with the DB
          lcd.setCursor(0,0);
          lcd.print(F("Synchronizing DB"));
          for(int i=1; i<=3; i++){
            lcd.setCursor(0,1);
            lcd.print(F("Try #"));
            lcd.print(i);
            lcd.print(F("      "));
            lcd.setCursor(6,1);
            if(syncFingerprint(enrollid)) {
              lcd.print(F(": Success"));
              Buzz();
              delay(3000);
              break;
            } else {
              lcd.print(F(": Fail"));
              Buzz();
              delay(3000);
            }
          }
                    
        }
        else
        {
          lcd.setCursor(0,1);
          lcd.print(F("   Failed: #"));
          lcd.print(iret);
          lcd.print(F("   "));
        }
      }
      else {
        lcd.setCursor(0,1);
        lcd.print(F("Fail: Bad finger"));
      }
    }
    else {
      lcd.setCursor(0,1);
      lcd.print(F("Fail: Bad finger"));
    }
  }
  else {
    lcd.setCursor(0,1);
    lcd.print(F("Fail: Bad finger"));
  }
  fps.SetLED(false);   //turn off LED
  delay(3000);
  Buzz();
  lcd.clear();
}

void loop()
{
  delay(100000); // Done, shutdown or restart
}
