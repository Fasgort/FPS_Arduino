#include <RokkitHash.h>
#include <doxygen.h>
#include <ESP8266.h>
#include "SoftwareSerial.h"
#include <FPS_GT511C3.h>

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
  //fps.DeleteAll(); // Clear the DB

  // Do the syncDB routine once. Restart to redo it.
  newSyncDB();
  Buzz(); // Buzz to tell the user the FPS is ready.

}

// Here we go
bool newSyncDB() {

  uint8_t enrolled_count = fps.GetEnrollCount(); // Get the number of enrolled fingerprints (Max 200 for our FPS, if using another FPS, change type to uint16_t).
  uint8_t* hash_array8; // I need to declare this variable outside the if nest
  uint8_t* id_enrolled_array = new uint8_t[enrolled_count]; // This list holds the IDs of the hashed fingerprints, used for deletions
  if (!enrolled_count == 0) {
    uint32_t* hash_array32 = new uint32_t[enrolled_count];
    HashFingerprintDDBB(hash_array32, id_enrolled_array, enrolled_count); // Generate a list of hashes from every existing fingerprint in the FPS
    hash_array8 = ConvertArray32To8(hash_array32, enrolled_count); // ESP only works with arrays of a single byte
    delete hash_array32;
  }

  // TO-DO: Connecthing method that retries until it works

  // Identify yourself to the server and declare intentions
  uint8_t sync_start_code[5] = {1, 253, 0, 1, 219}; // 01 FD 00 01 DB
  esp->send(sync_start_code, 5);

  // Receive the reply
  uint8_t reply_buffer[5];
  esp->recv(reply_buffer, 5, 5000);

  // Check reply
  if (reply_buffer[0] == 1 && reply_buffer[1] == 219 && reply_buffer[4] == 170) {

    if (enrolled_count == 0) {
      // Ask for every fingerprint, no deletions
      uint8_t full_sync_code[5] = {1, 253, 0, 1, 253}; // 01 FD 00 01 FD
      esp->send(full_sync_code, 5);
    } else {
      // Ask for a partial DDBB download
      uint8_t partial_sync_code[5] = {1, 253, 0, 1, 93}; // 01 FD 00 01 5D
      esp->send(partial_sync_code, 5);

      // Send hash list
      esp->send(hash_array8, enrolled_count * 4);
    }

    uint8_t sync_reply_buffer[6] = {0, 0, 0, 0, 0, 0};
    while (sync_reply_buffer[0] == 1 && sync_reply_buffer[1] == 219 && sync_reply_buffer[4] == 13) { // Keep processing received packets until the server is done (Reply = 0D)
      esp->recv(sync_reply_buffer, 6, 5000); // If the DDBB is slow generating the list of fingerprint hashes and replying, this may fail.

      if (sync_reply_buffer[4] == 173) { // Reply = AD (Add new fingerprints)
        uint8_t num_additions = sync_reply_buffer[5];
        uint8_t additions_buffer[num_additions * 4];
        esp->recv(additions_buffer, num_additions * 4, 5000);
        SyncAdd(additions_buffer, num_additions); // Add the required fingerprints
      }

      if (sync_reply_buffer[4] == 222) { // Reply = DE (deletion)
        uint8_t num_deletions = sync_reply_buffer[5];
        uint8_t deletions_buffer[num_deletions * 4];
        esp->recv(deletions_buffer, num_deletions * 4, 5000);
        SyncDelete(deletions_buffer, id_enrolled_array, num_deletions); // Delete the required fingerprints
      }
    }

    delete hash_array8;
    delete id_enrolled_array;
    return true;
  }

  return false;
}

void SyncAdd(uint8_t additions_buffer[], uint8_t num_additions){
  
}

void SyncDelete(uint8_t deletions_buffer[], uint8_t id_array[], uint8_t num_deletions){
  
}

// Generate a list of hashes from every existing fingerprint in the FPS
void HashFingerprintDDBB(uint32_t hash_array32[], uint8_t id_array[], uint8_t enrolled_count) {

  for (uint8_t i = 0; i < 200; i++) {
    if (fps.CheckEnrolled(i)) {

      uint8_t data[500];
      bool sync_failed = true;

      while (sync_failed) { // Infinite bucle if something is wrong with the FPS, still wondering what to do in those cases
        sync_failed = fps.GetTemplate(i, data);
      }

      hash_array32[--enrolled_count] = rokkit((char*)data, 500); // This cast is iffy, but should work
      id_array[enrolled_count] = i;
      if (enrolled_count == 0) break;
    }
  }
}

// Converts an array of 32 bits to 8 bits, big-endian
uint8_t* ConvertArray32To8(uint32_t array32[], uint16_t len) {

  uint8_t* array8 = new uint8_t[len * 4];

  for (uint16_t i = 0; i < len; i++) {
    array8[i * 4] = (array32[i] & 0xff000000) >> 24;
    array8[i * 4 + 1] = (array32[i] & 0x00ff0000) >> 16;
    array8[i * 4 + 2] = (array32[i] & 0x0000ff00) >> 8;
    array8[i * 4 + 3] = (array32[i] & 0x000000ff);
  }

  return array8;
}

void Buzz() {
  tone(buzzer, 1000); // Send 1KHz sound signal...
  delay(100);
  noTone(buzzer);     // Stop sound...
}

// Still deciding how the sync will work
// IDEA: SyncDB generate a list of fingerprint IDs (DDBB IDs)
// that need to be updated.
// The behaviour would be like this:
// > Create a list of hashes with the fingerprints in the FPS
// > Send to the DDBB, that will compare with the local DDBB hashes
// > DDBB sends a list of fingerprints that need to be removed (or nothing)
// > FPS deletes the fingerprints
// > DDBB sends a list of fingerprints that need to be uploaded (IDs only)
// > FPS start asking for each ID, using addFingerprint(ID)
// > Sync is done
// addFingerprint sends a code with the ID and receives the template,
// finally updating it in the FPS
// NOTE: SHOULD USE HASH (obtained from the sync process) INSTEAD OF ID
bool addFingerprint(uint8_t id) {

  // Identify yourself to the server and declare intentions
  uint8_t enroller_code[5] = {1, 253, 0, 1, 34}; // 01 FD 00 01 22
  esp->send(enroller_code, 5);

  // Receive the reply
  uint8_t reply_buffer[5];
  esp->recv(reply_buffer, 5, 5000);

  // Check reply
  bool sync_failed = true;
  if (reply_buffer[0] == 1 && reply_buffer[1] == 219 && reply_buffer[4] == 170) {

    // Ask the DDBB to upload the fingerprint requested
    uint8_t request_code[6] = {1, 253, 0, 1, 48, id};
    esp->send(request_code, 6);

    uint8_t data[498];
    esp->recv(data, 498, 5000);

    // find open enroll id
    int enrollid = 0;
    bool usedid = true;
    while (usedid == true)
    {
      usedid = fps.CheckEnrolled(enrollid);
      if (usedid == true) enrollid++;
    }

    sync_failed = fps.SetTemplate(data, enrollid, true);
    esp_serial->listen();

  }

  if (!sync_failed) return true;
  else return false;

}

// DEBUG: Let's go easy and just ask for an already defined list of fingerprints
void SyncDB() {

  // Get the ESP online with a clean state
  esp_serial->listen();
  while (esp_serial->available() > 0) esp_serial->read();
  while (!esp->kick()) delay(1000);
  esp->unregisterUDP();
  esp->leaveAP();
  esp->restart();
  while (!esp->kick()) delay(1000);

  // Connect to wifi and start a UDP connection
  esp->joinAP(F("$WIFI_SSID$"), F("$WIFI_PASS$"));
  esp->registerUDP(F("10.0.0.2"), 40444);

  // DEBUG - Print the current connection details
  Serial.println(esp->getLocalIP());
  Serial.println(esp->getIPStatus());

  // Get all fingerprints (test)
  for (int i = 1; i <= 10; i++) {
    bool sync = false;
    for (int try_count = 0; try_count < 2; try_count++) {
      sync = addFingerprint(i);
      if (sync) break;
    }
  }

  // Try to leave the ESP offline with a clean state
  while (!esp->kick()) delay(1000);
  while (esp_serial->available() > 0) esp_serial->read();
  esp->unregisterUDP();
  esp->leaveAP();
  esp->restart();
  while (esp_serial->available() > 0) esp_serial->read();
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
  delay(100);
}
