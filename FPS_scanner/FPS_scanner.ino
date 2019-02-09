#include <LiquidCrystal.h>
#include <AES.h>
#include <GCM.h>
#include <RokkitHash.h>
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
const uint8_t buzzer = 9; //buzzer to arduino pin 9

// Touch Interface
const uint8_t touch = 13;

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const uint8_t rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
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

  // Do the syncDB routine once. Restart to redo it.
  SyncDB();
  Buzz(); // Buzz to tell the user the FPS is ready.

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

// Here we go
bool SyncDB() {

  // Connect to the server
  initiateConnection();

  // Identify yourself to the server and declare intentions
  uint8_t* sync_start_code = (uint8_t*) malloc(5 + 28);
  sync_start_code[12] = 1;
  sync_start_code[13] = 253;
  sync_start_code[14] = 0;
  sync_start_code[15] = 1;
  sync_start_code[16] = 34;
  sendEncrypted(sync_start_code, 5); // 01 FD 00 01 22
  free(sync_start_code);

  // Receive the reply
  uint8_t* reply_buffer = receiveEncrypted(5);

  // Check reply
  if (reply_buffer[0] == 1 && reply_buffer[1] == 219 && reply_buffer[4] == 170) {
    free(reply_buffer);

    uint8_t enrolled_count = fps.GetEnrollCount(); // Get the number of enrolled fingerprints (Max 200 for our FPS, if using another FPS, change type to uint16_t).

    if (enrolled_count == 0) {
      // Ask for every fingerprint, no deletions
      uint8_t* full_sync_code = (uint8_t*) malloc(5 + 28);
      full_sync_code[12] = 1;
      full_sync_code[13] = 253;
      full_sync_code[14] = 0;
      full_sync_code[15] = 1;
      full_sync_code[16] = 253;
      sendEncrypted(full_sync_code, 5); // 01 FD 00 01 FD
      free(full_sync_code);
    } else {
      // Ask for a partial DDBB download
      uint8_t* partial_sync_code = (uint8_t*) malloc(6 + 28);
      partial_sync_code[12] = 1;
      partial_sync_code[13] = 253;
      partial_sync_code[14] = 0;
      partial_sync_code[15] = 1;
      partial_sync_code[16] = 93;
      partial_sync_code[17] = enrolled_count;
      sendEncrypted(partial_sync_code, 6); // 01 FD 00 01 5D enrolled_count
      free(partial_sync_code);

      uint8_t last_enrolled = -1;
      uint8_t num_runs = enrolled_count / 8;
      if (enrolled_count % 8) num_runs++;
      uint8_t* id_enrolled_array = (uint8_t*) malloc(enrolled_count); // This list holds the IDs of the hashed fingerprints

      for (uint8_t sync_run = 0; sync_run < num_runs; sync_run++) {

        uint8_t num_fingerprints;

        if (sync_run == num_runs - 1) num_fingerprints = enrolled_count % 8;
        else num_fingerprints = 8;

        uint8_t* hash_array = (uint8_t*) malloc(num_fingerprints * 4 + 28); // Careful with memory here
        HashFingerprintDDBB(hash_array, id_enrolled_array + sync_run * 8, last_enrolled, num_fingerprints); // Generate a list of hashes from every existing fingerprint in the FPS

        // Send hash list
        sendEncrypted(hash_array, num_fingerprints * 4);
        free(hash_array);

      }

      while (true) { // Keep processing received packets until the server is done (Reply = 0D)
        uint8_t* sync_reply_buffer = receiveEncrypted(6); // If the DDBB is slow generating the list of fingerprint hashes and replying, this may fail.

        if (sync_reply_buffer[4] == 222) { // Reply = DE (deletion)
          uint8_t num_deletions = sync_reply_buffer[5];
          uint8_t* deletions_buffer = receiveEncrypted(num_deletions);
          SyncDelete(deletions_buffer, id_enrolled_array, num_deletions); // Delete the required fingerprints
          free(deletions_buffer);
        }

        if (sync_reply_buffer[0] == 1 && sync_reply_buffer[1] == 219 && sync_reply_buffer[4] == 13) {
          free(sync_reply_buffer);
          break;
        } else free(sync_reply_buffer);
      }
      free(id_enrolled_array);

    }

    while (true) { // Keep processing received packets until the server is done (Reply = 0D)
      uint8_t* sync_reply_buffer = receiveEncrypted(6); // If the DDBB is slow generating the list of fingerprint hashes and replying, this may fail.

      if (sync_reply_buffer[4] == 173) { // Reply = AD (Add new fingerprints)
        uint8_t num_additions = sync_reply_buffer[5];
        uint8_t* additions_buffer = receiveEncrypted(num_additions * 4); // Receives a list of what fingerprints must be added (template hashes)
        SyncAdd(additions_buffer, num_additions); // Add the required fingerprints
        free(additions_buffer);
      }

      if (sync_reply_buffer[0] == 1 && sync_reply_buffer[1] == 219 && sync_reply_buffer[4] == 13) {
        free(sync_reply_buffer);
        break;
      } else free(sync_reply_buffer);
    }

    dropConnection();
    return true;
  } else {
    free(reply_buffer);
    dropConnection();
    return false;
  }
}

// Receives a list of positions (that are traslated to in-FPS IDs, that must be deleted
void SyncDelete(uint8_t deletions_buffer[], uint8_t id_array[], uint8_t num_deletions) {
  for (uint8_t i = 0; i < num_deletions; i++) fps.DeleteID(id_array[deletions_buffer[i]]); // Yep, that's it.
}

void SyncAdd(uint8_t additions_buffer[], uint8_t num_additions) {
  for (uint8_t i = 0; i < num_additions; i++) {
    const uint8_t template_hash[4] = {additions_buffer[i * 4], additions_buffer[i * 4 + 1], additions_buffer[i * 4 + 2], additions_buffer[i * 4 + 3]};
    SyncFingerprint(template_hash);
  }
}

bool SyncFingerprint(const uint8_t template_hash[4]) {

  // Ask the DDBB to upload the fingerprint requested
  uint8_t* request_code = (uint8_t*) malloc(9 + 28);
  request_code[12] = 1;
  request_code[13] = 253;
  request_code[14] = 0;
  request_code[15] = 1;
  request_code[16] = 48;
  request_code[17] = template_hash[0];
  request_code[18] = template_hash[1];
  request_code[19] = template_hash[2];
  request_code[20] = template_hash[3];
  sendEncrypted(request_code, 9);
  free(request_code);

  uint8_t* data = receiveEncrypted(500); // 2 bytes ID + 498 data

  // check ID position
  uint16_t enrollid = *((uint16_t*) data);
  if (fps.CheckEnrolled(enrollid)) fps.DeleteID(enrollid);

  bool sync_failed = fps.SetTemplate(data + 2, enrollid, true);
  free(data);

  if (!sync_failed) return true;
  else return false;

}

// Generate a list of hashes from every existing fingerprint in the FPS
// WARNING: Inconsistent use of 8 and 16 bits IDs. Other models of the FPS require 16 bits only. May not fix this yet.
void HashFingerprintDDBB(uint8_t hash_array[], uint8_t id_array[], uint8_t& last_enrolled, uint8_t enrolled_count) {

  uint8_t count = 0;

  do {
    last_enrolled++;
    if (fps.CheckEnrolled(last_enrolled)) {

      uint8_t* data = (uint8_t*) malloc(500 + 2);
      *((uint16_t*) data) = last_enrolled;

      bool sync_failed = true;

      while (sync_failed) { // Infinite bucle if something is wrong with the FPS, still wondering what to do in those cases
        sync_failed = fps.GetTemplate(last_enrolled, data + 2);
      }

      ((uint32_t*) hash_array)[count + 3] = rokkit((char*)data, 500);
      free(data);
      id_array[count++] = last_enrolled;
      if (count == enrolled_count) return;
    }
  } while (last_enrolled < 200);

}

// Send to the server a hash of the fingerprint reading
bool sendFingerprintRead(uint16_t id) {

  // Connect to the server
  initiateConnection();

  // Identify yourself to the server and declare intentions
  uint8_t* read_code = (uint8_t*) malloc(5 + 28);
  read_code[12] = 1;
  read_code[13] = 253;
  read_code[14] = 0;
  read_code[15] = 1;
  read_code[16] = 200;
  sendEncrypted(read_code, 5); // 01 FD 00 01 C8
  free(read_code);

  // Receive the reply
  uint8_t* reply_buffer = receiveEncrypted(5);

  // Send the data if the reply is okay
  uint8_t sync_failed = 1;
  if (reply_buffer[0] == 1 && reply_buffer[1] == 219 && reply_buffer[4] == 170) {
    free(reply_buffer);

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
  while (!esp->joinAP(F("$WIFI_SSID$"), F("$WIFI_PASS$"))) delay(1000);
  while (!esp->registerUDP(F("10.0.0.2"), 40444)) delay(1000);

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

void Buzz() {
  tone(buzzer, 1000); // Send 1KHz sound signal...
  delay(100);
  noTone(buzzer);     // Stop sound...
}

void loop()
{
  lcd.setCursor(0, 0);
  lcd.print(F("Waiting finger  "));

  if (digitalRead(touch)) {
    fps.SetLED(true);   //turn on LED so fps can see fingerprint

    // Identify fingerprint test
    if (fps.IsPressFinger())
    {
      fps.CaptureFinger(false);
      fps.SetLED(false);
      Buzz();
      int id = fps.Identify1_N();

      if (id < 200) //<- change id value depending model you are using
      { //if the fingerprint matches, provide the matching template ID
        Serial.print(F("Verified ID:"));
        Serial.println(id);
        lcd.setCursor(0, 1);
        lcd.print(F("  Found ID #"));
        lcd.print(id);
        sendFingerprintRead(id);
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

  } else fps.SetLED(false);
  delay(100);
}
