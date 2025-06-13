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

void drawGauge(float vacuum_mbar)
{
    int cx       = 64; // Center X
    int cy       = 56; // Center Y (bottom of display)
    int r        = 30; // Radius
    int min_mbar = 0;
    int max_mbar = 1200; // Adjust as needed

    // Draw semicircle arc (180° to 360°)
    for (int angle = 180; angle <= 360; angle += 4)
    {
        float rad = angle * PI / 180.0;
        int x1    = cx + r * cos(rad);
        int y1    = cy + r * sin(rad);
        int x2    = cx + (r - 3) * cos(rad);
        int y2    = cy + (r - 3) * sin(rad);
        display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
    }

    // Map vacuum to angle (right=180°, left=360°)
    float angle = 180 + (vacuum_mbar - min_mbar) * 180.0 / (max_mbar - min_mbar);
    if (angle < 180) angle = 180;
    if (angle > 360) angle = 360;

    // Needle endpoint
    float rad = angle * PI / 180.0;
    int nx    = cx + (r - 6) * cos(rad);
    int ny    = cy + (r - 6) * sin(rad);

    // Draw needle
    display.drawLine(cx, cy, nx, ny, SSD1306_WHITE);
}

void loop()
{
    unsigned long now = millis();
    if (now - lastUpdate >= updateInterval)
    {
        lastUpdate = now;

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

        display.clearDisplay();

        // Draw gauge
        drawGauge(vacuum_mbar);

        // Display values
        display.setCursor(0, 0);
        display.setTextSize(1);
        display.print("Vacuum: ");
        display.print(vacuum_mbar, 1);
        display.println(" mbar");
        display.print("Voltage: ");
        display.print(voltage, 3);
        display.println(" V");
        display.display();

        // Serial output
        Serial.print("Vacuum: ");
        Serial.print(vacuum_mbar, 1);
        Serial.print(" mbar, Voltage: ");
        Serial.print(voltage, 3);
        Serial.println(" V");
    }
    // ...future code...
}