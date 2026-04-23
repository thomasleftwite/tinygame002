#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <esp_now.h>
#include "config.h"

// --- Game Constants ---
#define NUM_SWITCHES 3
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// --- Game States ---
enum GameState {
  PLAY_MODE,
  WAITING_MODE,
  BOMBED_MODE
};

// --- Variables ---
GameState currentState = PLAY_MODE;
bool switches[NUM_SWITCHES] = {false, false, false};  // true = ON
int cursorIndex = 0;  // Currently selected switch (0 to NUM_SWITCHES-1)
bool alertStatus = false;

// ESP-NOW Data
typedef struct {
  bool switchState[NUM_SWITCHES];  // All switch states
} Message;
Message myMsg;

// Hardware
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_NeoPixel pixels(NUM_LEDS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// Sound Timer
unsigned long soundEndTime = 0;
bool isBooming = false;

// Alert Sound
bool alertSoundPending = false;
unsigned long alertSoundEndTime = 0;
int alertBeepCount = 0;
unsigned long alertBeepNextTime = 0;

// Blink Timer
unsigned long lastBlinkToggle = 0;
bool blinkOn = false;

// Bombed Mode Info
bool iAmLoser = false;  // true if I triggered the explosion

// --- Forward Declarations ---
void startBeep();
void startBoom();
void startAlertSound();
void updateSound();
void updatePixels();
void drawDisplay();
bool allSwitchesOn();
int countOffSwitches();

// --- ESP-NOW Callbacks ---
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Send status (can add logging here)
}

void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  if (currentState == BOMBED_MODE) return;  // Ignore if already game over

  Message received;
  memcpy(&received, incomingData, sizeof(received));

  // Update local switch states
  for (int i = 0; i < NUM_SWITCHES; i++) {
    switches[i] = received.switchState[i];
  }

  if (currentState == WAITING_MODE) {
    if (allSwitchesOn()) {
      // Another player triggered the explosion — I survived!
      currentState = BOMBED_MODE;
      iAmLoser = false;
      startBoom();
    } else {
      // Check alert condition
      bool wasAlert = alertStatus;
      if (countOffSwitches() == 1) {
        alertStatus = true;
        if (!wasAlert) {
          startAlertSound();
        }
      }
      // Move to PLAY_MODE
      currentState = PLAY_MODE;
      if (!wasAlert || !alertStatus) {
        startBeep();
      }
    }
  }
}

// --- Sound Logic (Non-blocking) ---
void startBeep() {
  tone(BUZZER_PIN, 1000);
  soundEndTime = millis() + 100;
  isBooming = false;
}

void startBoom() {
  isBooming = true;
  soundEndTime = millis() + 1000;
}

void startAlertSound() {
  // "ピピピッ！" — 3 short beeps
  alertSoundPending = true;
  alertBeepCount = 0;
  alertBeepNextTime = millis();
}

void updateSound() {
  // Handle alert beep sequence
  if (alertSoundPending) {
    if (millis() >= alertBeepNextTime) {
      if (alertBeepCount < 3) {
        if (alertBeepCount % 1 == 0) {  // Even: beep on
          tone(BUZZER_PIN, 2000);
          alertBeepNextTime = millis() + 80;
        }
        alertBeepCount++;
        if (alertBeepCount < 3) {
          // Schedule gap
          alertBeepNextTime = millis() + 120;
        } else {
          // Final beep off
          alertSoundPending = false;
          soundEndTime = millis() + 80;
        }
      }
    }
    return;  // Don't process normal sound while alert is playing
  }

  if (millis() > soundEndTime) {
    noTone(BUZZER_PIN);
    isBooming = false;
  } else if (isBooming) {
    // Generate explosion sound by rapid frequency changes
    tone(BUZZER_PIN, random(100, 2000));
  }
}

// --- Helper Functions ---
bool allSwitchesOn() {
  for (int i = 0; i < NUM_SWITCHES; i++) {
    if (!switches[i]) return false;
  }
  return true;
}

int countOffSwitches() {
  int count = 0;
  for (int i = 0; i < NUM_SWITCHES; i++) {
    if (!switches[i]) count++;
  }
  return count;
}

// --- NeoPixel Update ---
unsigned long lastPixelUpdate = 0;
void updatePixels() {
  if (millis() - lastPixelUpdate < 30) return;
  lastPixelUpdate = millis();

  pixels.clear();
  uint32_t color = pixels.Color(0, 0, 0);

  switch (currentState) {
    case PLAY_MODE:
    case WAITING_MODE:
      if (alertStatus) {
        // Cyclic blink Amber (0.5s period = 250ms on, 250ms off)
        if ((millis() / 250) % 2 == 0) {
          color = pixels.Color(255, 160, 0);  // Amber
        } else {
          color = pixels.Color(0, 0, 0);
        }
      } else {
        color = pixels.Color(0, 255, 0);  // Green
      }
      break;

    case BOMBED_MODE:
      // Blink Red (0.5s period)
      if ((millis() / 250) % 2 == 0) {
        color = pixels.Color(255, 0, 0);
      } else {
        color = pixels.Color(0, 0, 0);
      }
      break;
  }

  for (int i = 0; i < NUM_LEDS; i++) pixels.setPixelColor(i, color);
  pixels.show();
}

// --- OLED Display ---
void drawDisplay() {
  display.clearDisplay();

  switch (currentState) {
    case PLAY_MODE:
      drawSwitches();
      break;

    case WAITING_MODE:
      drawWaiting();
      break;

    case BOMBED_MODE:
      drawBombed();
      break;
  }

  display.display();
}

void drawSwitches() {
  // Draw switch numbers in a row, centered on screen
  // Each switch is displayed as a box with its number
  int boxW = 30;
  int boxH = 30;
  int gap = 8;
  int totalW = NUM_SWITCHES * boxW + (NUM_SWITCHES - 1) * gap;
  int startX = (SCREEN_WIDTH - totalW) / 2;
  int startY = (SCREEN_HEIGHT - boxH) / 2;

  // Title
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("SELECT SWITCH");

  if (alertStatus) {
    display.setCursor(SCREEN_WIDTH - 6*6, 0);  // Right-aligned
    display.print("ALERT!");
  }

  for (int i = 0; i < NUM_SWITCHES; i++) {
    int x = startX + i * (boxW + gap);
    int y = startY;

    if (i == cursorIndex) {
      // Inverted (filled) for selected switch
      display.fillRect(x, y, boxW, boxH, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      // Normal (outlined)
      display.drawRect(x, y, boxW, boxH, SSD1306_WHITE);
      display.setTextColor(SSD1306_WHITE);
    }

    // Draw switch number centered in box
    display.setTextSize(2);
    int numX = x + (boxW - 12) / 2;  // 12 = char width at size 2
    int numY = y + (boxH - 16) / 2;  // 16 = char height at size 2
    display.setCursor(numX, numY);
    display.print(i + 1);
  }

  // Reset text color
  display.setTextColor(SSD1306_WHITE);
}

void drawWaiting() {
  // Blink "WAITING" text
  if ((millis() / 500) % 2 == 0) {
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    // Center text
    int textW = 7 * 12;  // 7 chars * 12px (size 2)
    int x = (SCREEN_WIDTH - textW) / 2;
    int y = (SCREEN_HEIGHT - 16) / 2;
    display.setCursor(x, y);
    display.print("WAITING");
  }
}

void drawBombed() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (iAmLoser) {
    // I triggered the explosion
    display.setTextSize(2);
    int textW = 10 * 12;  // "You lose..." = 11 chars
    int x = (SCREEN_WIDTH - 11 * 12) / 2;
    int y = (SCREEN_HEIGHT - 16) / 2;
    display.setCursor(x > 0 ? x : 0, y);
    display.print("You lose..");
  } else {
    // Someone else triggered it — I survived
    display.setTextSize(1);
    // Line 1
    const char* line1 = "*** YOU ***";
    int w1 = strlen(line1) * 6;
    display.setCursor((SCREEN_WIDTH - w1) / 2, SCREEN_HEIGHT / 2 - 12);
    display.print(line1);
    // Line 2
    display.setTextSize(2);
    const char* line2 = "SURVIVED";
    int w2 = strlen(line2) * 12;
    display.setCursor((SCREEN_WIDTH - w2) / 2, SCREEN_HEIGHT / 2 - 2);
    display.print(line2);
  }
}

// --- Setup ---
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(2000);
  Serial.println("Starting tinygame002 - Bomb Pass Game");

  // Pixels
  Serial.println("Initializing Pixels...");
  pixels.begin();
  pixels.setBrightness(50);
  // Startup Test: White flash
  for (int i = 0; i < NUM_LEDS; i++) pixels.setPixelColor(i, 255, 255, 255);
  pixels.show();
  delay(500);
  pixels.clear();
  pixels.show();

  // OLED
  Serial.println("Initializing OLED...");
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
  }
  display.clearDisplay();
  display.display();

  // Buttons
  Serial.println("Configuring Buttons...");
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_LEFT, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);

  // ESP-NOW
  Serial.println("Initializing ESP-NOW...");
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
  } else {
    esp_now_register_send_cb((esp_now_send_cb_t)onDataSent);
    esp_now_register_recv_cb(onDataRecv);

    // Add Broadcast Peer
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Failed to add peer");
    }
  }

  // Init switches — all OFF
  for (int i = 0; i < NUM_SWITCHES; i++) {
    switches[i] = false;
  }

  Serial.println("Setup Complete. Game Ready!");
}

// --- Main Loop ---
void loop() {
  updateSound();
  updatePixels();
  drawDisplay();

  // --- Button Reading ---
  static bool btnUpPrev = HIGH;
  static bool btnDownPrev = HIGH;
  static bool btnLeftPrev = HIGH;
  static bool btnRightPrev = HIGH;

  // Long press tracking for BUTTON_UP (reset)
  static unsigned long upPressTime = 0;
  static bool upIsPressed = false;

  // Auto-repeat tracking for LEFT/RIGHT
  static unsigned long leftRepeatTime = 0;
  static unsigned long rightRepeatTime = 0;

  bool btnUp = digitalRead(BUTTON_UP);
  bool btnDown = digitalRead(BUTTON_DOWN);
  bool btnLeft = digitalRead(BUTTON_LEFT);
  bool btnRight = digitalRead(BUTTON_RIGHT);

  // =============================================
  // BUTTON_UP — Long Press Reset (ALL states)
  // =============================================
  if (btnUp == LOW && btnUpPrev == HIGH) {
    upIsPressed = true;
    upPressTime = millis();
    delay(20);
  } else if (btnUp == LOW && upIsPressed) {
    if (millis() - upPressTime > 1500) {
      Serial.println("Resetting...");
      ESP.restart();
    }
  } else if (btnUp == HIGH && upIsPressed) {
    upIsPressed = false;
    delay(20);
  }

  // In BOMBED_MODE or WAITING_MODE, only reset is active
  if (currentState == BOMBED_MODE || currentState == WAITING_MODE) {
    btnUpPrev = btnUp;
    btnDownPrev = btnDown;
    btnLeftPrev = btnLeft;
    btnRightPrev = btnRight;
    return;
  }

  // =============================================
  // PLAY_MODE Controls
  // =============================================

  // --- LEFT Button: cursor move left (with auto-repeat) ---
  if (btnLeft == LOW && btnLeftPrev == HIGH) {
    cursorIndex = (cursorIndex - 1 + NUM_SWITCHES) % NUM_SWITCHES;
    leftRepeatTime = millis() + 400;
    delay(20);
  } else if (btnLeft == LOW && millis() > leftRepeatTime) {
    cursorIndex = (cursorIndex - 1 + NUM_SWITCHES) % NUM_SWITCHES;
    leftRepeatTime = millis() + 100;
  }

  // --- RIGHT Button: cursor move right (with auto-repeat) ---
  if (btnRight == LOW && btnRightPrev == HIGH) {
    cursorIndex = (cursorIndex + 1) % NUM_SWITCHES;
    rightRepeatTime = millis() + 400;
    delay(20);
  } else if (btnRight == LOW && millis() > rightRepeatTime) {
    cursorIndex = (cursorIndex + 1) % NUM_SWITCHES;
    rightRepeatTime = millis() + 100;
  }

  // --- DOWN Button: click the selected switch ---
  if (btnDown == LOW && btnDownPrev == HIGH) {
    // Toggle the selected switch
    switches[cursorIndex] = !switches[cursorIndex];

    Serial.print("Switch ");
    Serial.print(cursorIndex + 1);
    Serial.print(" toggled to ");
    Serial.println(switches[cursorIndex] ? "ON" : "OFF");

    // Check if all switches are ON — boom!
    if (allSwitchesOn()) {
      // I triggered the explosion — I lose
      currentState = BOMBED_MODE;
      iAmLoser = true;

      // Still send the state so others know
      for (int i = 0; i < NUM_SWITCHES; i++) {
        myMsg.switchState[i] = switches[i];
      }
      esp_now_send(NULL, (uint8_t *)&myMsg, sizeof(myMsg));

      startBoom();
    } else {
      // Send switch states and go to WAITING_MODE
      for (int i = 0; i < NUM_SWITCHES; i++) {
        myMsg.switchState[i] = switches[i];
      }
      esp_now_send(NULL, (uint8_t *)&myMsg, sizeof(myMsg));

      currentState = WAITING_MODE;
      startBeep();

      Serial.println("Switch states sent. Waiting for other player...");
    }

    delay(20);
  }

  // --- Update previous button states ---
  btnUpPrev = btnUp;
  btnDownPrev = btnDown;
  btnLeftPrev = btnLeft;
  btnRightPrev = btnRight;
}
