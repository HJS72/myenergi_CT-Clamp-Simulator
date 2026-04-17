#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "config.h"

static U8G2_SH1106_128X64_NONAME_F_HW_I2C gU8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL_PIN, OLED_SDA_PIN);
static const char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

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

static void printRuntimeInfo() {
    Serial.println("=== SH1106 U8g2 Display Test ===");
    Serial.print("SDA pin: ");
    Serial.println(OLED_SDA_PIN);
    Serial.print("SCL pin: ");
    Serial.println(OLED_SCL_PIN);
    Serial.print("Configured OLED addr: 0x");
    Serial.println(OLED_I2C_ADDRESS, HEX);
    Serial.print("Chip model: ");
    Serial.println(ESP.getChipModel());
    Serial.print("Flash size (bytes): ");
    Serial.println(ESP.getFlashChipSize());
    Serial.print("Flash speed (Hz): ");
    Serial.println(ESP.getFlashChipSpeed());
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(300);

    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
    printRuntimeInfo();
    scanI2C();

    gU8g2.begin();
    gU8g2.setContrast(255);
    gU8g2.setPowerSave(0);

    gU8g2.clearBuffer();
    gU8g2.setFont(u8g2_font_6x12_tf);
    gU8g2.drawStr(0, 10, "SH1106 U8g2 Test");
    gU8g2.drawStr(0, 24, "If this is visible,");
    gU8g2.drawStr(0, 36, "OLED + wiring works");
    gU8g2.sendBuffer();

    Serial.println("U8g2 initialized and first frame sent");
}

void loop() {
    static size_t index = 0;
    static bool invert = false;

    char ch[2] = {kAlphabet[index], '\0'};

    gU8g2.clearBuffer();
    gU8g2.setDrawColor(1);
    gU8g2.setFont(u8g2_font_6x12_tf);
    gU8g2.drawStr(0, 10, "Alphabet (U8g2)");

    gU8g2.setFont(u8g2_font_logisoso34_tf);
    gU8g2.drawStr(46, 50, ch);

    gU8g2.setFont(u8g2_font_6x12_tf);
    gU8g2.drawStr(0, 62, invert ? "Invert: ON" : "Invert: OFF");
    gU8g2.sendBuffer();

    gU8g2.setFlipMode(invert ? 1 : 0);
    invert = !invert;

    Serial.print("Frame index=");
    Serial.print(index);
    Serial.print(", char=");
    Serial.println(ch[0]);

    index = (index + 1) % (sizeof(kAlphabet) - 1);
    delay(800);
}
