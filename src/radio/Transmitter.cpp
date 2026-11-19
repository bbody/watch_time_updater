#include <Arduino.h>
#include <Ticker.h>

#include "Transmitter.h"
#include "jjy/JjyEncoder.h"
#include "../time/TimeSource.h"

namespace Transmitter {
    namespace {
        constexpr uint8_t LEDC_PIN = 13;  // BF-018A signal in (G22 stack net)
        constexpr uint8_t LEDC_CHANNEL = 0;
        constexpr uint32_t JJY_FREQUENCY_HZ = 40000;
        constexpr uint8_t LEDC_RESOLUTION = 8;
        constexpr uint32_t LEDC_DUTY_ON = 128;
        constexpr uint32_t LEDC_DUTY_OFF = 0;
        constexpr int TICKER_INTERVAL_MS = 100;

        Ticker ticker;
        bool ledcReady = false;
        bool transmitting = false;

        void writeLedcDuty(uint32_t duty) {
            if (!ledcReady) {
                return;
            }

#if defined(ESP_ARDUINO_VERSION) \
    && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
            ledcWrite(LEDC_PIN, duty);
#else
            ledcWrite(LEDC_CHANNEL, duty);
#endif
        }

        void carrierOn() {
            writeLedcDuty(LEDC_DUTY_ON);
        }

        void carrierOff() {
            writeLedcDuty(LEDC_DUTY_OFF);
        }

        void onTicker() {
            JjyEncoder::onTicker();
        }

        bool setupLedc() {
            if (ledcReady) {
                return true;
            }

            uint32_t freq = 0;

#if defined(ESP_ARDUINO_VERSION) \
    && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
            freq = ledcAttach(LEDC_PIN, JJY_FREQUENCY_HZ, LEDC_RESOLUTION);
#else
            freq = ledcSetup(LEDC_CHANNEL, JJY_FREQUENCY_HZ, LEDC_RESOLUTION);
            ledcAttachPin(LEDC_PIN, LEDC_CHANNEL);
#endif

            if (freq < JJY_FREQUENCY_HZ / 2) {
                Serial.printf(
                    "[JJY] LEDC failed: %u Hz on GPIO %u (wanted %u) — check FQBN/board\n",
                    freq,
                    LEDC_PIN,
                    JJY_FREQUENCY_HZ
                );
                return false;
            }

            ledcReady = true;
            writeLedcDuty(LEDC_DUTY_OFF);

            Serial.printf(
                "[JJY] LEDC %u Hz on GPIO %u\n",
                freq,
                LEDC_PIN
            );
            return true;
        }

        void alignTickerPhase() {
            struct timespec ts = {};
            clock_gettime(CLOCK_REALTIME, &ts);

            const uint32_t delayUs =
                (150000000 - static_cast<uint32_t>(ts.tv_nsec % 100000000))
                / 1000;

            delayMicroseconds(delayUs);
        }

        void startTransmission() {
            // JJY emitting disabled — re-enable block below (LEDC on GPIO 13 → BF-018A)
            return;

            /*
            if (transmitting) {
                return;
            }

            if (!setupLedc()) {
                return;
            }

            alignTickerPhase();

            ticker.attach_ms(TICKER_INTERVAL_MS, onTicker);
            transmitting = true;

            Serial.println("[JJY] transmission started");
            */
        }
    }

    void begin() {
        // JJY emitting disabled
        // JjyEncoder::setCarrierCallbacks(carrierOn, carrierOff);
    }

    void stop() {
        if (transmitting) {
            ticker.detach();
            transmitting = false;
            carrierOff();
            Serial.println("[JJY] transmission stopped");
        }
    }

    void update(Mode mode) {
        Time::Source& source = Time::sourceForMode(mode);

        if (!source.isTimeValid()) {
            stop();
            return;
        }

        startTransmission();
    }
}
