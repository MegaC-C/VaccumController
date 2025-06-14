#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>

// OLED setup
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Vacuum sensor
#define VACUUM_SENSOR_PIN A6

// Rotary encoder pins
#define ENCODER_BTN_PIN 11
#define ENCODER_PIN_A   10 // Clockwise
#define ENCODER_PIN_B   9  // Counterclockwise

// Screen navigation buttons
#define BTN_NEXT_SCREEN 8
#define BTN_PREV_SCREEN 12

long target_mbar      = 0; // Target pressure in mbar
const long min_target = 0;
const long max_target = 1200;

int lastEncoderA    = LOW;
int lastButtonState = HIGH;
bool stepIsTen      = true; // true = 10 mbar, false = 1 mbar

int lastBtnNextState = HIGH;
int lastBtnPrevState = HIGH;

const unsigned long updateInterval = 500; // ms
unsigned long lastUpdate           = 0;

enum Screen
{
    MAIN,
    SECOND
};
Screen currentScreen = MAIN;

void setup()
{
    Serial.begin(9600);

    pinMode(ENCODER_PIN_A, INPUT_PULLUP);
    pinMode(ENCODER_PIN_B, INPUT_PULLUP);
    pinMode(ENCODER_BTN_PIN, INPUT_PULLUP);
    pinMode(BTN_NEXT_SCREEN, INPUT_PULLUP);
    pinMode(BTN_PREV_SCREEN, INPUT_PULLUP);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;)
            ;
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Vacuum Sensor Test");
    display.display();
    delay(1000);

    lastEncoderA     = digitalRead(ENCODER_PIN_A);
    lastButtonState  = digitalRead(ENCODER_BTN_PIN);
    lastBtnNextState = digitalRead(BTN_NEXT_SCREEN);
    lastBtnPrevState = digitalRead(BTN_PREV_SCREEN);
}

void readEncoder()
{
    int encoderA = digitalRead(ENCODER_PIN_A);
    int encoderB = digitalRead(ENCODER_PIN_B);

    long step = stepIsTen ? 10 : 1;

    if (encoderA != lastEncoderA)
    {
        if (encoderA == HIGH)
        {
            if (encoderB == LOW)
            {
                target_mbar += step; // Clockwise
            }
            else
            {
                target_mbar -= step; // Counterclockwise
            }
            if (target_mbar < min_target) target_mbar = min_target;
            if (target_mbar > max_target) target_mbar = max_target;
        }
        lastEncoderA = encoderA;
    }
}

void readEncoderButton()
{
    int buttonState = digitalRead(ENCODER_BTN_PIN);
    if (lastButtonState == HIGH && buttonState == LOW)
    {
        stepIsTen = !stepIsTen;
    }
    lastButtonState = buttonState;
}

void readScreenButtons()
{
    int btnNextState = digitalRead(BTN_NEXT_SCREEN);
    int btnPrevState = digitalRead(BTN_PREV_SCREEN);

    // D8 pressed: go to second screen
    if (lastBtnNextState == HIGH && btnNextState == LOW)
    {
        currentScreen = SECOND;
    }
    // D12 pressed: go back to main screen
    if (lastBtnPrevState == HIGH && btnPrevState == LOW)
    {
        currentScreen = MAIN;
    }
    lastBtnNextState = btnNextState;
    lastBtnPrevState = btnPrevState;
}

void drawMainScreen(float vacuum_mbar, float voltage)
{
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.print("Vacuum: ");
    display.print(vacuum_mbar, 1);
    display.println(" mbar");
    display.print("Voltage: ");
    display.print(voltage, 3);
    display.println(" V");
    display.print("Target: ");
    display.print(target_mbar);
    display.println(" mbar");
    display.print("Step: ");
    display.print(stepIsTen ? 10 : 1);
    display.println(" mbar");
    display.display();
}

void drawSecondScreen()
{
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(2);
    display.println("Second");
    display.setTextSize(1);
    display.println("Screen");
    display.println("Placeholder");
    display.display();
}

void loop()
{
    readEncoder();
    readEncoderButton();
    readScreenButtons();

    unsigned long now = millis();
    if (now - lastUpdate >= updateInterval)
    {
        lastUpdate = now;

        if (currentScreen == MAIN)
        {
            // Take 128 samples and average
            long adcSum = 0;
            for (int i = 0; i < 128; i++)
            {
                adcSum += analogRead(VACUUM_SENSOR_PIN);
            }
            float adcValue = adcSum / 128.0;
            float voltage  = adcValue * (5.0 / 1023.0);

            float vacuum_bar  = (voltage - 0.5) * (1.0 / (4.5 - 0.5));
            float vacuum_mbar = vacuum_bar * 1000.0;

            drawMainScreen(vacuum_mbar, voltage);

            // Serial output
            Serial.print("Vacuum: ");
            Serial.print(vacuum_mbar, 1);
            Serial.print(" mbar, Voltage: ");
            Serial.print(voltage, 3);
            Serial.print(" V, Target: ");
            Serial.print(target_mbar);
            Serial.print(" mbar, Step: ");
            Serial.println(stepIsTen ? 10 : 1);
        }
        else if (currentScreen == SECOND)
        {
            drawSecondScreen();
        }
    }
}