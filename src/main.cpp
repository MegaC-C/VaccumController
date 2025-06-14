#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>

// Pin definitions
#define ENCODER_PIN_A     10
#define ENCODER_PIN_B     9
#define ENCODER_BTN_PIN   11
#define BTN_NEXT_SCREEN   8
#define BTN_PREV_SCREEN   12
#define VACUUM_SENSOR_PIN A6

// OLED setup
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Globals
const int min_target = 0;
const int max_target = 1200;
int target_mbar      = 0;
int calib_offset     = 0;
bool stepIsTen       = true;

enum Screen
{
    MAIN,
    SECOND
};
Screen currentScreen = MAIN;

// Encoder state
int lastA                 = HIGH;
unsigned long lastEncMove = 0;
#define ENCODER_DEBOUNCE_MS 5

// Encoder state for state machine
int8_t encoderPos    = 0;
int8_t encoderDelta  = 0;
uint8_t lastEncState = 0;

const int8_t enc_states[] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

// Timing
unsigned long lastUpdate           = 0;
const unsigned long updateInterval = 100; // ms

void setup()
{
    pinMode(ENCODER_PIN_A, INPUT_PULLUP);
    pinMode(ENCODER_PIN_B, INPUT_PULLUP);
    pinMode(ENCODER_BTN_PIN, INPUT_PULLUP);
    pinMode(BTN_NEXT_SCREEN, INPUT_PULLUP);
    pinMode(BTN_PREV_SCREEN, INPUT_PULLUP);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        for (;;)
            ;
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Vacuum Sensor Ready");
    display.display();
    delay(1000);

    lastA = digitalRead(ENCODER_PIN_A);

    // Initialize encoder state
    lastEncState = 0;
    if (digitalRead(ENCODER_PIN_A)) lastEncState |= 2;
    if (digitalRead(ENCODER_PIN_B)) lastEncState |= 1;
}

void handleEncoder()
{
    int a             = digitalRead(ENCODER_PIN_A);
    int b             = digitalRead(ENCODER_PIN_B);
    unsigned long now = millis();

    if (a != lastA && a == HIGH && (now - lastEncMove) > ENCODER_DEBOUNCE_MS)
    {
        int step = stepIsTen ? 10 : 1;
        if (b == LOW)
        {
            target_mbar += step; // Clockwise
        }
        else
        {
            target_mbar -= step; // Counterclockwise
        }
        if (target_mbar < min_target) target_mbar = min_target;
        if (target_mbar > max_target) target_mbar = max_target;
        lastEncMove = now;
    }
    lastA = a;
}

void handleEncoderButton()
{
    static int lastBtn = HIGH;
    int btn            = digitalRead(ENCODER_BTN_PIN);
    if (lastBtn == HIGH && btn == LOW)
    {
        stepIsTen = !stepIsTen;
    }
    lastBtn = btn;
}

void handleEncoderStateMachine()
{
    // Read both pins and combine into 2-bit value
    uint8_t enc = 0;
    if (digitalRead(ENCODER_PIN_A)) enc |= 2;
    if (digitalRead(ENCODER_PIN_B)) enc |= 1;

    // Combine last state and current state into 4-bit index
    uint8_t idx = (lastEncState << 2) | enc;
    encoderDelta += enc_states[idx];
    lastEncState = enc;

    // Only update value if a full detent (4 steps) is reached
    if (encoderDelta >= 4)
    {
        int step = stepIsTen ? 10 : 1;
        target_mbar += step;
        if (target_mbar > max_target) target_mbar = max_target;
        encoderDelta = 0;
    }
    else if (encoderDelta <= -4)
    {
        int step = stepIsTen ? 10 : 1;
        target_mbar -= step;
        if (target_mbar < min_target) target_mbar = min_target;
        encoderDelta = 0;
    }
}

void drawMainScreen(int vacuum_mbar, int voltage_mv, int adc_samples)
{
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.print("Vacuum: ");
    display.print(vacuum_mbar - calib_offset);
    display.println(" mbar");
    display.print("Sensor: ");
    display.print(voltage_mv);
    display.println(" mV");
    display.print("Target: ");
    display.print(target_mbar);
    display.println(" mbar");
    display.print("Step: ");
    display.print(stepIsTen ? 10 : 1);
    display.println(" mbar");
    display.print("ADC samples: ");
    display.println(adc_samples);
    display.display();
}

void loop()
{
    static long adcSum  = 0;
    static int adcCount = 0;

    // Take one ADC sample every loop
    adcSum += analogRead(VACUUM_SENSOR_PIN);
    adcCount++;

    handleEncoderStateMachine();
    handleEncoderButton();

    unsigned long now = millis();
    if (now - lastUpdate >= updateInterval)
    {
        lastUpdate = now;

        int adcValue    = (adcCount > 0) ? (adcSum / adcCount) : 0;
        int voltage_mv  = (adcValue * 5000L) / 1023;
        int vacuum_mbar = ((voltage_mv - 500) * 1000L) / (4500 - 500);

        drawMainScreen(vacuum_mbar, voltage_mv, adcCount);

        adcSum   = 0;
        adcCount = 0;
    }
}