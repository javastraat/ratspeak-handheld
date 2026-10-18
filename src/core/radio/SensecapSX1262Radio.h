#pragma once
#include "config/BoardConfig.h"
#if defined(RSSENSECAPD1L)
#include <Arduino.h>
#include <SPI.h>
#include <hal/Arduino/ArduinoHal.h>
#include <modules/SX126x/SX1262.h>
#include "hal/IOExpander.h"

// RadioLib Hal for the SenseCAP Indicator D1L: CS/RESET/BUSY/IRQ are not
// real ESP32-S3 GPIOs, they're routed through a TCA9535/PCA9535 I2C GPIO
// expander (see IOExpander.h). Virtual pin numbers are encoded as
// EXPANDER_FLAG | <bit> so the base ArduinoHal's real-GPIO path is used
// for everything else (SPI pins, the shared expander IRQ line) unchanged.
class SensecapRadioHal final : public ArduinoHal {
public:
    static constexpr uint32_t EXPANDER_FLAG = 0x8000;
    static uint32_t expanderPin(uint8_t bit) { return EXPANDER_FLAG | bit; }

    SensecapRadioHal(SPIClass& spi, IOExpander& expander)
        : ArduinoHal(spi, SPISettings(SPI_FREQUENCY, MSBFIRST, SPI_MODE0)), _expander(expander) {}

    void pinMode(uint32_t pin, uint32_t mode) override {
        if (isExpanderPin(pin)) { _expander.pinMode(expanderBit(pin), mode); return; }
        ArduinoHal::pinMode(pin, mode);
    }
    void digitalWrite(uint32_t pin, uint32_t value) override {
        if (isExpanderPin(pin)) { _expander.digitalWrite(expanderBit(pin), value); return; }
        ArduinoHal::digitalWrite(pin, value);
    }
    uint32_t digitalRead(uint32_t pin) override {
        if (isExpanderPin(pin)) return _expander.digitalRead(expanderBit(pin));
        return ArduinoHal::digitalRead(pin);
    }
    // The expander's single IRQ line is shared across all of its inputs,
    // not just the radio's DIO1 — redirect to the one real physical pin.
    // The ISR fires on any watched input changing; if the trigger wasn't
    // really DIO1, RadioLib's own follow-up SPI status read harmlessly
    // finds nothing; one redundant check, not a correctness problem.
    void attachInterrupt(uint32_t interruptNum, void (*cb)(void), uint32_t mode) override {
        ArduinoHal::attachInterrupt(isExpanderPin(interruptNum) ? IO_EXPANDER_IRQ : interruptNum, cb, mode);
    }
    void detachInterrupt(uint32_t interruptNum) override {
        ArduinoHal::detachInterrupt(isExpanderPin(interruptNum) ? IO_EXPANDER_IRQ : interruptNum);
    }
    uint32_t pinToInterrupt(uint32_t pin) override {
        return isExpanderPin(pin) ? IO_EXPANDER_IRQ : ArduinoHal::pinToInterrupt(pin);
    }

private:
    static bool isExpanderPin(uint32_t pin) { return (pin & EXPANDER_FLAG) != 0; }
    static uint8_t expanderBit(uint32_t pin) { return uint8_t(pin & 0xFF); }
    IOExpander& _expander;
};

// Raw physical frames only. LoRaInterface remains the sole RNode-framing,
// split-packet, queue, airtime policy and receipt owner — same contract as
// every other radio driver in this codebase (see LR1121Radio.h).
class SensecapSX1262Radio {
public:
    SensecapSX1262Radio(SPIClass* spi, IOExpander* expander);
    bool begin(uint32_t frequency);
    void end();
    int beginPacket(int implicitHeader = 0);
    int endPacket(bool async = false);
    bool isTxBusy();
    bool txFailed() const { return _txFailed; }
    size_t write(uint8_t byte) { return write(&byte, 1); }
    size_t write(const uint8_t* buffer, size_t size);
    void receive(int size = 0);
    int parsePacket(int size = 0);
    const uint8_t* packetBuffer() const { return _packet; }
    void setFrequency(uint32_t frequency);
    uint32_t getFrequency() const { return _frequency; }
    void setTxPower(int level);
    int8_t getTxPower() const { return _power; }
    void setSpreadingFactor(int sf);
    uint8_t getSpreadingFactor() const { return _sf; }
    void setSignalBandwidth(uint32_t hz);
    uint32_t getSignalBandwidth() const { return _bandwidth; }
    void setCodingRate4(int denominator);
    uint8_t getCodingRate4() const { return _cr; }
    void setPreambleLength(long symbols);
    long getPreambleLength() const { return _preamble; }
    void setInvertIQ(bool enabled);
    bool getInvertIQ() const { return _invertIq; }
    bool isRadioOnline() const { return _online; }
    int currentRssi();
    int packetRssi() const { return _rssi; }
    float packetSnr() const { return _snr; }
    uint8_t getStatus();
    uint32_t getIrqFlags();
    float getAirtime(uint16_t bytes);
    uint32_t getBitrate() const;
    bool lowDataRateEnabled() const;
    void standby();
    void sleep();
    using YieldCallback = void(*)();
    void setYieldCallback(YieldCallback cb) { _yield = cb; }
    void printDiagnostics();
    volatile bool packetAvailable = false;
private:
    bool configureReady();
    bool checked(int16_t result, const char* operation);
    static void IRAM_ATTR interrupt();
    static SensecapSX1262Radio* _instance;
    SensecapRadioHal _hal;
    Module _module;
    SX1262 _chip;
    uint8_t _packet[MAX_PACKET_SIZE]{};
    size_t _written = 0;
    uint32_t _frequency = LORA_DEFAULT_FREQ, _bandwidth = LORA_DEFAULT_BW;
    uint32_t _txStarted = 0, _txBudget = 0;
    uint8_t _sf = LORA_DEFAULT_SF, _cr = LORA_DEFAULT_CR;
    int8_t _power = LORA_DEFAULT_TX_POWER;
    uint16_t _preamble = LORA_DEFAULT_PREAMBLE;
    int _rssi = -127;
    float _snr = 0;
    int16_t _lastError = 0;
    bool _online = false, _tx = false, _txFailed = false, _invertIq = false;
    YieldCallback _yield = nullptr;
};
#endif
