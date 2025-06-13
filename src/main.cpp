#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define VACUUM_SENSOR_PIN A6

const unsigned long updateInterval = 500; // ms
unsigned long lastUpdate           = 0;

void setup()
{
    Serial.begin(9600);

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
}

void loop()
{
    unsigned long now = millis();
    if (now - lastUpdate >= updateInterval)
    {
        lastUpdate = now;

        // Take 16 samples and average
        long adcSum = 0;
        for (int i = 0; i < 128; i++)
        {
            adcSum += analogRead(VACUUM_SENSOR_PIN);
        }
        float adcValue = adcSum / 128.0;
        float voltage  = adcValue * (5.0 / 1023.0);

        // Sensor: 0.5V = 0 bar, 4.5V = 1 bar (linear)
        float vacuum_bar  = (voltage - 0.5) * (1.0 / (4.5 - 0.5));
        float vacuum_mbar = vacuum_bar * 1000.0;

        // Display on OLED
        display.clearDisplay();
        display.setCursor(0, 0);
        display.setTextSize(1);
        display.print("Vacuum: ");
        display.print(vacuum_mbar, 1);
        display.println(" mbar");
        display.print("Voltage: ");
        display.print(voltage, 3);
        display.println(" V");
        display.display();

        // Send to Serial
        Serial.print("Vacuum: ");
        Serial.print(vacuum_mbar, 1);
        Serial.print(" mbar, Voltage: ");
        Serial.print(voltage, 3);
        Serial.println(" V");
    }

    // ...future code for other modules...
}