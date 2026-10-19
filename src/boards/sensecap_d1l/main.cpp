// =============================================================================
// Seeed SenseCAP Indicator D1L — Minimal hardware bring-up
// =============================================================================
// Deliberately NOT the full LXMF app yet, and deliberately not even LVGL yet
// — this isolates the new, unproven pieces (radio/display/touch drivers)
// from the rest of the app's dependency graph (which runs deep into the
// Rust FFI protocol almost everywhere) so a hardware problem here can't be
// confused with an app-wiring problem. LVGL/UIManager/the full app get
// layered on in later, separate steps once this is confirmed working on
// real hardware. See TODO.md.

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

#include "config/BoardConfig.h"
#include "config/Config.h"
#include "hal/IOExpander.h"
#include "hal/Display.h"
#include "hal/TouchInput.h"
#include "radio/BoardRadio.h"

SPIClass loraSpi(FSPI);
IOExpander expander;
BoardRadio radio(&loraSpi, &expander);
Display display;
#if HAS_TOUCH
TouchInput touch;
#endif

bool radioOnline = false;
static uint32_t txCount = 0;

static void drawStatus(const char* line1, const char* line2 = nullptr) {
    auto& gfx = display.gfx();
    gfx.fillScreen(TFT_BLACK);
    gfx.setTextColor(TFT_WHITE, TFT_BLACK);
    gfx.setTextSize(2);
    gfx.setCursor(20, 40);
    gfx.print(line1);
    if (line2) {
        gfx.setCursor(20, 70);
        gfx.print(line2);
    }
}

// Manually triggered rather than periodic: TX is unexercised on this board
// so far (see TODO.md), and a human pressing the one physical button on the
// unit to fire it keeps airtime bounded during bring-up instead of flooding
// the band automatically.
static void sendTestPacket() {
    if (!radioOnline) { Serial.println("[TX] skipped, radio offline"); return; }
    char msg[48];
    const int len = snprintf(msg, sizeof(msg), "D1L TX test #%lu", (unsigned long)(++txCount));
    radio.beginPacket();
    radio.write(reinterpret_cast<const uint8_t*>(msg), len);
    const bool ok = radio.endPacket();  // synchronous: blocks until TX done or timeout
    Serial.printf("[TX] \"%s\" -> %s\n", msg, ok ? "sent" : "FAILED");
    drawStatus(radioOnline ? "Radio: ONLINE" : "Radio: FAILED", ok ? "TX sent" : "TX FAILED");
    radio.receive();
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(100);
    Serial.println();
    Serial.println("=================================");
    Serial.println("  SenseCAP Indicator D1L — bring-up");
    Serial.println("=================================");
    Serial.printf("[BOOT] Heap: %lu  PSRAM: %lu\n",
                  (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getPsramSize());

    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);
    if (!expander.begin(Wire, IO_EXPANDER_ADDR)) {
        Serial.println("[BOOT] FATAL: IO expander not found on I2C bus");
        while (true) delay(1000);
    }
    Serial.println("[BOOT] IO expander ready");

    // Display before radio, matching this board's confirmed-correct boot
    // order (same as Seeed's own reference firmware) — see Display.cpp for
    // the CS-assertion fix this depends on.
    display.setExpander(&expander);
    if (!display.begin()) {
        Serial.println("[BOOT] FATAL: display init failed");
        while (true) delay(1000);
    }
    display.setBrightness(128);
    drawStatus("Display OK", "Radio: starting...");

    // Radio's real hardware SPI shares physical pins 41/47/48 with the
    // display's one-time bit-banged init commands above — this ordering
    // (display init fully done first, radio claims the SPI peripheral on
    // these pins after) is required, not incidental. See BoardConfig.h.
    loraSpi.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    radioOnline = radio.begin(LORA_DEFAULT_FREQ);
    Serial.printf("[BOOT] Radio %s at %lu Hz\n",
                  radioOnline ? "online" : "not detected",
                  (unsigned long)LORA_DEFAULT_FREQ);
    if (radioOnline) radio.receive();

#if HAS_TOUCH
    const bool touchReady = touch.begin();
    Serial.printf("[BOOT] Touch %s\n", touchReady ? "ready" : "not detected");
#endif

    pinMode(PIN_USER_BTN, INPUT_PULLUP);

    drawStatus(radioOnline ? "Radio: ONLINE" : "Radio: FAILED",
               "Button: send test TX");
    Serial.println("[BOOT] Bring-up complete");
}

void loop() {
#if HAS_TOUCH
    touch.update();
    static bool wasTouched = false;
    if (touch.isTouched() && !wasTouched) {
        Serial.printf("[TOUCH] x=%d y=%d\n", touch.x(), touch.y());
        drawStatus(radioOnline ? "Radio: ONLINE" : "Radio: FAILED", "Touch OK!");
    }
    wasTouched = touch.isTouched();
#endif

    static bool btnWasPressed = false;
    static uint32_t lastBtnPress = 0;
    const bool btnPressed = digitalRead(PIN_USER_BTN) == LOW;
    if (btnPressed && !btnWasPressed && millis() - lastBtnPress > 300) {
        lastBtnPress = millis();
        Serial.println("[BTN] pressed, sending test TX packet");
        sendTestPacket();
    }
    btnWasPressed = btnPressed;

    if (radioOnline) {
        const int len = radio.parsePacket();
        if (len > 0) {
            Serial.printf("[RX] %d bytes, rssi=%d snr=%.1f\n", len, radio.packetRssi(), radio.packetSnr());
            radio.receive();
        }
    }

    static uint32_t lastPrint = 0;
    if (millis() - lastPrint >= 2000) {
        lastPrint = millis();
        Serial.printf("[STATUS] radio=%s rssi=%d\n",
                      radioOnline && radio.isRadioOnline() ? "online" : "offline",
                      radioOnline ? radio.currentRssi() : 0);
    }
    delay(5);
}
