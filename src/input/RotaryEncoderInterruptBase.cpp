#include "RotaryEncoderInterruptBase.h"
#include "configuration.h"

RotaryEncoderInterruptBase::RotaryEncoderInterruptBase(const char *name) : concurrency::OSThread(name)
{
    this->_originName = name;
}

void RotaryEncoderInterruptBase::init(
    uint8_t pinA, uint8_t pinB, uint8_t pinPress, input_broker_event eventCw, input_broker_event eventCcw,
    input_broker_event eventPressed, input_broker_event eventPressedLong,
    //    std::function<void(void)> onIntA, std::function<void(void)> onIntB, std::function<void(void)> onIntPress) :
    void (*onIntA)(), void (*onIntB)(), void (*onIntPress)())
{
    this->_pinA = pinA;
    this->_pinB = pinB;
    this->_pinPress = pinPress;
    this->_eventCw = eventCw;
    this->_eventCcw = eventCcw;
    this->_eventPressed = eventPressed;
    this->_eventPressedLong = eventPressedLong;

    bool isRAK = false;
#ifdef RAK_4631
    isRAK = true;
#endif

    if (!isRAK || pinPress != 0) {
        pinMode(pinPress, INPUT_PULLUP);
        attachInterrupt(pinPress, onIntPress, CHANGE);
    }
    if (!isRAK || this->_pinA != 0) {
        pinMode(this->_pinA, INPUT_PULLUP);
        attachInterrupt(this->_pinA, onIntA, CHANGE);
    }
    if (!isRAK || this->_pinA != 0) {
        pinMode(this->_pinB, INPUT_PULLUP);
        attachInterrupt(this->_pinB, onIntB, CHANGE);
    }

    this->rotaryLevelA = digitalRead(this->_pinA);
    this->rotaryLevelB = digitalRead(this->_pinB);
    LOG_INFO("Rotary initialized (%d, %d, %d)", this->_pinA, this->_pinB, pinPress);
}

int32_t RotaryEncoderInterruptBase::runOnce()
{
    InputEvent e = {};
    e.inputEvent = INPUT_BROKER_NONE;
    // Handle long press detection for press button
    if (pressDetected && pressStartTime > 0) {
        uint32_t pressDuration = millis() - pressStartTime;
        bool buttonStillPressed = false;

#if defined(T_DECK)
        buttonStillPressed = (this->action == TB_ACTION_PRESSED);
#else
        buttonStillPressed = !digitalRead(_pinPress);
#endif

        if (!buttonStillPressed) {
            // Button released - check if it was a short press
            if (pressDuration < LONG_PRESS_DURATION) {
                e.inputEvent = this->_eventPressed;
            }
            // Reset state
            pressDetected = false;
            pressStartTime = 0;
            lastPressKeyTime = 0;
            this->action = ROTARY_ACTION_NONE;
        } else if (pressDuration >= LONG_PRESS_DURATION && lastPressKeyTime == 0) {
            // First long press event only - avoid repeated events that cause lag
            e.inputEvent = this->_eventPressedLong;
            lastPressKeyTime = millis();
        }
    }

    e.source = this->_originName;
    if (this->action == ROTARY_ACTION_PRESSED && !digitalRead(_pinPress) && !pressDetected) {
        // Start long press detection
        LOG_DEBUG("Rotary event Press,Detecting LongPress");
        pressDetected = true;
        pressStartTime = millis();
        // Don't send event yet, wait to see if it's a long press
    } else if (this->action == ROTARY_ACTION_CW) {
        LOG_DEBUG("Rotary event CW");
        e.inputEvent = this->_eventCw;
    } else if (this->action == ROTARY_ACTION_CCW) {
        LOG_DEBUG("Rotary event CCW");
        e.inputEvent = this->_eventCcw;
    }

    if (this->action != ROTARY_ACTION_PRESSED || !pressDetected) {
        this->action = ROTARY_ACTION_NONE;
    }
    if (e.inputEvent != INPUT_BROKER_NONE) {
        e.kbchar = 0x00;
        e.source = this->_originName;
        this->notifyObservers(&e);
    }

    if (!pressDetected) {
        this->action = ROTARY_ACTION_NONE;
    }

    if (pressDetected) {
        return 50;
    } else {
        return INT32_MAX;
    }
}

void RotaryEncoderInterruptBase::intPressHandler()
{
    this->action = ROTARY_ACTION_PRESSED;
    setIntervalFromNow(20); // start checking for long/short
}

void RotaryEncoderInterruptBase::intAHandler()
{
    // CW rotation (at least on most common rotary encoders)
    int currentLevelA = digitalRead(this->_pinA);
    if (this->rotaryLevelA == currentLevelA) {
        return;
    }
    this->rotaryLevelA = currentLevelA;
    this->rotaryStateCCW = intHandler(currentLevelA == HIGH, this->rotaryLevelB, ROTARY_ACTION_CCW, this->rotaryStateCCW);
}

void RotaryEncoderInterruptBase::intBHandler()
{
    // CW rotation (at least on most common rotary encoders)
    int currentLevelB = digitalRead(this->_pinB);
    if (this->rotaryLevelB == currentLevelB) {
        return;
    }
    this->rotaryLevelB = currentLevelB;
    this->rotaryStateCW = intHandler(currentLevelB == HIGH, this->rotaryLevelA, ROTARY_ACTION_CW, this->rotaryStateCW);
}

/**
 * @brief Rotary action implementation.
 *   We assume, the following pin setup:
 *    A   --||
 *    GND --||]========
 *    B   --||
 *
 * @return The new state for rotary pin.
 */
RotaryEncoderInterruptBaseStateType RotaryEncoderInterruptBase::intHandler(bool actualPinRaising, int otherPinLevel,
                                                                           RotaryEncoderInterruptBaseActionType action,
                                                                           RotaryEncoderInterruptBaseStateType state)
{
    RotaryEncoderInterruptBaseStateType newState = state;
    if (actualPinRaising && (otherPinLevel == LOW)) {
        if (state == ROTARY_EVENT_CLEARED) {
            newState = ROTARY_EVENT_OCCURRED;
            if ((this->action != ROTARY_ACTION_PRESSED) && (this->action != action)) {
                this->action = action;
            }
        }
    } else if (!actualPinRaising && (otherPinLevel == HIGH)) {
        // Logic to prevent bouncing.
        newState = ROTARY_EVENT_CLEARED;
    }
    setIntervalFromNow(ROTARY_DELAY); // TODO: this modifies a non-volatile variable!

    return newState;
}
