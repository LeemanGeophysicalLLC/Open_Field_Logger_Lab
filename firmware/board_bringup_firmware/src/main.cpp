/*
 * Open Field Logger Lab — Board Bring-Up Firmware
 *
 * Sequential self-test via serial at 115200 baud.
 * Press Enter before each test; type y/n for visual LED tests.
 * A PASS/FAIL summary table is printed at the end.
 *
 * Hardware (from schematic):
 *   Error LED    IO27  active HIGH
 *   Log LED      IO14  active HIGH
 *   Log Button   IO16  active LOW (INPUT_PULLUP)
 *   SD CS        IO5
 *   SD CD        IO32  LOW = card present
 *   ADS1115 ALRT IO26
 *   RTC MFP      IO33
 *   I2C SDA/SCL  IO21 / IO22
 *   SPI SCK/MISO/MOSI  IO18 / IO19 / IO23
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_ADS1X15.h>

// ---------------------------------------------------------------------------
// Pin assignments
// ---------------------------------------------------------------------------
static constexpr uint8_t PIN_ERROR_LED  = 27;
static constexpr uint8_t PIN_LOG_LED    = 14;
static constexpr uint8_t PIN_LOG_BUTTON = 16;
static constexpr uint8_t PIN_SD_CS      =  5;
static constexpr uint8_t PIN_SD_CD      = 32;

// I2C
static constexpr uint8_t PIN_I2C_SDA   = 21;
static constexpr uint8_t PIN_I2C_SCL   = 22;

// SPI (matches ESP32 VSPI defaults, stated explicitly for clarity)
static constexpr uint8_t PIN_SPI_SCK   = 18;
static constexpr uint8_t PIN_SPI_MISO  = 19;
static constexpr uint8_t PIN_SPI_MOSI  = 23;

// ---------------------------------------------------------------------------
// I2C addresses
// ---------------------------------------------------------------------------
static constexpr uint8_t ADS1115_ADDR  = 0x48;  // ADDR pin → GND
static constexpr uint8_t MCP7940_ADDR  = 0x6F;

// ---------------------------------------------------------------------------
// Test registry
// ---------------------------------------------------------------------------
static const char* TEST_NAMES[] = {
    "Error LED   (IO27)",
    "Log LED     (IO14)",
    "Log Button  (IO16)",
    "ADS1115 ADC (0x48)",
    "RTC MCP7940 (0x6F)",
    "SD Card     (IO5) "
};
static constexpr int NUM_TESTS = 6;
static bool g_results[NUM_TESTS];

// ---------------------------------------------------------------------------
// MCP7940 BCD helpers
// ---------------------------------------------------------------------------
static uint8_t toBCD(uint8_t v)   { return ((v / 10) << 4) | (v % 10); }
static uint8_t fromBCD(uint8_t b) { return ((b >> 4) & 0x0F) * 10 + (b & 0x0F); }

// ---------------------------------------------------------------------------
// Serial helpers
// ---------------------------------------------------------------------------
static void flushSerial() {
    delay(20);
    while (Serial.available()) Serial.read();
}

static void waitForEnter() {
    Serial.println(F("  [Press Enter to run this test]"));
    flushSerial();
    while (!Serial.available()) delay(10);
    flushSerial();
}

static char waitForYN() {
    flushSerial();
    char c = 0;
    while (c != 'y' && c != 'Y' && c != 'n' && c != 'N') {
        while (!Serial.available()) delay(10);
        c = Serial.read();
        flushSerial();
    }
    return c;
}

static void printHeader(int n) {
    Serial.println();
    Serial.print(F("--- TEST "));
    Serial.print(n + 1);
    Serial.print('/');
    Serial.print(NUM_TESTS);
    Serial.print(F(": "));
    Serial.println(TEST_NAMES[n]);
}

static void printResult(bool pass) {
    Serial.println(pass ? F("  >> PASS") : F("  >> FAIL"));
}

// ---------------------------------------------------------------------------
// Startup: print whatever the RTC currently holds
// ---------------------------------------------------------------------------
static void printRTCCurrentTime() {
    Wire.beginTransmission(MCP7940_ADDR);
    Wire.write(0x00);
    uint8_t err = Wire.endTransmission(false);
    if (err != 0 || Wire.requestFrom((uint8_t)MCP7940_ADDR, (uint8_t)7) < 7) {
        Serial.println(F("  RTC on startup: not reachable on I2C."));
        return;
    }

    uint8_t secRaw  = Wire.read();
    uint8_t minRaw  = Wire.read();
    uint8_t hourRaw = Wire.read();
    Wire.read();                        // weekday
    uint8_t dayRaw  = Wire.read();
    uint8_t monRaw  = Wire.read();
    uint8_t yearRaw = Wire.read();

    bool oscRunning = (secRaw & 0x80);  // ST bit set = oscillator enabled

    uint8_t sec  = fromBCD(secRaw  & 0x7F);
    uint8_t min  = fromBCD(minRaw);
    uint8_t hour = fromBCD(hourRaw & 0x3F);
    uint8_t day  = fromBCD(dayRaw);
    uint8_t mon  = fromBCD(monRaw  & 0x1F);
    uint8_t year = fromBCD(yearRaw);

    Serial.print(F("  RTC on startup: 20"));
    if (year < 10) Serial.print('0');
    Serial.print(year);
    Serial.print('-');
    if (mon  < 10) Serial.print('0');
    Serial.print(mon);
    Serial.print('-');
    if (day  < 10) Serial.print('0');
    Serial.print(day);
    Serial.print(' ');
    if (hour < 10) Serial.print('0');
    Serial.print(hour);
    Serial.print(':');
    if (min  < 10) Serial.print('0');
    Serial.print(min);
    Serial.print(':');
    if (sec  < 10) Serial.print('0');
    Serial.print(sec);
    Serial.println(oscRunning ? F("  (oscillator running)")
                              : F("  (oscillator stopped — fresh chip)"));
}

// ---------------------------------------------------------------------------
// TEST 1 — Error LED
// ---------------------------------------------------------------------------
static bool testErrorLED() {
    Serial.println(F("  Driving Error LED (IO27) HIGH."));
    digitalWrite(PIN_ERROR_LED, HIGH);
    Serial.println(F("  Is the red Error LED lit?  (y / n)"));
    char c = waitForYN();
    digitalWrite(PIN_ERROR_LED, LOW);
    return (c == 'y' || c == 'Y');
}

// ---------------------------------------------------------------------------
// TEST 2 — Log LED
// ---------------------------------------------------------------------------
static bool testLogLED() {
    Serial.println(F("  Driving Log LED (IO14) HIGH. Look for the BLUE led."));
    digitalWrite(PIN_LOG_LED, HIGH);
    Serial.println(F("  Is the blue Log LED lit?  (y / n)"));
    char c = waitForYN();
    digitalWrite(PIN_LOG_LED, LOW);
    return (c == 'y' || c == 'Y');
}

// ---------------------------------------------------------------------------
// TEST 3 — Log Button
// ---------------------------------------------------------------------------
static bool testLogButton() {
    Serial.println(F("  Press the Log Button (SW1) within 10 seconds..."));
    unsigned long deadline = millis() + 10000UL;

    while (millis() < deadline) {
        if (digitalRead(PIN_LOG_BUTTON) == LOW) {
            delay(20);  // debounce
            if (digitalRead(PIN_LOG_BUTTON) == LOW) {
                Serial.println(F("  Button press detected!"));
                while (digitalRead(PIN_LOG_BUTTON) == LOW) delay(10);
                return true;
            }
        }
    }
    Serial.println(F("  Timeout — no press detected."));
    return false;
}

// ---------------------------------------------------------------------------
// TEST 4 — ADS1115
// ---------------------------------------------------------------------------
static bool testADS1115() {
    Adafruit_ADS1115 ads;

    if (!ads.begin(ADS1115_ADDR, &Wire)) {
        Serial.println(F("  ERROR: ADS1115 not found at 0x48."));
        return false;
    }

    ads.setGain(GAIN_ONE);  // ±4.096 V full-scale, 0.125 mV/LSB
    Serial.println(F("  ADS1115 found. Single-ended readings (inputs may be floating):"));

    for (uint8_t ch = 0; ch < 4; ch++) {
        int16_t raw   = ads.readADC_SingleEnded(ch);
        float   volts = ads.computeVolts(raw);
        Serial.print(F("    AIN"));
        Serial.print(ch);
        Serial.print(F(": raw = "));
        Serial.print(raw);
        Serial.print(F(",  "));
        Serial.print(volts, 4);
        Serial.println(F(" V"));
    }
    return true;
}

// ---------------------------------------------------------------------------
// TEST 5 — RTC (MCP7940)
// ---------------------------------------------------------------------------
static bool testRTC() {
    // Parse compile-time stamp from preprocessor macros
    const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char mon[4] = { __DATE__[0], __DATE__[1], __DATE__[2], '\0' };
    uint8_t compileMonth = 0;
    for (int i = 0; i < 12; i++) {
        if (strncmp(mon, months + i * 3, 3) == 0) { compileMonth = i + 1; break; }
    }
    uint8_t compileDay  = (uint8_t)atoi(__DATE__ + 4);
    uint8_t compileYear = (uint8_t)(atoi(__DATE__ + 7) - 2000);
    uint8_t compileHour = (uint8_t)atoi(__TIME__ + 0);
    uint8_t compileMin  = (uint8_t)atoi(__TIME__ + 3);
    uint8_t compileSec  = (uint8_t)atoi(__TIME__ + 6);

    Serial.print(F("  Compile time: 20"));
    if (compileYear < 10) Serial.print('0');
    Serial.print(compileYear);
    Serial.print('-');
    if (compileMonth < 10) Serial.print('0');
    Serial.print(compileMonth);
    Serial.print('-');
    if (compileDay < 10) Serial.print('0');
    Serial.print(compileDay);
    Serial.print(' ');
    if (compileHour < 10) Serial.print('0');
    Serial.print(compileHour);
    Serial.print(':');
    if (compileMin < 10) Serial.print('0');
    Serial.print(compileMin);
    Serial.print(':');
    if (compileSec < 10) Serial.print('0');
    Serial.println(compileSec);

    // Write to MCP7940 starting at register 0x00
    Wire.beginTransmission(MCP7940_ADDR);
    Wire.write(0x00);
    Wire.write(toBCD(compileSec) | 0x80);   // 0x00  seconds  | ST=1 (start osc)
    Wire.write(toBCD(compileMin));           // 0x01  minutes
    Wire.write(toBCD(compileHour));          // 0x02  hours (24 h, bit6=0)
    Wire.write(0x08);                        // 0x03  VBATEN=1 (bit3); day-of-week unused
    Wire.write(toBCD(compileDay));           // 0x04  date
    Wire.write(toBCD(compileMonth));         // 0x05  month
    Wire.write(toBCD(compileYear));          // 0x06  year
    uint8_t txErr = Wire.endTransmission();

    if (txErr != 0) {
        Serial.print(F("  ERROR: I2C write failed (err "));
        Serial.print(txErr);
        Serial.println(')');
        return false;
    }
    Serial.println(F("  RTC set. Waiting 2 s to verify it ticks..."));
    delay(2000);

    // Read back
    Wire.beginTransmission(MCP7940_ADDR);
    Wire.write(0x00);
    Wire.endTransmission(false);
    if (Wire.requestFrom((uint8_t)MCP7940_ADDR, (uint8_t)7) < 7) {
        Serial.println(F("  ERROR: Could not read RTC registers."));
        return false;
    }

    uint8_t secNow  = fromBCD(Wire.read() & 0x7F);   // mask ST bit
    uint8_t minNow  = fromBCD(Wire.read());
    uint8_t hourNow = fromBCD(Wire.read() & 0x3F);   // mask 12/24 bit
    Wire.read();                                       // weekday (unused)
    uint8_t dayNow  = fromBCD(Wire.read());
    uint8_t monNow  = fromBCD(Wire.read() & 0x1F);   // mask LPYR bit
    uint8_t yearNow = fromBCD(Wire.read());

    Serial.print(F("  RTC reads:     20"));
    if (yearNow < 10) Serial.print('0');
    Serial.print(yearNow);
    Serial.print('-');
    if (monNow < 10) Serial.print('0');
    Serial.print(monNow);
    Serial.print('-');
    if (dayNow < 10) Serial.print('0');
    Serial.print(dayNow);
    Serial.print(' ');
    if (hourNow < 10) Serial.print('0');
    Serial.print(hourNow);
    Serial.print(':');
    if (minNow < 10) Serial.print('0');
    Serial.print(minNow);
    Serial.print(':');
    if (secNow < 10) Serial.print('0');
    Serial.println(secNow);

    // Seconds should have advanced by ~2 (allow 1-5 for setup/flash variance)
    int delta = (int)secNow - (int)compileSec;
    if (delta < 0) delta += 60;  // handle minute rollover

    if (delta >= 1 && delta <= 5) {
        Serial.print(F("  Seconds advanced by "));
        Serial.print(delta);
        Serial.println(F(". RTC is running."));
        return true;
    }

    Serial.print(F("  ERROR: seconds delta = "));
    Serial.print(delta);
    Serial.println(F(" (expected 1-5). RTC may not be oscillating."));
    return false;
}

// ---------------------------------------------------------------------------
// TEST 6 — SD Card
// ---------------------------------------------------------------------------
static bool testSD() {
    bool cardPresent = (digitalRead(PIN_SD_CD) == LOW);
    Serial.print(F("  Card detect (IO32): "));
    if (!cardPresent) {
        Serial.println(F("no card present. Insert a FAT32 card and re-run."));
        return false;
    }
    Serial.println(F("card present."));

    // Explicit SPI pin assignment guards against featheresp32 variant remapping
    SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SD_CS);

    if (!SD.begin(PIN_SD_CS, SPI)) {
        Serial.println(F("  ERROR: SD.begin() failed. Check card (FAT32) and wiring."));
        return false;
    }

    uint8_t cardType = SD.cardType();
    Serial.print(F("  Card type: "));
    switch (cardType) {
        case CARD_MMC:  Serial.println(F("MMC"));     break;
        case CARD_SD:   Serial.println(F("SDSC"));    break;
        case CARD_SDHC: Serial.println(F("SDHC"));   break;
        default:        Serial.println(F("Unknown")); break;
    }
    Serial.print(F("  Card size: "));
    Serial.print((uint32_t)(SD.cardSize() / (1024 * 1024)));
    Serial.println(F(" MB"));

    // Write / read-back / verify
    static const char* path     = "/bringup.txt";
    static const char* testData = "Open Field Logger bring-up OK\n";

    SD.remove(path);
    File f = SD.open(path, FILE_WRITE);
    if (!f) {
        Serial.println(F("  ERROR: Could not open file for writing."));
        return false;
    }
    f.print(testData);
    f.close();
    Serial.println(F("  Write OK."));

    f = SD.open(path, FILE_READ);
    if (!f) {
        Serial.println(F("  ERROR: Could not open file for reading."));
        return false;
    }
    char buf[64] = {};
    int  i = 0;
    while (f.available() && i < (int)(sizeof(buf) - 1)) buf[i++] = (char)f.read();
    f.close();

    bool match = (strcmp(buf, testData) == 0);
    if (match) {
        Serial.println(F("  Read-back OK — contents match."));
    } else {
        Serial.print(F("  ERROR: content mismatch. Got: "));
        Serial.println(buf);
    }

    SD.remove(path);
    return match;
}

// ---------------------------------------------------------------------------
// setup / loop
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(500);  // allow USB-serial to enumerate

    // Pin modes
    pinMode(PIN_ERROR_LED,  OUTPUT);
    pinMode(PIN_LOG_LED,    OUTPUT);
    pinMode(PIN_LOG_BUTTON, INPUT_PULLUP);
    pinMode(PIN_SD_CD,      INPUT_PULLUP);

    digitalWrite(PIN_ERROR_LED, LOW);
    digitalWrite(PIN_LOG_LED,   LOW);

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    // Banner
    Serial.println(F("\r\n"));
    Serial.println(F("============================================="));
    Serial.println(F("  Open Field Logger — Board Bring-Up Test"));
    Serial.print  (F("  Built: "));
    Serial.print  (F(__DATE__));
    Serial.print  (F(" "));
    Serial.println(F(__TIME__));
    Serial.println(F("============================================="));
    Serial.println(F("  Press Enter before each test."));
    Serial.println(F("  LED tests: type  y  (pass) or  n  (fail)."));
    Serial.println(F("============================================="));
    printRTCCurrentTime();

    // Test dispatch table
    typedef bool (*TestFn)();
    static const TestFn testFns[NUM_TESTS] = {
        testErrorLED,
        testLogLED,
        testLogButton,
        testADS1115,
        testRTC,
        testSD
    };

    for (int i = 0; i < NUM_TESTS; i++) {
        printHeader(i);
        waitForEnter();
        g_results[i] = testFns[i]();
        printResult(g_results[i]);
    }

    // Summary table
    Serial.println(F("\r\n============================================="));
    Serial.println(F("  BRING-UP SUMMARY"));
    Serial.println(F("============================================="));

    bool allPass = true;
    for (int i = 0; i < NUM_TESTS; i++) {
        Serial.print(F("  "));
        Serial.print(TEST_NAMES[i]);
        Serial.println(g_results[i] ? F("  PASS") : F("  FAIL"));
        if (!g_results[i]) allPass = false;
    }

    Serial.println(F("============================================="));
    Serial.println(allPass ? F("  ALL TESTS PASSED") : F("  FAILURES DETECTED"));
    Serial.println(F("============================================="));

    // Hold result on LEDs
    digitalWrite(allPass ? PIN_LOG_LED : PIN_ERROR_LED, HIGH);
}

void loop() { /* complete after setup() */ }
