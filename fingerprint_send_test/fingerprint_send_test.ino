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
  fps.Open(); //send serial command to initialize fps
  //fps.SetLED(true);   //turn on LED so fps can see fingerprint
  //fps.DeleteAll(); // Clear the DB

  // Enroll test
  //Enroll();

}

void Buzz(){
  tone(buzzer, 1000); // Send 1KHz sound signal...
  delay(100);
  noTone(buzzer);     // Stop sound...
}

int freeRam () {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

void test(uint8_t n) {

  Serial.print("test number ");
  Serial.println(n);
  esp_serial->listen();
  esp->unregisterUDP();
  esp->leaveAP();
  esp->restart();
  //delay(5000);
  while(!esp->kick()) delay(1000);
  Serial.println(esp->joinAP("$WIFI_SSID$", "$WIFI_PASS$"));
  Serial.println(esp->getLocalIP());
  esp->registerUDP("10.0.0.2",40444);
  uint8_t enroller_code[5] = {1, 238, 0, 1, 17}; // 01 EE 00 01 11
  esp->send(enroller_code, 5);
  uint8_t reply_buffer[5];
  if(esp->recv(reply_buffer, 5, 5000)) {
    for (int i=0; i<5; i++) {
      Serial.print(reply_buffer[i]);
      Serial.print(" ");
    }
    uint8_t sending_code[5] = {1, 238, 0, 1, 29}; // 01 EE 00 01 1D
    esp->send(sending_code, 5);
    fps.GetTemplate(0);
    esp_serial->listen();
  } else Serial.print("No answer from server.");
  //uint8_t test_data[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,n};
  //esp->send(test_data, 16);
  esp->unregisterUDP();
  esp->leaveAP();
  
}

bool syncFingerprint(int id) {

  esp_serial->listen();
  esp->unregisterUDP();
  esp->restart();
  while(!esp->kick()) delay(100);
  Serial.println(esp->joinAP("$WIFI_SSID$", "$WIFI_PASS$"));
  Serial.println(esp->getLocalIP());
  esp->registerUDP("10.0.0.2",40444);
  while(esp_serial->available() > 0) esp_serial->read();
  esp_serial->overflow();

  Serial.println(freeRam()); 
  
  bool sync_done = false;
  //bool sync_done = fps.GetTemplate(id);
  uint8_t test_data[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
  while(true) esp->send(test_data, 16);

  Serial.println(freeRam()); 
  
  esp_serial->listen();

  if (esp_serial->overflow()) Serial.println("SoftwareSerial overflow!5");
  else Serial.println("fine for now...5");
  
  while(esp_serial->available() > 0) esp_serial->read();

  while(esp_serial->available() > 0) esp_serial->read();

  Serial.println("All done.");
  Serial.print("Sync was ");
  Serial.println(sync_done);
  if(!sync_done) return 1;
  else return 0;
  
}

void Enroll() {
  
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
  //lcd.print("Enrolling #");
  lcd.print(enrollid);

  // enroll
  Buzz();
  //Serial.print("Press finger to Enroll #");
  //Serial.println(enrollid);
  lcd.setCursor(0,1);
  //lcd.print("  Press finger  ");
  while(fps.IsPressFinger() == false) delay(100);
  bool bret = fps.CaptureFinger(true);
  int iret = 0;
  if (bret != false)
  {
    Buzz();
    //Serial.println("Remove finger");
    lcd.setCursor(0,1);
    //lcd.print(" Remove finger  ");
    fps.Enroll1(); 
    while(fps.IsPressFinger() == true) delay(100);
    Buzz();
    //Serial.println("Press same finger again");
    lcd.setCursor(0,1);
    //lcd.print("  Press finger  ");
    while(fps.IsPressFinger() == false) delay(100);
    bret = fps.CaptureFinger(true);
    if (bret != false)
    {
      Buzz();
      //Serial.println("Remove finger");
      lcd.setCursor(0,1);
      //lcd.print(" Remove finger  ");
      fps.Enroll2();
      while(fps.IsPressFinger() == true) delay(100);
      Buzz();
      //Serial.println("Press same finger yet again");
      lcd.setCursor(0,1);
      //lcd.print("  Press finger  ");
      while(fps.IsPressFinger() == false) delay(100);
      bret = fps.CaptureFinger(true);
      if (bret != false)
      {
        Buzz();
        //Serial.println("Remove finger");
        lcd.setCursor(0,1);
        //lcd.print(" Remove finger  ");
       iret = fps.Enroll3();
        while(fps.IsPressFinger() == true) delay(100);
        if (iret == 0)
        {
          //Serial.println("Enrolling Successful");
          lcd.setCursor(0,1);
          //lcd.print("   Successful   ");
        }
        else
        {
          //Serial.print("Enrolling Failed with error code:");
          //Serial.println(iret);
          lcd.setCursor(0,1);
          //lcd.print("   Failed: #");
          lcd.print(iret);
          //lcd.print("   ");
        }
      }
      else {
        //Serial.println("Failed to capture third finger");
        lcd.setCursor(0,1);
        //lcd.print("Fail: Bad finger");
      }
    }
    else {
      //Serial.println("Failed to capture second finger");
      lcd.setCursor(0,1);
      //lcd.print("Fail: Bad finger");
    }
  }
  else {
    //Serial.println("Failed to capture first finger");
    lcd.setCursor(0,1);
    //lcd.print("Fail: Bad finger");
  }
  delay(3000);
  Buzz();

  lcd.clear();
  lcd.setCursor(0,0);
  //lcd.print("Synchronizing DB");
  for(int i=1; i<=3; i++){
    lcd.setCursor(0,1);
    //lcd.print("Try #");
    lcd.print(i);
    //lcd.print("      ");
    lcd.setCursor(6,1);
    if(syncFingerprint(enrollid)) {
      //lcd.print(": Succesful");
      Buzz();
      delay(3000);
      break;
    } else {
      //lcd.print(": Fail");
      Buzz();
      delay(3000);
    }
  }
  
  lcd.clear();
}

uint8_t n = 1;
void loop()
{
  test(n++);
  delay(100000);
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
