#include <doxygen.h>
#include <ESP8266.h>
#include "SoftwareSerial.h"
#include <FPS_GT511C3.h>

// need a serial port to communicate with the GT-511C3
FPS_GT511C3 fps(8, 7); // Arduino RX (GT TX), Arduino TX (GT RX)
// the Arduino TX pin needs a voltage divider, see wiring diagram at:
// http://startingelectronics.com/articles/GT-511C3-fingerprint-scanner-hardware/

// SoftwareSerial for ESP8266
SoftwareSerial esp_serial(A0, A1); // RX, TX
ESP8266 esp(esp_serial, 9600);

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
  
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  // Print a message to the LCD.
  // lcd.print("hello, world!");
  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  //lcd.setCursor(0, 1);

  // Buzzer
  pinMode(buzzer, OUTPUT); // Set buzzer - pin 9 as an output

  // FPS

  Serial.begin(9600);
  //fps.UseSerialDebug = true; // so you can see the messages in the serial debug screen
  //fps.Open(); //send serial command to initialize fps
  //fps.SetLED(true);   //turn on LED so fps can see fingerprint

  //fps.DeleteAll(); // Clear the DB
  //Enroll();

  // ESP8266
  //ESP_serial.begin(9600); //set up Arduino's hardware serial UART
  //ESP_serial.setTimeout(5000);
  //Serial.print("AT+RST");

}

void Buzz(){
  tone(buzzer, 1000); // Send 1KHz sound signal...
  delay(100);
  noTone(buzzer);     // Stop sound...
}

/*void Enroll()
{
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
  lcd.print("Enrolling #");
  lcd.print(enrollid);

  // enroll
  Buzz();
  Serial.print("Press finger to Enroll #");
  Serial.println(enrollid);
  lcd.setCursor(0,1);
  lcd.print("  Press finger  ");
  while(fps.IsPressFinger() == false) delay(100);
  bool bret = fps.CaptureFinger(true);
  int iret = 0;
  if (bret != false)
  {
    Buzz();
    Serial.println("Remove finger");
    lcd.setCursor(0,1);
    lcd.print(" Remove finger  ");
    fps.Enroll1(); 
    while(fps.IsPressFinger() == true) delay(100);
    Buzz();
    Serial.println("Press same finger again");
    lcd.setCursor(0,1);
    lcd.print("  Press finger  ");
    while(fps.IsPressFinger() == false) delay(100);
    bret = fps.CaptureFinger(true);
    if (bret != false)
    {
      Buzz();
      Serial.println("Remove finger");
      lcd.setCursor(0,1);
      lcd.print(" Remove finger  ");
      fps.Enroll2();
      while(fps.IsPressFinger() == true) delay(100);
      Buzz();
      Serial.println("Press same finger yet again");
      lcd.setCursor(0,1);
      lcd.print("  Press finger  ");
      while(fps.IsPressFinger() == false) delay(100);
      bret = fps.CaptureFinger(true);
      if (bret != false)
      {
        Buzz();
        Serial.println("Remove finger");
        lcd.setCursor(0,1);
        lcd.print(" Remove finger  ");
        iret = fps.Enroll3();
        while(fps.IsPressFinger() == true) delay(100);
        if (iret == 0)
        {
          Serial.println("Enrolling Successful");
          lcd.setCursor(0,1);
          lcd.print("   Successful   ");
        }
        else
        {
          Serial.print("Enrolling Failed with error code:");
          Serial.println(iret);
          lcd.setCursor(0,1);
          lcd.print("   Failed: #");
          lcd.print(iret);
          lcd.print("   ");
        }
      }
      else {
        Serial.println("Failed to capture third finger");
        lcd.setCursor(0,1);
        lcd.print("Fail: Bad finger");
      }
    }
    else {
      Serial.println("Failed to capture second finger");
      lcd.setCursor(0,1);
      lcd.print("Fail: Bad finger");
    }
  }
  else {
    Serial.println("Failed to capture first finger");
    lcd.setCursor(0,1);
    lcd.print("Fail: Bad finger");
  }
  delay(3000);
  Buzz();
  lcd.clear();
}*/

void loop()
{
  Serial.println(esp.joinAP("$WIFI_SSID$", "$WIFI_PASS$"));
  Serial.println(esp.getLocalIP());
  Serial.println(esp.createTCP("10.0.0.2",40444));
  uint8_t test[8] = {0,1,2,3,4,5,6,7};
  Serial.println(esp.send(test, 8));
  Serial.println("All done.");

  delay(1000);
/*  
  lcd.setCursor(0,0);
  while(true){
    Serial.println("AT");
    delay(10);
    int counta = 0;
    while(Serial.available()) {
      lcd.print((char)Serial.read());
      counta++;
      if((counta%16) == 0) lcd.setCursor(0,0);
      delay(100);
    }
  }*/
  /*lcd.setCursor(0,0);
  lcd.print("Waiting finger  ");
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
       model you are using *//*
    if (id <200) //<- change id value depending model you are using
    {//if the fingerprint matches, provide the matching template ID
      Serial.print("Verified ID:");
      Serial.println(id);
      lcd.setCursor(0,1);
      lcd.print("  Found ID #");
      lcd.print(id);
    }
    else
    {//if unable to recognize
      Serial.println("Finger not found");
      lcd.setCursor(0,1);
      lcd.print("  Not found     ");
    }
    Buzz();
    delay(3000);
    Buzz();
    lcd.clear();
  }
  else
  {
    Serial.println("Please press finger");
  }
  delay(100);*/
}
