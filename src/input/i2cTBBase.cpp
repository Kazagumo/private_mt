#include "i2cTBBase.h"
#include "configuration.h"
#if HAS_TRACKBALL_I2C
#include "i2cTBImpl1.h"
extern bool osk_found;

i2cTBBase::i2cTBBase(const char *name) : concurrency::OSThread(name), _originName(name) {}

void i2cTBBase::init(uint8_t pinInt, input_broker_event eventDown, input_broker_event eventUp, input_broker_event eventLeft,
                     input_broker_event eventRight, input_broker_event eventPressed, input_broker_event eventPressedLong,
                     void (*onInt)())
{
    this->_pinDown = 1;
    this->_pinUp = 2;
    this->_pinLeft = 3;
    this->_pinRight = 4;
    this->_pinPress = 0;
    this->_eventDown = eventDown;
    this->_eventUp = eventUp;
    this->_eventLeft = eventLeft;
    this->_eventRight = eventRight;
    this->_eventPressed = eventPressed;
    this->_eventPressedLong = eventPressedLong;

    this->pcf8574IntFlag = false;

    pcf8574 = new PCF8574();
    pcf8574->begin(0x20);

    if (pinInt != 255) {
        pinMode(pinInt, INPUT_PULLUP);
        attachInterrupt(pinInt, onInt, FALLING);
    }

    LOG_DEBUG("I2C Trackball GPIO initialized - Interrupt %d", pinInt);
    osk_found = true;
    this->setInterval(100);
}

int32_t i2cTBBase::runOnce()
{
    if (this->pcf8574IntFlag) {
        uint8_t ret = pcf8574->readAll();
        switch (ret) {
        case 0b11111110: // P0 press
            this->intPressHandler();
            break;
        case 0b11111101: // P1 down
            this->intDownHandler();
            break;
        case 0b11111011: // P2 up
            this->intUpHandler();
            break;
        case 0b11110111: // P3 left
            this->intLeftHandler();
            break;
        case 0b11101111: // P4 right
            this->intRightHandler();
            break;
        case 0b11011111: // P5 usrbutton
            this->action = TB_ACTION_USER;
            break;
        case 0b10111111: // P6 cancel
            this->action = TB_ACTION_CANCEL;
            break;
        case 0b01111111: // P7 GPS
            break;
        default:
            break;
        }
    }

    InputEvent e = {};
    e.inputEvent = INPUT_BROKER_NONE;

    // Handle long press detection for press button
    if (pressDetected && pressStartTime > 0) {
        uint32_t pressDuration = millis() - pressStartTime;
        bool buttonStillPressed = false;
        buttonStillPressed = !pcf8574->digitalRead(_pinPress);

        if (!buttonStillPressed) {
            // Button released
            if (pressDuration < LONG_PRESS_DURATION) {
                // Short press
                e.inputEvent = this->_eventPressed;
            }
            // Reset state
            pressDetected = false;
            pressStartTime = 0;
            lastPressKeyTime = 0;
            this->action = TB_ACTION_NONE;
        } else if (pressDuration >= LONG_PRESS_DURATION) {
            // Long press detected
            uint32_t currentTime = millis();
            // Only trigger long press event if enough time has passed since the last one
            if (lastPressKeyTime == 0 || (currentTime - lastPressKeyTime) >= LONG_PRESS_REPEAT_INTERVAL) {
                e.inputEvent = this->_eventPressedLong;
                lastPressKeyTime = currentTime;
            }
            this->action = TB_ACTION_PRESSED_LONG;
        }
    }

    if (directionDetected && directionStartTime > 0) {
        uint32_t directionDuration = millis() - directionStartTime;
        // uint8_t directionPressed = this->action;
        uint8_t directionPressedNow = 0;
        directionInterval++;

        if (!pcf8574->digitalRead(_pinUp)) {
            directionPressedNow = TB_ACTION_UP;
        } else if (!pcf8574->digitalRead(_pinDown)) {
            directionPressedNow = TB_ACTION_DOWN;
        } else if (!pcf8574->digitalRead(_pinLeft)) {
            directionPressedNow = TB_ACTION_LEFT;
        } else if (!pcf8574->digitalRead(_pinRight)) {
            directionPressedNow = TB_ACTION_RIGHT;
        }

        if (directionPressedNow < 3) {
            // Reset state
            directionDetected = false;
            directionStartTime = 0;
            directionInterval = 0;
            this->action = TB_ACTION_NONE;
        } else if (directionDuration >= LONG_PRESS_DURATION && directionPressedNow >= 3 && directionInterval >= 3) {
            // repeat event when long press these direction.
            switch (directionPressedNow) {
            case TB_ACTION_UP:
                e.inputEvent = this->_eventUp;
                break;
            case TB_ACTION_DOWN:
                e.inputEvent = this->_eventDown;
                break;
            case TB_ACTION_LEFT:
                e.inputEvent = this->_eventLeft;
                break;
            case TB_ACTION_RIGHT:
                e.inputEvent = this->_eventRight;
                break;
            }

            directionInterval = 0;
        }
    }

    if (this->action == TB_ACTION_PRESSED && !pcf8574->digitalRead(_pinPress) && !pressDetected) {
        // Start long press detection
        pressDetected = true;
        pressStartTime = millis();
        // Don't send event yet, wait to see if it's a long press
    } else if (this->action == TB_ACTION_UP && !pcf8574->digitalRead(_pinUp) && !directionDetected) {
        directionDetected = true;
        directionStartTime = millis();
        e.inputEvent = this->_eventUp;
        // send event first,will automatically trigger every 50ms * 3 after 500ms
    } else if (this->action == TB_ACTION_DOWN && !pcf8574->digitalRead(_pinDown) && !directionDetected) {
        directionDetected = true;
        directionStartTime = millis();
        e.inputEvent = this->_eventDown;
    } else if (this->action == TB_ACTION_LEFT && !pcf8574->digitalRead(_pinLeft) && !directionDetected) {
        directionDetected = true;
        directionStartTime = millis();
        e.inputEvent = this->_eventLeft;
    } else if (this->action == TB_ACTION_RIGHT && !pcf8574->digitalRead(_pinRight) && !directionDetected) {
        directionDetected = true;
        directionStartTime = millis();
        e.inputEvent = this->_eventRight;
    } else if (this->action == TB_ACTION_USER) {
        e.inputEvent = INPUT_BROKER_USER_PRESS;
    } else if (this->action == TB_ACTION_CANCEL) {
        e.inputEvent = INPUT_BROKER_CANCEL;
    }

    if (e.inputEvent != INPUT_BROKER_NONE) {
        e.source = this->_originName;
        e.kbchar = 0x00;
        this->notifyObservers(&e);
    }

    // Only update lastEvent for non-press actions or completed press actions
    if (this->action != TB_ACTION_PRESSED || !pressDetected) {
        lastEvent = action;
        if (!pressDetected) {
            this->action = TB_ACTION_NONE;
        }
    }
    if (i2cTBImpl1->pcf8574IntFlag)
        i2cTBImpl1->pcf8574IntFlag = false;
    return 50; // Check more frequently for better long press detection
}

void i2cTBBase::intPressHandler()
{
    this->action = TB_ACTION_PRESSED;
}

void i2cTBBase::intDownHandler()
{
    this->action = TB_ACTION_DOWN;
}

void i2cTBBase::intUpHandler()
{
    this->action = TB_ACTION_UP;
}

void i2cTBBase::intLeftHandler()
{
    this->action = TB_ACTION_LEFT;
}

void i2cTBBase::intRightHandler()
{
    this->action = TB_ACTION_RIGHT;
}

void i2cTBBase::toggleGPS(bool stat)
{
    pcf8574->digitalWrite(7, stat);
}

#endif
