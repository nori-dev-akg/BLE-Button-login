/*
  BLE-Button-login for Windows
  BLE keyboard device for automatic password input by button operation

  - Single click: No action
  - Double click: Send Ctrl key, then send password stored in NVS and Enter
  - Long press (3 seconds or more): Enter password reset mode via serial monitor (NVS update)
  - DeepSleep after 10 seconds of inactivity
  - Built-in LED indicates status
  - Password is stored in NVS using Preferences library

*/

#include <Arduino.h>
#include <BleKeyboard.h>
#include "esp_sleep.h"
#include <Preferences.h>

BleKeyboard bleKeyboard("ESP32BLEButton", "nori-dev-akg", 100);

const int BUTTON_PIN = GPIO_NUM_1; // GPIO1
const int LED_PIN = LED_BUILTIN;  // Built-in LED
const int SLEEP_TIMEOUT = 10;      // DeepSleep timeout (seconds)
const int WAIT_LOGIN = 2000;      // Wait time for login screen (ms)
const int LONG_PRESS_DURATION = 3000; // Long press duration (ms)
const int CONNECT_TIMEOUT = 30;   // Connection timeout (seconds)

unsigned long lastButtonTime = 0;
char pass[32] = ""; // Password buffer

Preferences prefs;
void setPassword();
void clearNVS();
void gotoSleep();

void setup() {

  Serial.begin(115200);
  Serial.println("--------------------------");

  pinMode(BUTTON_PIN, INPUT); // Use pulldown if needed
  pinMode(LED_PIN, OUTPUT);  // Set LED pin as output
  digitalWrite(LED_PIN, HIGH); // Turn off built-in LED

  bleKeyboard.begin(); // Start BLE keyboard
  Serial.print("BLE Keyboard started. Waiting for connection...");
  unsigned long startTime = millis();
  while (!bleKeyboard.isConnected()) {
    if (millis() - startTime > CONNECT_TIMEOUT * 1000) {
      Serial.println("Connection timeout. Please try again.");
      gotoSleep();
      return;
    }
    digitalWrite(LED_PIN, LOW);
    delay(100);
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    Serial.print(".");
  }
  Serial.println("connected!");

  prefs.begin("ble-login", false); // Start NVS

  // Check if password is saved in NVS
  if (prefs.isKey("password")) {
    prefs.getString("password", pass, sizeof(pass));
    //Serial.print("Password loaded from NVS: ");
    //Serial.println(pass);
  } else {
    // Password input
    setPassword();
  }


  lastButtonTime = millis();
}

void loop() {
  static int pressCount = 0;
  static unsigned long lastPressTime = 0;
  static bool wasPressed = false;
  static unsigned long pressStartTime = 0;
  bool isPressed = digitalRead(BUTTON_PIN) == HIGH;
  bool isConnected = bleKeyboard.isConnected();

  digitalWrite(LED_PIN, isConnected ? LOW : HIGH); // Turn on LED if connected

  // Long press detection (3 seconds or more)
  if (isPressed && !wasPressed) {
    pressStartTime = millis();
  }
  if (pressStartTime > 0 && wasPressed) {
    if (millis() - pressStartTime > LONG_PRESS_DURATION) {
      clearNVS(); // Clear NVS & rebboot
      delay(1000);
      ESP.restart();
    }
  }

  // Reset timer when button is pressed
  if (isPressed) {
    lastButtonTime = millis();
  }

  if (isPressed && !wasPressed && isConnected) {
    Serial.println("Pressed");
    unsigned long now = millis();
    if (now - lastPressTime < 500) { // Double click detection within 500ms
      pressCount++;
    } else {
      pressCount = 1;
      delay(100); // Debounce
    }
    lastPressTime = now;

    if (pressCount == 2) {
      Serial.println("Double Pressed");
      // On double click, send Ctrl key
      bleKeyboard.press(KEY_LEFT_CTRL);
      delay(100);
      bleKeyboard.releaseAll();
      delay(WAIT_LOGIN); // Wait for login screen

      bleKeyboard.write((const uint8_t*)pass, strlen(pass)); // Send password
      bleKeyboard.write(KEY_RETURN);
      
      pressCount = 0; // Reset count
    }
  }
  wasPressed = isPressed;

  // DeepSleep after 10 seconds of inactivity (only if not connected to PC)
  if (!Serial && (millis() - lastButtonTime > SLEEP_TIMEOUT * 1000)) {
    gotoSleep();
  }

  delay(10);
}

void setPassword() {
  if (!Serial) return; // Do nothing if serial port is not open

  Serial.println("\n--- Password Set Mode ---");
  Serial.print("Enter new password: ");
  char newPass[32] = "";
  while (Serial.available()) Serial.read(); // Clear buffer
  while (strlen(newPass) == 0) {
    if (Serial.available()) {
      size_t len = Serial.readBytesUntil('\n', newPass, sizeof(newPass) - 1);
      newPass[len] = '\0';
      Serial.print("Password set: ");
      Serial.println(newPass);
      prefs.putString("password", newPass); // Save to NVS
      Serial.println("Password saved to NVS.");
      strncpy(pass, newPass, sizeof(pass)); // Update current buffer
    }
    delay(10);
  }
  delay(1000); // Prevent accidental operation
}

void clearNVS() {
  prefs.begin("ble-login", false);
  prefs.clear();
  prefs.end();
  Serial.println("NVS memory cleared.");
}

void gotoSleep() {
  Serial.println("Going to sleep...");
  bleKeyboard.end(); // End BLE connection
  digitalWrite(LED_PIN, HIGH); // Turn off LED
  delay(500);
  
  esp_sleep_enable_ext1_wakeup(1ULL << BUTTON_PIN, ESP_EXT1_WAKEUP_ANY_HIGH); // Wakeup on HIGH
  esp_deep_sleep_start(); // Start deep sleep
}