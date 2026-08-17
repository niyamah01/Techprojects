#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <cstdio>

// ====================== YOUR PIN CONFIG ======================
#define OLED_SDA      21
#define OLED_SCL      22
#define OLED_RESET    -1
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

#define BTN_CAND1     15
#define BTN_CAND2     2
#define BTN_CAND3     4
#define BTN_RESULT    16
#define BTN_RESET     17

#define BUZZER_PIN    25
#define LED_STATUS    5

// ====================== SETTINGS ======================
const unsigned long DEBOUNCE_MS = 50;

// ====================== GLOBALS ======================
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

enum SystemState { STATE_WELCOME, STATE_SETUP, STATE_VOTING, STATE_RESULT };
SystemState currentState = STATE_WELCOME;

int votes[3] = {0, 0, 0};
int maxVoters = 10;
int votingTimeSec = 60;
int remainingSeconds = 60;
int setupItem = 0;          // 0 = Max Voters, 1 = Voting Time

unsigned long lastSecondMillis = 0;
unsigned long stateStartMillis = 0;
bool votingActive = true;
bool resultShown = false;

unsigned long lastDebounceTime[5] = {0};
bool lastButtonState[5] = {HIGH, HIGH, HIGH, HIGH, HIGH};
bool buttonState[5]     = {HIGH, HIGH, HIGH, HIGH, HIGH};
const int buttonPins[5] = {BTN_CAND1, BTN_CAND2, BTN_CAND3, BTN_RESULT, BTN_RESET};

// ====================== HELPER FUNCTIONS ======================
void beep(int duration = 70) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(duration);
  digitalWrite(BUZZER_PIN, LOW);
}

void centerText(const char* text, int y, int size = 1) {
  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, y);
  display.print(text);
}

// -------------------- Welcome Screen --------------------
void showWelcome() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);

  centerText("ESP32", 8, 2);
  centerText("VOTING MACHINE", 30, 1);
  centerText("BRAC University", 46, 1);
  centerText("Press any key...", 56, 1);
  display.display();
}

// -------------------- Setup Screen --------------------
void showSetup() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);

  centerText("SETUP MODE", 2, 1);

  // Max Voters
  display.setCursor(10, 18);
  display.print("Max Voters:");
  display.setCursor(95, 18);
  display.print(maxVoters);
  if (setupItem == 0) {
    display.drawRect(90, 16, 30, 12, SSD1306_WHITE);  // highlight
  }

  // Voting Time
  display.setCursor(10, 34);
  display.print("Time (sec):");
  display.setCursor(95, 34);
  display.print(votingTimeSec);
  if (setupItem == 1) {
    display.drawRect(90, 32, 30, 12, SSD1306_WHITE);  // highlight
  }

  // Instructions
  display.setTextSize(1);
  display.setCursor(5, 50);
  display.print("C1-  C2:Switch  C3+");
  display.setCursor(5, 58);
  display.print("RESULT=Start  RESET=Default");

  display.display();
}

// -------------------- Voting Screen (Secret) --------------------
void showVotingScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  centerText("VOTING IN PROGRESS", 2, 1);

  // Big Timer
  display.drawRect(20, 16, 88, 24, SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(36, 20);
  if (remainingSeconds < 10) display.print("0");
  display.print(remainingSeconds);
  display.print("s");

  // Total votes only
  int total = votes[0] + votes[1] + votes[2];
  display.setTextSize(1);
  display.setCursor(22, 48);
  display.print("Votes Cast: ");
  display.print(total);
  display.print("/");
  display.print(maxVoters);

  display.setCursor(35, 58);
  if (votingActive) display.print("Status: OPEN");
  else              display.print("Status: CLOSED");

  display.display();
}

// -------------------- Thank You --------------------
void showThankYou() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);

  centerText("THANK YOU!", 16, 2);
  centerText("Your vote has been", 40, 1);
  centerText("recorded successfully", 52, 1);

  display.display();
  beep(100);
  delay(1100);
}

// -------------------- Result --------------------
void showResult() {
  int total = votes[0] + votes[1] + votes[2];
  int maxVotes = -1;
  int winners = 0;
  int winnerIndex = -1;

  for (int i = 0; i < 3; i++) {
    if (votes[i] > maxVotes) {
      maxVotes = votes[i];
      winnerIndex = i;
      winners = 1;
    } else if (votes[i] == maxVotes) {
      winners++;
    }
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);

  if (total == 0) {
    centerText("NO VOTES", 22, 2);
    centerText("Yet recorded", 48, 1);
  }
  else if (winners > 1) {
    centerText("TIE!", 6, 2);
    centerText("No clear winner", 28, 1);

    display.setTextSize(1);
    display.setCursor(10, 46);
    display.print("C1:");
    display.print(votes[0]);
    display.print("  C2:");
    display.print(votes[1]);
    display.print("  C3:");
    display.print(votes[2]);
  }
  else {
    bool hasMajority = (maxVotes * 2 > total);

    if (hasMajority) centerText("MAJORITY WINNER", 2, 1);
    else             centerText("WINNER (No Majority)", 2, 1);

    char win[8];
    sprintf(win, "C%d", winnerIndex + 1);
    centerText(win, 16, 3);

    char buf[20];
    sprintf(buf, "%d votes", maxVotes);
    centerText(buf, 42, 1);

    display.setTextSize(1);
    display.setCursor(5, 54);
    display.print("C1:");
    display.print(votes[0]);
    display.print("  C2:");
    display.print(votes[1]);
    display.print("  C3:");
    display.print(votes[2]);
  }

  display.display();
  resultShown = true;
  currentState = STATE_RESULT;
  beep(160);
  delay(70);
  beep(160);
}

// -------------------- Start Voting --------------------
void startVoting() {
  votes[0] = votes[1] = votes[2] = 0;
  remainingSeconds = votingTimeSec;
  votingActive = true;
  resultShown = false;
  lastSecondMillis = millis();
  currentState = STATE_VOTING;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  centerText("VOTING STARTED", 20, 1);
  centerText("Good Luck!", 40, 2);
  display.display();
  beep(150);
  delay(1200);
}

// ====================== SETUP ======================
void setup() {
  Serial.begin(115200);

  for (int i = 0; i < 5; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_STATUS, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_STATUS, LOW);

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 failed"));
    for (;;);
  }

  currentState = STATE_WELCOME;
  stateStartMillis = millis();
  showWelcome();
}

// ====================== LOOP ======================
void loop() {
  // ---------- WELCOME STATE ----------
  if (currentState == STATE_WELCOME) {
    // Auto go to setup after 2.5 sec or any button
    bool anyPressed = false;
    for (int i = 0; i < 5; i++) {
      if (digitalRead(buttonPins[i]) == LOW) anyPressed = true;
    }
    if (anyPressed || (millis() - stateStartMillis > 2500)) {
      currentState = STATE_SETUP;
      beep(80);
      showSetup();
      delay(300);
    }
    return;
  }

  // ---------- SETUP STATE ----------
  if (currentState == STATE_SETUP) {
    for (int i = 0; i < 5; i++) {
      bool reading = digitalRead(buttonPins[i]);

      if (reading != lastButtonState[i]) lastDebounceTime[i] = millis();

      if ((millis() - lastDebounceTime[i]) > DEBOUNCE_MS) {
        if (reading != buttonState[i]) {
          buttonState[i] = reading;

          if (buttonState[i] == LOW) {
            beep(40);

            if (i == 0) {          // C1 = Decrease
              if (setupItem == 0) {
                if (maxVoters > 1) maxVoters--;
              } else {
                if (votingTimeSec > 10) votingTimeSec -= 5;
              }
            }
            else if (i == 1) {     // C2 = Switch item
              setupItem = 1 - setupItem;
            }
            else if (i == 2) {     // C3 = Increase
              if (setupItem == 0) {
                if (maxVoters < 50) maxVoters++;
              } else {
                if (votingTimeSec < 300) votingTimeSec += 5;
              }
            }
            else if (i == 3) {     // RESULT = Start Voting
              startVoting();
            }
            else if (i == 4) {     // RESET = Defaults
              maxVoters = 10;
              votingTimeSec = 60;
              setupItem = 0;
              beep(100);
            }

            if (currentState == STATE_SETUP) showSetup();
          }
        }
      }
      lastButtonState[i] = reading;
    }
    return;
  }

  // ---------- VOTING + RESULT STATE ----------
  if (currentState == STATE_VOTING || currentState == STATE_RESULT) {

    // Timer
    if (votingActive && currentState == STATE_VOTING && (millis() - lastSecondMillis >= 1000)) {
      lastSecondMillis = millis();
      if (remainingSeconds > 0) remainingSeconds--;

      if (remainingSeconds == 0) {
        votingActive = false;
        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);
        centerText("TIME OVER", 18, 2);
        centerText("Voting Closed", 42, 1);
        display.display();
        beep(300);
        delay(1500);
      }
    }

    // Buttons
    for (int i = 0; i < 5; i++) {
      bool reading = digitalRead(buttonPins[i]);

      if (reading != lastButtonState[i]) lastDebounceTime[i] = millis();

      if ((millis() - lastDebounceTime[i]) > DEBOUNCE_MS) {
        if (reading != buttonState[i]) {
          buttonState[i] = reading;

          if (buttonState[i] == LOW) {
            digitalWrite(LED_STATUS, HIGH);

            if (i < 3 && votingActive && currentState == STATE_VOTING) {
              int total = votes[0] + votes[1] + votes[2];
              if (total < maxVoters) {
                votes[i]++;
                showThankYou();

                total = votes[0] + votes[1] + votes[2];
                if (total >= maxVoters) {
                  votingActive = false;
                  display.clearDisplay();
                  display.setTextColor(SSD1306_WHITE);
                  centerText("MAX VOTERS", 14, 2);
                  centerText("Reached", 36, 1);
                  centerText("Voting Closed", 52, 1);
                  display.display();
                  beep(280);
                  delay(1800);
                }
              }
            }
            else if (i == 3) {   // RESULT
              showResult();
            }
            else if (i == 4) {   // RESET → back to Setup
              currentState = STATE_SETUP;
              votes[0] = votes[1] = votes[2] = 0;
              resultShown = false;
              votingActive = true;
              showSetup();
              beep(120);
            }

            digitalWrite(LED_STATUS, LOW);
          }
        }
      }
      lastButtonState[i] = reading;
    }

    // Update voting screen
    static unsigned long lastScreenUpdate = 0;
    if (currentState == STATE_VOTING && (millis() - lastScreenUpdate > 250)) {
      showVotingScreen();
      lastScreenUpdate = millis();
    }
  }
}