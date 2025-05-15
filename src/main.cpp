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
const unsigned long ledInterval = 500; // 500ms per LED

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

// put function declarations here:
void sendToShiftRegister(uint16_t data);

void setup() {
  // put your setup code here, to run once:
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Hello, World!");

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
}

void loop() {
  // put your main code here, to run repeatedly:
  if (buttonPressed) {
    unsigned long currentTime = millis();
    if (currentTime - lastDebounceTime > debounceDelay) {
      tone(BUZZER_PIN, wrongSound, buzzerDuration);
      lastDebounceTime = currentTime;
    }
    buttonPressed = false;
  }

  char key = keypad.getKey();
  if (key) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("You pressed:");
    lcd.setCursor(0, 1);
    lcd.print(key);
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
