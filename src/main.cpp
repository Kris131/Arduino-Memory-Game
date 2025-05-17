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

volatile bool buttonPressed = false;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;


volatile bool idleState = true;
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
byte rowPins[ROWS] = {9, 8, 7, 6};  // Adjust to your wiring
byte colPins[COLS] = {5, 4, 3, 2};  // Adjust to your wiring

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

const int wrongSound = 300;
const int rightSound = 1000;
const int buzzerDuration = 200;

// initialize the easy/hard difficulty arrays
const int minLength = 4;
int currLength = 4;
int highScore = 0;
uint16_t easy[32];
uint16_t hard[32];

// put function declarations here:
void sendToShiftRegister(uint16_t data);
void printIdleMessage();
void printWrongMessage();
void printCorrectMessage();

void setup() {
  // put your setup code here, to run once:
  lcd.init();
  lcd.backlight();
  printIdleMessage();

  pinMode(BUZZER_PIN, OUTPUT);

  DDRC &= ~(1 << BUTTON_BIT); // Set BUTTON_BIT as input
  PORTC |= (1 << BUTTON_BIT); // Enable pull-up resistor

  PCICR |= (1 << PCIE1); // Enable pin change interrupt for PCINT0
  PCMSK1 |= (1 << PCINT11); // Enable interrupt for BUTTON_BIT
  sei(); // Enable global interrupts

  // Initialize the shift register pins
  pinMode(SR_DATA_PIN, OUTPUT);
  pinMode(SR_LATCH_PIN, OUTPUT);
  pinMode(SR_CLK_PIN, OUTPUT);

  sendToShiftRegister(0);

  for (int i = 0; i < currLength; i++) {
    easy[i] = random(0, 8);
    hard[i] = random(0, 8);
  }
}

void loop() {
  if (!idleState) {
    for (int i = 0; i < currLength; i++) {
      sendToShiftRegister(0);
      delay(300);
      sendToShiftRegister(1 << easy[i]);
      delay(300);
    }
    sendToShiftRegister(0);

    for (int i = 0; i < currLength; i++) {
      char keyPressed = keypad.waitForKey();
      uint16_t keyInt = keyPressed - '0';

      if (keyInt != easy[i] + 1) {
        tone(BUZZER_PIN, wrongSound, buzzerDuration);
        printWrongMessage();

        currLength = minLength;

        for (int j = 0; j < currLength; j++) {
          easy[j] = random(0, 8);
        }

        idleState = true;
        ledIndex = 0;
        sendToShiftRegister(0);
        break;
      }

      if (i == currLength - 1) {
        tone(BUZZER_PIN, rightSound, buzzerDuration);
        printCorrectMessage();

        if (currLength > highScore) {
          highScore = currLength;
        }

        currLength++;
        easy[currLength - 1] = random(0, 8);

        break;
      }
    }

    return;
  }

  // put your main code here, to run repeatedly:
  if (buttonPressed) {
    unsigned long currentTime = millis();
    if (currentTime - lastDebounceTime > debounceDelay) {
      difficulty = 1 - difficulty;
      printIdleMessage();
      lcd.setCursor(0, 1);
      lcd.print("Difficulty: ");
      lcd.print(difficulty ? "Easy" : "Hard");
      // tone(BUZZER_PIN, wrongSound, buzzerDuration);
      lastDebounceTime = currentTime;

      currLength = minLength;
      for (int i = 0; i < currLength; i++) {
        easy[i] = random(0, 8);
        hard[i] = random(0, 8);
      }
    }
    buttonPressed = false;
  }

  char key = keypad.getKey();
  // if (key) {
  //   lcd.clear();
  //   lcd.setCursor(0, 0);
  //   lcd.print("You pressed:");
  //   lcd.setCursor(0, 1);
  //   lcd.print(key);
  // }

  if (key == '0') {
    idleState = !idleState;
    ledIndex = 0;
    sendToShiftRegister(0);
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

ISR(PCINT1_vect) {
  if (!(PINC & (1 << BUTTON_BIT))) { // Check if button is pressed
    buttonPressed = true;
  }
}

// functions
void sendToShiftRegister(uint16_t data) {
  digitalWrite(SR_LATCH_PIN, LOW); // Set latch low to start sending data
  shiftOut(SR_DATA_PIN, SR_CLK_PIN, MSBFIRST, highByte(data)); // Send high byte
  shiftOut(SR_DATA_PIN, SR_CLK_PIN, MSBFIRST, lowByte(data)); // Send low byte
  digitalWrite(SR_LATCH_PIN, HIGH); // Set latch high to finish sending data
}

void printIdleMessage() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Press 0 to start");
  lcd.setCursor(0, 1);
  lcd.print("HS: ");
  lcd.print(highScore);
}

void printWrongMessage() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Wrong! Press 0 to play again");
  lcd.setCursor(0, 1);
  lcd.print("HS: ");
  lcd.print(highScore);
  lcd.print(" ");
  lcd.print("S: ");
  lcd.print(currLength - 1);
}

void printCorrectMessage() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Correct! Next lvl");
  lcd.setCursor(0, 1);
  lcd.print("HS: ");
  lcd.print(highScore);
  lcd.print(" ");
  lcd.print("S: ");
  lcd.print(currLength);
}

/*
TODO: make the user watch the sequence
print on lcd 'Start'
the user will input n numbers
check the input with the sequence
if an input is wrong, play a sound and a message on lcd with 'press 0 to start again'
if the input is correct, play a sound and print 'correct' and add 1 to the sequence
if easy add one led, if hard redo the sequence with new numbers with one more led
*/
