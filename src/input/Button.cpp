
#include <Arduino.h>
#include "Button.h"

namespace Button {
    namespace {
        constexpr uint32_t DEBOUNCE_MS = 50;
        constexpr uint32_t LONG_PRESS_MS = 800;

        constexpr uint8_t PIN_LEFT = 33;
        constexpr uint8_t PIN_ENTER = 32;
        constexpr uint8_t PIN_RIGHT = 21;

        constexpr int BUTTON_COUNT = 3;

        const uint8_t PINS[BUTTON_COUNT] = {
            PIN_LEFT,
            PIN_ENTER,
            PIN_RIGHT
        };

        volatile uint8_t interruptPendingMask = 0;

        enum class State {
            Idle,
            DebouncePress,
            Pressed,
            DebounceRelease
        };

        struct ButtonState {
            State state = State::Idle;
            uint32_t stateChangedAt = 0;
            uint32_t pressStartedAt = 0;
            bool longPressFired = false;
            bool shortPressAvailable = false;
            bool longPressAvailable = false;
        };

        ButtonState buttons[BUTTON_COUNT];

        int indexOf(Id id) {
            switch (id) {
                case Id::Left:
                    return 0;
                case Id::Enter:
                    return 1;
                case Id::Right:
                    return 2;
            }

            return 1;
        }

        void IRAM_ATTR onInterruptLeft() {
            interruptPendingMask |= 0x01;
        }

        void IRAM_ATTR onInterruptEnter() {
            interruptPendingMask |= 0x02;
        }

        void IRAM_ATTR onInterruptRight() {
            interruptPendingMask |= 0x04;
        }

        const char* nameForIndex(int index) {
            switch (index) {
                case 0:
                    return "left";
                case 1:
                    return "enter";
                case 2:
                    return "right";
                default:
                    return "?";
            }
        }

        void updateOne(int index) {
            const uint8_t pin = PINS[index];
            ButtonState& btn = buttons[index];
            const uint32_t now = millis();
            const uint8_t pendingMask = static_cast<uint8_t>(0x01 << index);

            switch (btn.state) {
                case State::Idle:
                    if (
                        (interruptPendingMask & pendingMask)
                        || digitalRead(pin) == LOW
                    ) {
                        noInterrupts();
                        interruptPendingMask =
                            static_cast<uint8_t>(
                                interruptPendingMask & ~pendingMask
                            );
                        interrupts();

                        btn.stateChangedAt = now;
                        btn.state = State::DebouncePress;
                    }
                    break;

                case State::DebouncePress:
                    if (now - btn.stateChangedAt >= DEBOUNCE_MS) {
                        if (digitalRead(pin) == LOW) {
                            btn.pressStartedAt = now;
                            btn.longPressFired = false;
                            btn.state = State::Pressed;
                        } else {
                            btn.state = State::Idle;
                        }
                    }
                    break;

                case State::Pressed:
                    if (
                        !btn.longPressFired
                        && now - btn.pressStartedAt >= LONG_PRESS_MS
                    ) {
                        btn.longPressFired = true;
                        btn.longPressAvailable = true;
                        Serial.printf(
                            "[Button] %s long press (GPIO %u)\n",
                            nameForIndex(index),
                            pin
                        );
                    }

                    if (digitalRead(pin) == HIGH) {
                        btn.stateChangedAt = now;
                        btn.state = State::DebounceRelease;
                    }
                    break;

                case State::DebounceRelease:
                    if (digitalRead(pin) == LOW) {
                        btn.state = State::Pressed;
                    } else if (now - btn.stateChangedAt >= DEBOUNCE_MS) {
                        noInterrupts();
                        interruptPendingMask =
                            static_cast<uint8_t>(
                                interruptPendingMask & ~pendingMask
                            );
                        interrupts();

                        if (!btn.longPressFired) {
                            btn.shortPressAvailable = true;
                            Serial.printf(
                                "[Button] %s short press (GPIO %u)\n",
                                nameForIndex(index),
                                pin
                            );
                        }

                        btn.state = State::Idle;
                    }
                    break;
            }
        }
    }

    void begin() {
        for (int i = 0; i < BUTTON_COUNT; i++) {
            pinMode(PINS[i], INPUT_PULLUP);
            buttons[i] = ButtonState{};
        }

        const int irqLeft = digitalPinToInterrupt(PIN_LEFT);
        const int irqEnter = digitalPinToInterrupt(PIN_ENTER);
        const int irqRight = digitalPinToInterrupt(PIN_RIGHT);

        if (irqLeft >= 0) {
            attachInterrupt(irqLeft, onInterruptLeft, CHANGE);
        }

        if (irqEnter >= 0) {
            attachInterrupt(irqEnter, onInterruptEnter, CHANGE);
        }

        if (irqRight >= 0) {
            attachInterrupt(irqRight, onInterruptRight, CHANGE);
        }

        Serial.printf(
            "[Button] left=%u enter=%u right=%u\n",
            PIN_LEFT,
            PIN_ENTER,
            PIN_RIGHT
        );
    }

    void update() {
        for (int i = 0; i < BUTTON_COUNT; i++) {
            updateOne(i);
        }
    }

    bool wasPressed(Id id) {
        const int index = indexOf(id);
        ButtonState& btn = buttons[index];

        if (!btn.shortPressAvailable) {
            return false;
        }

        btn.shortPressAvailable = false;
        return true;
    }

    bool wasLongPressed(Id id) {
        const int index = indexOf(id);
        ButtonState& btn = buttons[index];

        if (!btn.longPressAvailable) {
            return false;
        }

        btn.longPressAvailable = false;
        return true;
    }

    bool isHeld(Id id) {
        const int index = indexOf(id);
        const ButtonState& btn = buttons[index];

        return btn.state == State::Pressed
            || btn.state == State::DebounceRelease;
    }

    bool wasPressed() {
        return wasPressed(Id::Enter);
    }

    bool wasLongPressed() {
        return wasLongPressed(Id::Enter);
    }

    bool isHeld() {
        return isHeld(Id::Enter);
    }
}
