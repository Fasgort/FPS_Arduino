#include <LiquidCrystal.h>
#include <AES.h>
#include <GCM.h>
#include <doxygen.h>
#include <ESP8266.h>
#include "SoftwareSerial.h"
#include <FPS_GT511C3.h>
#include "limits.h"


// SoftwareSerial for ESP8266
SoftwareSerial *esp_serial = new SoftwareSerial(A0, A1); // RX, TX
ESP8266 *esp = new ESP8266(*esp_serial, 9600);

// need a serial port to communicate with the GT-511C3
FPS_GT511C3 fps(8, 7, 57600); // Arduino RX (GT TX), Arduino TX (GT RX)
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

// Touch Interface
const uint8_t touch = 13;

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// AES256 - GCM
GCM<AESTiny128> gcm;
const uint8_t key[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6};

void setup() {

  // RandomSeed
  randomSeed(analogRead(A2) * 16777216 + analogRead(A3) * 65536 + analogRead(A4) * 256 + analogRead(A5)); // Unused pins

  // Debug
  Serial.begin(115200);
  Serial.println(F("Start"));

  // LCD
  lcd.begin(16, 2);

  // Buzzer
  pinMode(buzzer, OUTPUT); // Set buzzer - pin 9 as an output

  // Touch Interface
  pinMode(touch, INPUT); // This pin notifies Arduino of FPS touch

  // FPS
  fps.Open(); //send serial command to initialize fps
  //fps.DeleteAll(); // Clear the DB

  // AES256-GCM cypher
  gcm.setKey(key, sizeof(key));

  // Do the enroll routine once. Restart to redo it.
  Enroll();

}

void Buzz() {
  tone(buzzer, 1000); // Send 1KHz sound signal...
  delay(100);
  noTone(buzzer);     // Stop sound...
}

// Method to send simple code messages to the server
// Data buffer must have 28 additional bytes reserved for IV+tag, with the actual data starting at byte 13
void sendEncrypted(uint8_t data[], const uint16_t unencrypted_len) {

  // Listen to ESP8266 serial channel
  esp_serial->listen();

  // Generate random IV
  *((uint32_t*) data) = (unsigned long) random(LONG_MAX) + LONG_MIN;
  *((uint32_t*) data + 1) = (unsigned long) random(LONG_MAX) + LONG_MIN;
  *((uint32_t*) data + 2) = (unsigned long) random(LONG_MAX) + LONG_MIN;

  gcm.setIV(data, 12);
  gcm.encrypt(data + 12, data + 12, unencrypted_len);
  gcm.computeTag(data + 12 + unencrypted_len, 16);

  esp->send(data, 28 + unencrypted_len);
}

// Method to receive encrypted data and convert to usable data
uint8_t* receiveEncrypted(const uint16_t unencrypted_len) {

  // Listen to ESP8266 serial channel
  esp_serial->listen();

  uint8_t* _buffer = (uint8_t*) malloc(unencrypted_len + 28); // Encrypted data has additional 12 bytes of IV + 16 bytes of tag
  esp->recv(_buffer, unencrypted_len + 28, 5000);

  gcm.setIV(_buffer, 12); // IV is stored in the first 12 bytes
  gcm.decrypt(_buffer, _buffer + 12, unencrypted_len);
  if (!gcm.checkTag(_buffer + 12 + unencrypted_len, 16)) {
    // Invalid data
    free(_buffer);
    return 0;
  } else {
    realloc(_buffer, unencrypted_len);
    return _buffer;
  }
}

bool syncFingerprint(uint16_t id) {

  // Connect to the server
  initiateConnection();

  // Identify yourself to the server and declare intentions
  uint8_t* enroller_code = (uint8_t*) malloc(5 + 28);
  enroller_code[12] = 1;
  enroller_code[13] = 238;
  enroller_code[14] = 0;
  enroller_code[15] = 1;
  enroller_code[16] = 17;
  sendEncrypted(enroller_code, 5); // 01 EE 00 01 11
  free(enroller_code);

  // Receive the reply
  uint8_t* reply_buffer = receiveEncrypted(5); // Don't forget to DELETE

  // Send the data if the reply is okay
  uint8_t sync_failed = 1;
  if (reply_buffer[0] == 1 && reply_buffer[1] == 219 && reply_buffer[4] == 170) {
    free(reply_buffer);

    // Initiate sync fingerprint process in the server
    uint8_t* sending_code = (uint8_t*) malloc(5 + 28);
    sending_code[12] = 1;
    sending_code[13] = 238;
    sending_code[14] = 0;
    sending_code[15] = 1;
    sending_code[16] = 29;
    sendEncrypted(sending_code, 5); // 01 EE 00 01 1D
    free(sending_code);

    uint8_t* data = (uint8_t*) malloc(500 + 28 + 2); // Template (498 bytes) + 2 checksum bytes + 28 bytes for GCM cypher + 2 ID
    *((uint16_t*) data + 6) = id; // Set the ID used for the fingerprint after the IV code

    while (sync_failed) { // Infinite bucle if something is wrong with the FPS, still wondering what to do in those cases
      sync_failed = fps.GetTemplate(id, data + 14); // FPS will get the fingerprint and send it to the ESP
    }

    if (!sync_failed) sendEncrypted(data, 502);
    free(data);

  } else free(reply_buffer);

  // Disconnect
  dropConnection();

  if (!sync_failed) return true;
  else return false;

}

void Enroll() {

  // find open enroll id
  int enrollid = 0;
  bool usedid = true;
  while (usedid == true)
  {
    usedid = fps.CheckEnrolled(enrollid);
    if (usedid == true) enrollid++;
  }
  fps.EnrollStart(enrollid);
  lcd.setCursor(0, 0);
  lcd.print(F("Enrolling #"));
  lcd.print(enrollid);

  // Enroll
  bool bret;
  int iret = 0;
  Buzz();
  lcd.setCursor(0, 1);
  lcd.print(F("  Press finger  "));

  while (true) {
    if (digitalRead(touch)) {
      fps.SetLED(true);
      if (fps.IsPressFinger()) {
        bret = fps.CaptureFinger(true);
        if (bret != false) fps.Enroll1();
        Buzz();
        break;
      }
    } else {
      fps.SetLED(false);
      delay(100);
    }
  }

  if (bret != false)
  {
    lcd.setCursor(0, 1);
    lcd.print(F(" Remove finger  "));
    while (fps.IsPressFinger() == true) delay(100);
    Buzz();
    lcd.setCursor(0, 1);
    lcd.print(F("  Press finger  "));

    while (true) {
      if (digitalRead(touch)) {
        fps.SetLED(true);
        if (fps.IsPressFinger()) {
          bret = fps.CaptureFinger(true);
          if (bret != false) fps.Enroll2();
          Buzz();
          break;
        }
      } else {
        fps.SetLED(false);
        delay(100);
      }
    }

    if (bret != false)
    {
      lcd.setCursor(0, 1);
      lcd.print(F(" Remove finger  "));
      while (fps.IsPressFinger() == true) delay(100);
      Buzz();
      lcd.setCursor(0, 1);
      lcd.print(F("  Press finger  "));

      while (true) {
        if (digitalRead(touch)) {
          fps.SetLED(true);
          if (fps.IsPressFinger()) {
            bret = fps.CaptureFinger(true);
            if (bret != false) iret = fps.Enroll3();
            Buzz();
            break;
          }
        } else {
          fps.SetLED(false);
          delay(100);
        }
      }

      if (bret != false)
      {
        lcd.setCursor(0, 1);
        lcd.print(F(" Remove finger  "));
        while (fps.IsPressFinger() == true) delay(100);
        Buzz();
        if (iret == 0)
        {
          lcd.setCursor(0, 1);
          lcd.print(F("   Successful   "));

          fps.SetLED(false);   //turn off LED
          delay(3000);
          Buzz();
          lcd.clear();

          // Synchronize the fingerprint with the DB
          lcd.setCursor(0, 0);
          lcd.print(F("Synchronizing DB"));
          for (int i = 1; i <= 3; i++) {
            lcd.setCursor(0, 1);
            lcd.print(F("Try #"));
            lcd.print(i);
            lcd.print(F("      "));
            lcd.setCursor(6, 1);
            if (syncFingerprint(enrollid)) {
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
          lcd.setCursor(0, 1);
          lcd.print(F("   Failed: #"));
          lcd.print(iret);
          lcd.print(F("   "));
        }
      }
      else {
        lcd.setCursor(0, 1);
        lcd.print(F("Fail: Bad finger"));
      }
    }
    else {
      lcd.setCursor(0, 1);
      lcd.print(F("Fail: Bad finger"));
    }
  }
  else {
    lcd.setCursor(0, 1);
    lcd.print(F("Fail: Bad finger"));
  }
  fps.SetLED(false);   //turn off LED
  delay(3000);
  Buzz();
  lcd.clear();
}

void initiateConnection() {
  // Get the ESP online with a clean state
  esp_serial->listen();
  while (esp_serial->available() > 0) esp_serial->read();
  while (!esp->kick()) delay(1000);
  esp->unregisterUDP();
  esp->leaveAP();
  esp->restart();
  while (!esp->kick()) delay(1000);

  // Connect to wifi and start a UDP connection
  // Retry steps if failed
  while (!esp->joinAP(F("$WIFI_SSID$", "$WIFI_PASS$")));
  while (!esp->registerUDP(F("10.0.0.2"), 40444));

  // DEBUG - Print the current connection details
  Serial.println(esp->getLocalIP());
  Serial.println(esp->getIPStatus());
}

void dropConnection() {
  // Try to leave the ESP offline with a clean state
  esp_serial->listen();
  while (!esp->kick()) delay(1000);
  while (esp_serial->available() > 0) esp_serial->read();
  esp->unregisterUDP();
  esp->leaveAP();
  esp->restart();
  while (esp_serial->available() > 0) esp_serial->read();
}

void loop()
{
  delay(100000); // Done, shutdown or restart
}
