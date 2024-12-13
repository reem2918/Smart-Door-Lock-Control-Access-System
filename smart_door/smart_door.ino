#include <Adafruit_Fingerprint.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <Servo.h>  // Include Servo library
#include <MFRC522.h>  // Include the MFRC522 library for RFID
#include <Keypad.h>  // Include the Keypad library

// Define pins
#define fingerprintRx 2         // Fingerprint sensor RX pin
#define fingerprintTx 3         // Fingerprint sensor TX pin
#define servoPin 9              // Pin to control the servo
#define SS_PIN 10               // RFID SS pin
#define RST_PIN 9               // RFID Reset pin
#define BUZZER_PIN 4            // Pin to control the buzzer

// Initialize LCD (I2C Address, 16 columns, 2 rows)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Initialize Servo
Servo myServo;

// Setup Fingerprint Sensor
SoftwareSerial mySerial(fingerprintRx, fingerprintTx);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// Setup RFID
MFRC522 rfid(SS_PIN, RST_PIN);  // Create MFRC522 instance

// Door state: 0 = locked, 1 = unlocked
bool doorState = false;

// Timing variables for door locking
unsigned long lastUnlockedTime = 0;
const unsigned long unlockDuration = 3000;  // 10 seconds to keep door unlocked

// Keypad setup
const byte ROWS = 4;  // 4 rows
const byte COLS = 4;  // 4 columns
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {5, 6, 7, 8};  // Row pins connected to Arduino
byte colPins[COLS] = {A0, A1, A2, A3};  // Column pins connected to Arduino

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

String inputPassword = "";  // Store user input
String correctPassword = "1234";  // Set the password

// Define the authorized RFID card UID
byte authorizedUID[4] = {0x33, 0xED, 0x07, 0x2A};  // Replace with your card UID

void setup() {
  Serial.begin(9600);
  while (!Serial);  // Wait for serial monitor to open
  delay(100);
  Serial.println("\n\nAdafruit Fingerprint Test");

  // Initialize LCD with size (16 columns, 2 rows)
  lcd.init();  // Initialize LCD
  lcd.backlight();  // Turn on the backlight initially
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");

  // Initialize fingerprint sensor
  finger.begin(57600);
  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sensor Found");
    delay(2000);
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sensor Error");
    while (1);  // Halt if fingerprint sensor is not found
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Waiting for Finger");

  // Initialize the servo motor
  myServo.attach(servoPin);
  lockDoor();  // Ensure the door starts in the locked position

  // Initialize RFID
  SPI.begin();
  rfid.PCD_Init();
  Serial.println("Scan RFID tag to unlock the door...");

  // Initialize Buzzer
  pinMode(BUZZER_PIN, OUTPUT);  // Set buzzer pin as output
}

void loop() {
  // Continuously check for fingerprints
  getFingerprintID();
  
  // Check for RFID tag detection
  checkRFID();

  // Check keypad input
  checkKeypad();

  // Handle door unlocking time
  if (doorState == true && millis() - lastUnlockedTime > unlockDuration) {
    lockDoor();  // Lock the door after 10 seconds
  }

  delay(50);
}

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();

  if (p == FINGERPRINT_NOFINGER) {
    return p;
  }

  if (p != FINGERPRINT_OK) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error Reading");
    delay(2000);
    lcd.clear();
    lcd.print("Try Again");
    return p;
  }

  // Convert the image to a template
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error Processing");
    delay(2000);
    lcd.clear();
    lcd.print("Try Again");
    return p;
  }

  // Search for a fingerprint match
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Fingerprint match found!");
    unlockDoor("Fingerprint");  // Unlock the door on fingerprint match
    buzz(1);  // Sound the buzzer
    Serial.println("Unlocked via Fingerprint");
  } else {
    Serial.println("No match found!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Fingerprint");
    lcd.setCursor(0, 1);
    lcd.print("Not Found!");
    buzz(0);  // Sound the buzzer for incorrect fingerprint
    delay(2000);
    lcd.clear();
    lcd.print("Try Again");
  }

  return p;
}

// Function to unlock the door
void unlockDoor(String source) {
  Serial.println("Door Unlocked!");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Door Unlocked!");
  lcd.setCursor(0, 1);
  lcd.print("Via " + source);  // Display the unlock source on the LCD
  myServo.attach(servoPin);
  myServo.write(50);  // Move servo to unlocked position
  delay(500);
  myServo.detach();

  doorState = true;
  lastUnlockedTime = millis();
  Serial.println("Unlocked via " + source);
}

// Function to lock the door
void lockDoor() {
  Serial.println("Door Locked!");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Door Locked!");
  myServo.attach(servoPin);
  myServo.write(0);  // Move servo to locked position
  delay(500);
  myServo.detach();

  doorState = false;
}

// Function to check RFID tag
void checkRFID() {
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    bool isAuthorized = true;

    // Compare scanned UID with authorized UID
    for (int i = 0; i < 4; i++) {
      if (rfid.uid.uidByte[i] != authorizedUID[i]) {
        isAuthorized = false;
        break;
      }
    }

    if (isAuthorized) {
      Serial.println("Authorized RFID card detected!");
      unlockDoor("RFID");
      buzz(1);  // Sound the buzzer for authorized RFID
      Serial.println("Unlocked via RFID");
    } else {
      Serial.println("Unauthorized RFID card detected!");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Wrong Card");
      buzz(0);  // Sound the buzzer for unauthorized RFID
      delay(2000);
      lcd.clear();
      lcd.print("Scan RFID Card");
    }

    rfid.PICC_HaltA();  // Stop RFID communication
  }
}

// Function to check keypad input
void checkKeypad() {
  char key = keypad.getKey();

  if (key) {
    Serial.print("Key pressed: ");
    Serial.println(key);

    if (key == '#') {  // Check if the Enter key (#) is pressed
      if (inputPassword == correctPassword) {  // Compare the entered password with the correct one
        Serial.println("Correct password!");
        unlockDoor("Keypad");
        buzz(1);  // Sound the buzzer for correct password
        inputPassword = "";  // Reset input after unlocking
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Correct Password");
        delay(2000);
        lcd.clear();
        lcd.print("Enter Password:");
      } else {
        Serial.println("Incorrect password!");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Wrong Password");
        buzz(0);  // Sound the buzzer for incorrect password
        delay(2000);
        lcd.clear();
        lcd.print("Enter Password:");
        inputPassword = "";  // Reset input after incorrect password
      }
    } else if (key == '*') {  // Check if the Clear key (*) is pressed
      inputPassword = "";  // Reset input
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Password cleared");
      delay(2000);
      lcd.clear();
      lcd.print("Enter Password:");
    } else {
      inputPassword += key;  // Add the pressed key to the password input
      lcd.setCursor(0, 1);  // Move the cursor to the second row

      // Clear the second row and display the correct number of asterisks
      lcd.clear();  
      lcd.setCursor(0, 1);
      for (int i = 0; i < inputPassword.length(); i++) {
        lcd.print("*");  // Print one asterisk for each character entered
      }
    }
  }
}

// Function to control the buzzer
void buzz(bool sound) {
  if (sound) {
    tone(BUZZER_PIN, 1000);  // Buzz at 1000Hz for 200ms
    delay(200);
    noTone(BUZZER_PIN);  // Stop the buzzer
  } else {
    tone(BUZZER_PIN, 500);  // Buzz at 500Hz for wrong action
    delay(200);
    noTone(BUZZER_PIN);  // Stop the buzzer
  }
}