#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "config.h"

static Adafruit_SH1106G gDisplay(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
static bool gDisplayReady = false;
static uint8_t gUsedAddr = 0;

static const char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static void printFlashSettings() {
    Serial.println("--- Runtime / Flash Settings ---");
    Serial.print("Chip model: ");
    Serial.println(ESP.getChipModel());
    Serial.print("CPU freq (MHz): ");
    Serial.println(ESP.getCpuFreqMHz());
    Serial.print("Flash chip size (bytes): ");
    Serial.println(ESP.getFlashChipSize());
    Serial.print("Flash chip speed (Hz): ");
    Serial.println(ESP.getFlashChipSpeed());
    Serial.print("Sketch size (bytes): ");
    Serial.println(ESP.getSketchSize());
    Serial.print("Free sketch space (bytes): ");
    Serial.println(ESP.getFreeSketchSpace());
    Serial.print("SDK version: ");
    Serial.println(ESP.getSdkVersion());
    Serial.println("Expected ESP32 web flash layout:");
    Serial.println("  bootloader.bin @ 0x1000");
    Serial.println("  boot_app0.bin @ 0xE000");
    Serial.println("  partitions.bin @ 0x8000");
    Serial.println("  firmware.bin @ 0x10000");
}

static void scanI2C() {
    Serial.println("--- I2C Scan ---");
    bool foundAny = false;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        uint8_t err = Wire.endTransmission();
        if (err == 0) {
            Serial.print("Found I2C device at 0x");
            if (addr < 16) Serial.print('0');
            Serial.println(addr, HEX);
            foundAny = true;
        }
    }
    if (!foundAny) {
        Serial.println("No I2C devices found");
    }
}

static void tryInitDisplay() {
    const uint8_t primaryAddr = (uint8_t)OLED_I2C_ADDRESS;
    const uint8_t fallbackAddr = (primaryAddr == 0x3C) ? 0x3D : 0x3C;

    Serial.print("Trying SH1106 init at 0x");
    if (primaryAddr < 16) Serial.print('0');
    Serial.println(primaryAddr, HEX);

    if (gDisplay.begin(primaryAddr, true)) {
        gDisplayReady = true;
        gUsedAddr = primaryAddr;
        Serial.println("Display init success on primary address");
        return;
    }

    Serial.print("Primary failed, trying fallback 0x");
    if (fallbackAddr < 16) Serial.print('0');
    Serial.println(fallbackAddr, HEX);

    if (gDisplay.begin(fallbackAddr, true)) {
        gDisplayReady = true;
        gUsedAddr = fallbackAddr;
        Serial.println("Display init success on fallback address");
    } else {
        Serial.println("Display init failed on both addresses");
    }
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(300);

    Serial.println("\n=== SH1106 Display Alphabet Test ===");
    Serial.print("I2C pins: SDA=");
    Serial.print(OLED_SDA_PIN);
    Serial.print(", SCL=");
    Serial.println(OLED_SCL_PIN);

    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
    printFlashSettings();
    scanI2C();
    tryInitDisplay();

    if (!gDisplayReady) {
        Serial.println("Display init failed on 0x3C and 0x3D");
        return;
    }

    Serial.print("Display init OK at address 0x");
    Serial.println(gUsedAddr, HEX);

    gDisplay.clearDisplay();
    gDisplay.setTextColor(SH110X_WHITE);
    gDisplay.setTextSize(1);
    gDisplay.setCursor(0, 0);
    gDisplay.println("SH1106 Alphabet Test");
    gDisplay.setCursor(0, 10);
    gDisplay.print("I2C: 0x");
    gDisplay.println(gUsedAddr, HEX);
    gDisplay.display();
}

void loop() {
    static size_t index = 0;
    static unsigned long lastLogMs = 0;

    if (!gDisplayReady) {
        delay(1000);
        return;
    }

    const char ch = kAlphabet[index];

    gDisplay.clearDisplay();
    gDisplay.setTextColor(SH110X_WHITE);

    gDisplay.setTextSize(1);
    gDisplay.setCursor(0, 0);
    gDisplay.println("Alphabet Sequence");
    gDisplay.setCursor(0, 10);
    gDisplay.print("Index: ");
    gDisplay.println(index);

    gDisplay.setTextSize(3);
    gDisplay.setCursor(52, 30);
    gDisplay.print(ch);

    gDisplay.setTextSize(1);
    gDisplay.setCursor(0, 56);
    gDisplay.println("A..Z then a..z");
    gDisplay.display();

    if (millis() - lastLogMs > 1000) {
        Serial.print("Display frame: index=");
        Serial.print(index);
        Serial.print(", char=");
        Serial.println(ch);
        lastLogMs = millis();
    }

    index = (index + 1) % (sizeof(kAlphabet) - 1);
    delay(700);
}
