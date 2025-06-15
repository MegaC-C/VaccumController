#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <EEPROM.h>
#include <PID_v1.h>
#include <Wire.h>
#include <avr/interrupt.h>

#define ENCODER_PIN_A     10
#define ENCODER_PIN_B     9
#define ENCODER_BTN_PIN   11
#define BTN_CALIB_SCREEN  8
#define BTN_PUMP_ONOFF    12
#define PUMP_PWM_PIN      3
#define VACUUM_SENSOR_PIN A6

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const int updateInterval_ms = 200;
const int min_mbar          = 0;
const int max_mbar          = 1200;
int target_mbar             = 1000;
int calibOffset_mbar        = 0;
bool stepIsTen              = true;
bool inCalibration          = false;
bool pumpOn                 = false;

double pidInput, pidOutput, pidSetpoint;
double Kp = 2.0, Ki = 0.5, Kd = 1.0;
PID vacuumPID(&pidInput, &pidOutput, &pidSetpoint, Kp, Ki, Kd, AUTOMATIC);

// Hybrid interrupt encoder
volatile bool encoderEvent        = false;
volatile int encoderDirection     = 0;
volatile unsigned long lastEncISR = 0;

ISR(PCINT0_vect)
{
    static int lastA  = HIGH;
    int a             = (PINB & (1 << PB2)) ? HIGH : LOW; // D10 = PB2
    int b             = (PINB & (1 << PB1)) ? HIGH : LOW; // D9  = PB1
    unsigned long now = millis();

    if (a != lastA && a == HIGH && (now - lastEncISR) > 5)
    { // Rising edge, debounce
        if (b == LOW)
        {
            encoderDirection = 1; // Clockwise
        }
        else
        {
            encoderDirection = -1; // Counterclockwise
        }
        encoderEvent = true;
        lastEncISR   = now;
    }
    lastA = a;
}

void setup()
{
    pinMode(ENCODER_PIN_A, INPUT_PULLUP);
    pinMode(ENCODER_PIN_B, INPUT_PULLUP);
    pinMode(ENCODER_BTN_PIN, INPUT_PULLUP);
    pinMode(BTN_CALIB_SCREEN, INPUT_PULLUP);
    pinMode(BTN_PUMP_ONOFF, INPUT_PULLUP);
    pinMode(PUMP_PWM_PIN, OUTPUT);

    vacuumPID.SetOutputLimits(0, 255);

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

    // Set up PCINT for D10 (PB2)
    PCICR |= (1 << PCIE0);   // Enable PCINT0 (D8-D13)
    PCMSK0 |= (1 << PCINT2); // Enable PCINT2 (D10)

    sei(); // Enable global interrupts

    calibOffset_mbar = EEPROM.read(0) | (EEPROM.read(1) << 8);
}

void handleScreenButtons()
{
    static int lastBtnNext = HIGH;
    int btnNext            = digitalRead(BTN_CALIB_SCREEN);

    // Toggle calibration mode with D8
    if (lastBtnNext == HIGH && btnNext == LOW)
    {
        inCalibration = !inCalibration;
        if (!inCalibration)
        {
            EEPROM.write(0, calibOffset_mbar & 0xFF);
            EEPROM.write(1, (calibOffset_mbar >> 8) & 0xFF);
        }
    }
    lastBtnNext = btnNext;
}

void handleEncoderHybridInterrupt()
{
    if (encoderEvent)
    {
        int step = stepIsTen ? 10 : 1;
        if (inCalibration)
        {
            if (encoderDirection == 1)
            {
                calibOffset_mbar += step;
            }
            else if (encoderDirection == -1)
            {
                calibOffset_mbar -= step;
            }
        }
        else
        {
            if (encoderDirection == 1)
            {
                target_mbar += step;
            }
            else if (encoderDirection == -1)
            {
                target_mbar -= step;
            }
            if (target_mbar < min_mbar) target_mbar = min_mbar;
            if (target_mbar > max_mbar) target_mbar = max_mbar;
        }
        encoderEvent = false;
    }
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

void handlePumpButton()
{
    static int lastBtn = HIGH;
    int btn            = digitalRead(BTN_PUMP_ONOFF);
    if (lastBtn == HIGH && btn == LOW)
    {
        pumpOn = !pumpOn;
    }
    lastBtn = btn;
}

void drawMainScreen(int vacuum_mbar, double pumpPWM_percent)
{
    display.clearDisplay();
    display.setCursor(0, 0);

    display.setTextSize(2);
    display.print(" ");
    display.print(target_mbar);
    display.setTextSize(1);
    display.setCursor(display.getCursorX() - 3, display.getCursorY() + 7);
    display.print(" mbar");
    display.println();
    display.setTextSize(2);

    // Calculate how many spaces to print to highlight the current step
    int numDigits = String(target_mbar).length();
    int spaces    = stepIsTen ? (numDigits - 1) : (numDigits);
    if (spaces < 0) spaces = 0;
    for (int i = 0; i < spaces; i++)
        display.print(" ");
    display.println("^");

    display.print(" ");
    display.setTextSize(4);
    display.print(vacuum_mbar);
    display.setTextSize(1);
    display.setCursor(display.getCursorX() - 6, display.getCursorY() + 21);
    display.println(" mbar");

    display.setCursor(104, 0);
    display.print(pumpOn ? String(pumpPWM_percent) + "%" : "OFF");

    display.display();
}

void drawCalibrationScreen(int vacuum_mbar, int voltage_mv, int adc_samples)
{
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.println("Calibration");
    display.print("Offset: ");
    display.print(calibOffset_mbar);
    display.println(" mbar");

    // Calculate how many spaces to print to highlight the current step
    int numDigits = String(calibOffset_mbar).length();
    int spaces    = stepIsTen ? (numDigits - 1) : (numDigits);
    if (spaces < 0) spaces = 0;
    display.print("       ");
    for (int i = 0; i < spaces; i++)
        display.print(" ");
    display.println("^");

    display.print("Vacuum: ");
    display.print(vacuum_mbar);
    display.println(" mbar");
    display.print("Sensor ADC: ");
    display.print(voltage_mv);
    display.println(" mV");
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

    unsigned long now                  = millis();
    static unsigned long lastUpdate_ms = 0;
    static int measured_mbar           = 1000;
    static int voltage_mv              = 0;
    static int pumpPWM                 = 0;
    static int pumpPWM_percent         = 0;

    handleScreenButtons();
    handleEncoderHybridInterrupt();
    handleEncoderButton();
    handlePumpButton();

    if (now - lastUpdate_ms >= updateInterval_ms)
    {
        lastUpdate_ms = now;

        int adcValue  = (adcCount > 0) ? (adcSum / adcCount) : 0;
        voltage_mv    = (adcValue * 5000L) / 1023;
        measured_mbar = (((voltage_mv - 500) * 1000L) / (4500 - 500)) + calibOffset_mbar;

        if (pumpOn)
        {
            pidInput    = (double)measured_mbar;
            pidSetpoint = (double)target_mbar;
            vacuumPID.Compute();
            pumpPWM         = (int)pidOutput;
            pumpPWM_percent = (pumpPWM * 100) / 255;
            analogWrite(PUMP_PWM_PIN, pumpPWM);
        }
        else
        {
            analogWrite(PUMP_PWM_PIN, 0);
        }

        if (inCalibration)
        {
            drawCalibrationScreen(measured_mbar, voltage_mv, adcCount);
        }
        else
        {
            drawMainScreen(measured_mbar, pidOutput);
        }

        adcSum   = 0;
        adcCount = 0;
    }
}