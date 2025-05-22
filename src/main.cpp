#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

// initialize pins
#define BUTTON_PIN A3
#define BUTTON_BIT PC3 // A3 pin on Arduino Uno
#define BUZZER_PIN 10
#define SR_DATA_PIN 11
#define SR_LATCH_PIN 12
#define SR_CLK_PIN 13

// button variables
volatile bool buttonPressed = false;
volatile bool buttonWasReleased = true;
volatile unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 150;

// system state variables
volatile bool powerOn = true;
volatile bool idleState = true;

// LED idle variables
int ledIndex = 0;
unsigned long lastLedUpdateTime = 0;
const unsigned long ledInterval = 500;

int difficulty = 0;

// initialize LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Keypad setup
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {9, 8, 7, 6}; 
byte colPins[COLS] = {5, 4, 3, 2};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// define buzzer frequencies
const int wrongSound = 300;
const int rightSound = 1000;
const int keyPressSound = 500;
const int buzzerDuration = 200;

// initialize the easy/hard difficulty arrays
const int minLength = 4;
int currLength = 4;
int easyHighScore = 0;
int hardHighScore = 0;
uint16_t easy[32];
uint16_t hard[32];

// put function declarations here:
void sendToShiftRegister(uint16_t data);
void printIdleMessage();
void printWrongMessage();
void printCorrectMessage();
void printLevelMessage();

void setup() {
  // put your setup code here, to run once:
  lcd.init();
  lcd.backlight();
  printIdleMessage();

  pinMode(BUZZER_PIN, OUTPUT);

  DDRC &= ~(1 << BUTTON_BIT); // set as input
  PORTC |= (1 << BUTTON_BIT); // enable pull-up resistor

  PCICR |= (1 << PCIE1); // enable pin change interrupt
  PCMSK1 |= (1 << PCINT11); // enable interrupt for BUTTON_BIT
  sei(); // enable global interrupts

  // initialize the shift register pins
  pinMode(SR_DATA_PIN, OUTPUT);
  pinMode(SR_LATCH_PIN, OUTPUT);
  pinMode(SR_CLK_PIN, OUTPUT);

  sendToShiftRegister(0);

  randomSeed(analogRead(A0));
  for (int i = 0; i < currLength; i++) {
    easy[i] = random(0, 8);
    hard[i] = random(0, 8);
  }
}

void loop() {
  if (!powerOn) {
    char key = keypad.getKey();
    if (key == '*') {
      tone(BUZZER_PIN, keyPressSound, 100);
      powerOn = true;
      lcd.backlight();
      printIdleMessage();
    }

    return;
  }

  if (!idleState) {
    for (int i = 0; i < currLength; i++) {
      sendToShiftRegister(0);
      delay(300);
      if (difficulty == 0) {
        sendToShiftRegister(1 << easy[i]);
      } else {
        sendToShiftRegister(1 << hard[i]);
      }
      delay(300);
    }
    sendToShiftRegister(0);

    for (int i = 0; i < currLength; i++) {
      char keyPressed = keypad.waitForKey();
      uint16_t keyInt = keyPressed - '0';

      tone(BUZZER_PIN, keyPressSound, 100);

      if (((keyInt != easy[i] + 1) && (difficulty == 0)) ||
          ((keyInt != hard[i] + 1) && (difficulty == 1))) {
        tone(BUZZER_PIN, wrongSound, buzzerDuration);
        printWrongMessage();

        currLength = minLength;

        for (int j = 0; j < currLength; j++) {
          if (difficulty == 0) {
            easy[j] = random(0, 8);
          } else {
            hard[j] = random(0, 8);
          }
        }

        idleState = true;
        ledIndex = 0;
        sendToShiftRegister(0);
        printIdleMessage();
        break;
      }

      if (i == currLength - 1) {
        tone(BUZZER_PIN, rightSound, buzzerDuration);

        if (currLength > easyHighScore && difficulty == 0) {
          easyHighScore = currLength;
        } else if (currLength > hardHighScore && difficulty == 1) {
          hardHighScore = currLength;
        }

        printCorrectMessage();

        currLength++;
        if (difficulty == 0) {
          easy[currLength - 1] = random(0, 8);
        } else {
          for (int j = 0; j < currLength; j++) {
            hard[j] = random(0, 8);
          }
        }
        printLevelMessage();
        break;
      }
    }
    return;
  }

  // put your main code here, to run repeatedly:
  if (buttonPressed) {

    difficulty = 1 - difficulty;
    printIdleMessage();

    currLength = minLength;
    for (int i = 0; i < currLength; i++) {
      easy[i] = random(0, 8);
      hard[i] = random(0, 8);
    }

    buttonPressed = false;
  }

  char key = keypad.getKey();
  if (key != NO_KEY) {
    tone(BUZZER_PIN, keyPressSound, 100);
  }

  if (key == '0') {
    idleState = !idleState;
    ledIndex = 0;
    sendToShiftRegister(0);

    if (!idleState) {
      printLevelMessage();
    }
  } else if (key == '*') {
    powerOn = false;
    idleState = true;
    ledIndex = 0;
    sendToShiftRegister(0);
    lcd.clear();
    lcd.noBacklight();
  }

  if (idleState) {
    unsigned long currentTime = millis();
    if (currentTime - lastLedUpdateTime >= ledInterval) {
      uint16_t data = 1 << ledIndex;
      sendToShiftRegister(data);
      ledIndex = (ledIndex + 1) % 9;
      lastLedUpdateTime = currentTime;
    }
  }
}

// interrupt function for the button
ISR(PCINT1_vect) {
  unsigned long currentTime = millis();
  bool isPressed = !(PINC & (1 << BUTTON_BIT));
  if (isPressed) {
    if (buttonWasReleased && (currentTime - lastDebounceTime > debounceDelay)) {
      buttonPressed = true;
      lastDebounceTime = currentTime;
      buttonWasReleased = false;
    }
  } else {
    buttonWasReleased = true;
  }
}

// functions
void sendToShiftRegister(uint16_t data) {
  digitalWrite(SR_LATCH_PIN, LOW); // set latch low to start sending data
  shiftOut(SR_DATA_PIN, SR_CLK_PIN, MSBFIRST, highByte(data)); // send upper 8 bits
  shiftOut(SR_DATA_PIN, SR_CLK_PIN, MSBFIRST, lowByte(data)); // send lower 8 bits
  digitalWrite(SR_LATCH_PIN, HIGH); // set latch high to finish sending data
}

void printIdleMessage() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Press 0 to start");
  lcd.setCursor(0, 1);
  lcd.print(difficulty == 0 ? "Easy " : "Hard ");
  lcd.print("HS: ");
  lcd.print(difficulty == 0 ? easyHighScore : hardHighScore);
}

void printWrongMessage() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Wrong!");
  lcd.setCursor(0, 1);
  lcd.print("HS: ");
  lcd.print(difficulty == 0 ? easyHighScore : hardHighScore);
  lcd.print(" ");
  lcd.print("S: ");
  lcd.print(currLength - 1);
  delay(2000);
}

void printCorrectMessage() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Correct!");
  lcd.setCursor(0, 1);
  lcd.print("HS: ");
  lcd.print(difficulty == 0 ? easyHighScore : hardHighScore);
  lcd.print(" ");
  lcd.print("S: ");
  lcd.print(currLength);
  delay(2000);
}

void printLevelMessage() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Level starting:");
  lcd.setCursor(0, 1);
  lcd.print("3");
  delay(1000);
  lcd.setCursor(0, 1);
  lcd.print("2");
  delay(1000);
  lcd.setCursor(0, 1);
  lcd.print("1");
  delay(1000);
  lcd.setCursor(0, 1);
  lcd.print("Go!");
  delay(1000);
}

