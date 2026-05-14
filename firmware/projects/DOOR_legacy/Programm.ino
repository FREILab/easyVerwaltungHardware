#include <SPI.h>
#include "MFRC522.h"
#include <AccelStepper.h>
#include <Ethernet.h>

#define RFID_VALID 1
#define RFID_INVALID 0
#define RFID_FAILED -1

/* 
 *  Pin definitions
 *  
  SPI:
  SCK     = 13
  MISO    = 12
  MOSI    = 11
  SS-Etha = 10
  SS-SD   = 4
  SS-RFID = 6
*/

#define PIN_STEP 2
#define PIN_DIR 3

#define RST_PIN  5  // RST-PIN für RC522 - RFID - SPI - Modul GPIO5 
#define SS_PIN  6  // SDA-PIN für RC522 - RFID - SPI - Modul GPIO4 

#define SER 7
#define RCLK 8
#define SRCLK 9

#define TASTER_ENDSCHALTER A0
#define TASTER_REED A1
#define TASTER_ABSCHLIESSEN A2
#define TASTER_TERASSE A3

// Schiftregister
#define SR_RED 0x1
#define SR_YELLOW 0x2
#define SR_GREEN 0x4
#define SR_SUMMER 0x8
#define SR_MOTOR_ENABLE 0x10
#define SR_WARNING_SUMMER 0x20
#define SR_WARNING_LED 0x40

char srOutput = 0;
char srRed = 0;
char srYellow = 0;
char srGreen = 0;
char srSummer = 0;
char srMotorEnable = 0;

// Network
byte mac[] = {0x74,0xD0,0x2B,0x19,0x0E,0x48};
IPAddress ip(192, 168, 178, 177); //  static IP address to use if the DHCP fails to assign
// String serverIP = "192.168.178.45";  // IP of Server(Raspberry Pi)
// IPAddress server(192,168,178,54);  // IP of Server(Raspberry Pi)
String serverIP = "192.168.178.79";  // IP of Server(Raspberry Pi)
IPAddress server(192,168,178,79);  // IP of Server(Raspberry Pi)
EthernetClient client;

// Stepper
#define STEPPER_SPEED 1200
#define STEPPER_ACCEL 2000
#define STEPS_TO_OPEN 500
#define STEPS_TO_CLOSE 500
#define STEPS_MAX 2000

AccelStepper stepper(1, PIN_STEP, PIN_DIR);

String lastUid;
int retryCount = 0;

// RFID
MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance

void setup() {
  // PinModes
  pinMode(SER, OUTPUT);
  pinMode(RCLK, OUTPUT);
  pinMode(SRCLK, OUTPUT);
  pinMode(TASTER_ENDSCHALTER, INPUT_PULLUP);
  pinMode(TASTER_REED, INPUT_PULLUP);
  pinMode(TASTER_ABSCHLIESSEN, INPUT_PULLUP);

  digitalWrite(SER, LOW);
  digitalWrite(RCLK, LOW);
  digitalWrite(SRCLK, LOW);

  srYellow = 1;
  updateSR();

  // Stepper
  stepper.setMaxSpeed(STEPPER_SPEED);
  stepper.setAcceleration(STEPPER_ACCEL);

  // Serial
  Serial.begin(9600);
  while (!Serial);

  // Ethanet
  Serial.println("Init Ethanet");
  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // try to congifure using IP address instead of DHCP:
    Ethernet.begin(mac, ip);
  }

  // RFID
  Serial.println("Init RFID");
  mfrc522.PCD_Init();

  srYellow = 0;
  updateSR();

  Serial.println("Ready!");
}

char check(String rfid) {
  Serial.println("connecting...");
  // if you get a connection, report back via serial:
  if (client.connect(server, 80)) {
  //if (client.connect(server, 6543)) {
    Serial.println("connected");
    // Make a HTTP request:
    // client.println("GET /lock.php?RFID=" + rfid + " HTTP/1.1");
    client.println("GET /check_key/21042017freilab1337fooboardasgeht111/" + rfid + " HTTP/1.1");
    client.println("Host: " + serverIP);
    client.println("Connection: close");
    client.println();

    // read response
    int numCr = 0;
    String message = "";
    while(client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (c == '\n') numCr++;
        Serial.print(c);
        if (numCr > 6) {
          message += c;  
        }
      }
    }
    Serial.println("disconnecting.");
    client.stop();
    message.trim();
    Serial.println("Response: " + message);
    if (message.equals("<html><head></head><body>true</body></html>")) {
      return RFID_VALID;
    } else {
      return RFID_INVALID;
    }
    
  } else {
    // if you didn't get a connection to the server:
    Serial.println("connection failed");
    return RFID_FAILED;
  }
}

int maintainCounter = 0;

void loop() {
  maintainCounter++;
  if (maintainCounter >= 6000) {
    maintainCounter = 0;
    Ethernet.maintain();
  }
  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    if(digitalRead(TASTER_ABSCHLIESSEN) == 0) {
      closeDoor();
    }
    delay(50);
    return;
  }
  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    delay(50);
    return;
  }
  srYellow = 1;
  updateSR();
  // Show some details of the PICC (that is: the tag/card)
  Serial.print(F("Card UID:"));
  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 16){
      uid = uid + "0";
    }
    uid = uid + String(mfrc522.uid.uidByte[i], 16);
    if (i < mfrc522.uid.size - 1 ) {
      uid = uid + ":";
    }
  }
  Serial.println(uid);
  char result = check(uid);
  srYellow = 0;
  updateSR();
  if (result == RFID_VALID) {
    if (lastUid.equals(uid)) {
      retryCount++;
    }
    if (retryCount == 2) {
      openDoor(true);
    } else {
      openDoor(false);
    }
  } else if (result == RFID_INVALID){
    retryCount = 0;
    lastUid = "";
    srRed = 1;
    updateSR();
    delay(2000);
    srRed = 0;
    updateSR();
  } else {
    // ...
  }
  lastUid = uid;
  mfrc522.PCD_Init(); 
}

void openDoor(boolean forceOpening) {
  if (digitalRead(TASTER_ENDSCHALTER) == 0 || forceOpening){
    srMotorEnable = 1;
    srGreen = 1;
    updateSR();

    stepper.setSpeed(STEPPER_SPEED);
    while(digitalRead(TASTER_ENDSCHALTER) == 0){
      stepper.runSpeed();
    }
    long pos = stepper.currentPosition();

    while(stepper.currentPosition() - pos < STEPS_TO_OPEN){
      stepper.runSpeed();
    }
    stepper.setSpeed(0);
          
    srMotorEnable = 0;
    updateSR();
  }
  srGreen = 1;
  srSummer = 1;
  updateSR();
  delay(2000);
  srGreen = 0;
  srSummer = 0;
  updateSR();
}

void closeDoor() {
  lastUid = "";
  retryCount = 0;
  while (digitalRead(TASTER_REED));
  delay(2000);
  
  srMotorEnable = 1;
  srGreen = 1;
  srYellow = 1;
  srRed = 1;
  updateSR();

  long posStart = stepper.currentPosition();
  stepper.setSpeed(-STEPPER_SPEED);
  while(digitalRead(TASTER_ENDSCHALTER) == 1 && posStart - stepper.currentPosition() < STEPS_MAX){
    stepper.runSpeed();
  }

  long pos = stepper.currentPosition();

  while(pos - stepper.currentPosition() < STEPS_TO_CLOSE){
    stepper.runSpeed();
  }

  srMotorEnable = 0;
  srGreen = 0;
  srYellow = 0;
  srRed = 0;
  updateSR();
  delay(500);
  srGreen = 1;
  srYellow = 1;
  srRed = 1;
  updateSR();
  delay(500);
  srGreen = 0;
  srYellow = 0;
  srRed = 0;
  updateSR();
  delay(500);
  srGreen = 1;
  srYellow = 1;
  srRed = 1;
  updateSR();
  delay(500);
  srGreen = 0;
  srYellow = 0;
  srRed = 0;
  updateSR();
}

void updateSR() {
  digitalWrite(SRCLK, LOW);
  digitalWrite(RCLK, LOW);
  
  if (srMotorEnable == 1){
    digitalWrite(SER, LOW);
  } else {
    digitalWrite(SER, HIGH);
  }
  digitalWrite(SRCLK, HIGH);
  digitalWrite(SRCLK, LOW);

  if (srSummer == 0){
    digitalWrite(SER, HIGH);
  } else {
    digitalWrite(SER, LOW);
  }
  digitalWrite(SRCLK, HIGH);
  digitalWrite(SRCLK, LOW);

  if (srGreen == 0){
    digitalWrite(SER, HIGH);
  } else {
    digitalWrite(SER, LOW);
  }
  digitalWrite(SRCLK, HIGH);
  digitalWrite(SRCLK, LOW);

  if (srYellow == 0){
    digitalWrite(SER, HIGH);
  } else {
    digitalWrite(SER, LOW);
  }
  digitalWrite(SRCLK, HIGH);
  digitalWrite(SRCLK, LOW);

  if (srRed == 0){
    digitalWrite(SER, HIGH);
  } else {
    digitalWrite(SER, LOW);
  }
  digitalWrite(SRCLK, HIGH);
  digitalWrite(SRCLK, LOW);

  digitalWrite(RCLK, HIGH);
  digitalWrite(RCLK, LOW);
}
