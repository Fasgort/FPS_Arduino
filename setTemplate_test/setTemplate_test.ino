#include <doxygen.h>
#include <ESP8266.h>
#include "SoftwareSerial.h"
#include <FPS_GT511C3.h>

// SoftwareSerial for ESP8266
SoftwareSerial *esp_serial = new SoftwareSerial(A0, A1); // RX, TX
ESP8266 *esp = new ESP8266(*esp_serial, 9600);

// need a serial port to communicate with the GT-511C3
FPS_GT511C3 fps(8, 7, 9600); // Arduino RX (GT TX), Arduino TX (GT RX)
// the Arduino TX pin needs a voltage divider, see wiring diagram at:
// http://startingelectronics.com/articles/GT-511C3-fingerprint-scanner-hardware/

/* 16x2 LCD circuit:

   LCD RS pin to digital pin 12
   LCD Enable pin to digital pin 11
   LCD D4 pin to digital pin 5
   LCD D5 pin to digital pin 4
   LCD D6 pin to digital pin 3
   LCD D7 pin to digital pin 2
   LCD R/W pin to ground
   LCD VSS pin to ground
   LCD VCC pin to 5V
   10K resistor:
   ends to +5V and ground
   wiper to LCD VO pin (pin 3)

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

  // Debug
  Serial.begin(115200);

  // LCD
  lcd.begin(16, 2);

  // Buzzer
  pinMode(buzzer, OUTPUT); // Set buzzer - pin 9 as an output

  // FPS
  fps.Open(); //send serial command to initialize fps
  fps.DeleteAll(); // Clear the DB

  // Do the syncDB routine once. Restart to redo it.
  Buzz(); // Buzz to tell the user the FPS is ready.
  setTemplateTest();

}

void Buzz() {
  tone(buzzer, 1000); // Send 1KHz sound signal...
  delay(100);
  noTone(buzzer);     // Stop sound...
}

void setTemplateTest() {

  uint8_t data[498] = {4, 22, 89, 0, 185, 138, 136, 19, 195, 153, 67, 255, 145, 227, 133, 238, 71, 119, 201, 222, 150, 139, 134, 118, 137, 88, 48, 24, 9, 68, 133, 223, 6, 168, 11, 232, 34, 148, 134, 12, 11, 88, 137, 238, 141, 132, 136, 38, 4, 151, 65, 231, 190, 212, 132, 83, 196, 87, 58, 88, 241, 180, 134, 32, 75, 27, 71, 236, 10, 37, 133, 205, 199, 247, 15, 4, 82, 117, 133, 223, 145, 252, 215, 207, 137, 138, 136, 131, 68, 248, 247, 23, 58, 187, 133, 0, 196, 247, 133, 254, 74, 19, 134, 118, 6, 72, 46, 231, 106, 109, 134, 36, 139, 236, 196, 7, 161, 3, 137, 29, 66, 184, 67, 7, 213, 115, 137, 149, 130, 248, 249, 7, 93, 90, 134, 234, 7, 154, 127, 248, 141, 154, 133, 231, 6, 249, 69, 255, 17, 155, 132, 222, 66, 249, 5, 255, 105, 187, 132, 99, 4, 57, 254, 7, 253, 163, 132, 218, 133, 250, 7, 255, 157, 173, 136, 50, 193, 181, 63, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 85, 40, 81, 19, 52, 137, 66, 67, 34, 55, 111, 84, 195, 71, 139, 123, 37, 51, 93, 52, 82, 129, 118, 63, 245, 255, 111, 255, 111, 243, 127, 120, 72, 31, 47, 54, 245, 248, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 76, 106};
  fps.SetTemplate(data, 165, true);
}

void loop()
{

  fps.SetLED(true);   //turn on LED so fps can see fingerprint
  lcd.setCursor(0, 0);
  lcd.print(F("Waiting finger  "));

  // Identify fingerprint test
  if (fps.IsPressFinger())
  {
    fps.CaptureFinger(false);
    int id = fps.Identify1_N();

    /*Note:  GT-521F52 can hold 3000 fingerprint templates
             GT-521F32 can hold 200 fingerprint templates
              GT-511C3 can hold 200 fingerprint templates.
             GT-511C1R can hold 20 fingerprint templates.
      Make sure to change the id depending on what
      model you are using */
    if (id < 200) //<- change id value depending model you are using
    { //if the fingerprint matches, provide the matching template ID
      Serial.print(F("Verified ID:"));
      Serial.println(id);
      lcd.setCursor(0, 1);
      lcd.print(F("  Found ID #"));
      lcd.print(id);
    }
    else
    { //if unable to recognize
      Serial.println(F("Finger not found"));
      lcd.setCursor(0, 1);
      lcd.print(F("  Not found     "));
    }
    Buzz();
    delay(3000);
    Buzz();
    lcd.clear();
  }
  else
  {
    Serial.println(F("Please press finger"));
  }
  delay(100);
}
