#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
// #include <Keypad.h>

// initialize pins
#define BUTTON_PIN A3
#define BUTTON_BIT PC3 // A3 pin on Arduino Uno
#define BUZZER_PIN 10

volatile bool buttonPressed = false;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// initialize LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

const int wrongSound = 300;
const int rightSound = 1000;
const int buzzerDuration = 200;

// put function declarations here:

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
}

ISR(PCINT1_vect) {
  if (!(PINC & (1 << BUTTON_BIT))) { // Check if button is pressed
    buttonPressed = true;
  }
}
