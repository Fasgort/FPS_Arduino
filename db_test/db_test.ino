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

#include <FPS_GT511C3.h>

// need a serial port to communicate with the GT-511C3
FPS_GT511C3 fps(8, 7, 38400); // Arduino RX (GT TX), Arduino TX (GT RX)
// the Arduino TX pin needs a voltage divider, see wiring diagram at:
// http://startingelectronics.com/articles/GT-511C3-fingerprint-scanner-hardware/

byte rx_byte = 0;        // stores received byte

void setup() {
  // LCD
  
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  // Print a message to the LCD.
  //lcd.print("hello, world!");
  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  lcd.setCursor(0, 1);

  // Buzzer
  pinMode(buzzer, OUTPUT); // Set buzzer - pin 9 as an output

  // FPS
  Serial.begin(115200); //set up Arduino's hardware serial UART
  //fps.UseSerialDebug = true; // so you can see the messages in the serial debug screen
  fps.Open(); //send serial command to initialize fps
  fps.SetLED(true);   //turn on LED so fps can see fingerprint

  //fps.DeleteAll(); // Clear the DB
  //Enroll();
}

void Buzz(){
  tone(buzzer, 1000); // Send 1KHz sound signal...
  delay(100);
  noTone(buzzer);     // Stop sound...
}

void Enroll()
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
}

void loop()
{
  //lcd.print("Waiting finger  ");
  fps.SetLED(true);   //turn on LED so fps can see fingerprint
  delay(1000);
  // Identify fingerprint test
  if (fps.IsPressFinger())
  {
    //fps.GetRawImage();
    //fps.GetImage();
    //fps.GetTemplate(0);
    //fps.DeleteAll();
    Buzz();
    uint8_t* data = new uint8_t[498];
    for(int i = 0; i<498; i++){
      lcd.print(i);
      delay(100);
      while(!Serial.available()) delay(10);
      data[i] = (uint8_t) Serial.read();
    }
    lcd.print(data[497]);
    lcd.print(data[497]);
    lcd.print(data[497]);
    lcd.print(data[497]);
    lcd.print(data[497]);
    Buzz();
    Buzz();
    Buzz();
  }
  else
  {
    fps.SetLED(false);   //turn on LED so fps can see fingerprint
    //Serial.println("Please press finger");
  }
  delay(100);
}

