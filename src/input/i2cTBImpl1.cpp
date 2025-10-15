#include "i2cTBImpl1.h"
#include "InputBroker.h"
#include "configuration.h"
#include <Wire.h>
#if HAS_TRACKBALL_I2C

I2cTBImpl1 *i2cTBImpl1;
PCF8574 *pcf8574;

I2cTBImpl1::I2cTBImpl1() : i2cTBBase("i2ctrackball1") {}

void I2cTBImpl1::init(uint8_t pinInt)
{
    input_broker_event eventDown = INPUT_BROKER_DOWN;
    input_broker_event eventUp = INPUT_BROKER_UP;
    input_broker_event eventLeft = INPUT_BROKER_LEFT;
    input_broker_event eventRight = INPUT_BROKER_RIGHT;
    input_broker_event eventPressed = INPUT_BROKER_SELECT;
    input_broker_event eventPressedLong = INPUT_BROKER_SELECT_LONG;

    i2cTBBase::init(pinInt, eventDown, eventUp, eventLeft, eventRight, eventPressed, eventPressedLong, I2cTBImpl1::handleInt);
    inputBroker->registerSource(this);
}

void I2cTBImpl1::handleInt()
{
    if (millis() > i2cTBImpl1->lastTime + 10 && !i2cTBImpl1->pcf8574IntFlag) {
        i2cTBImpl1->lastTime = millis();
        i2cTBImpl1->pcf8574IntFlag = true;
        i2cTBImpl1->setIntervalFromNow(20);
    }
}

PCF8574::PCF8574()
{
    // Constructor
    _PinVal = 0xFF;
}

bool PCF8574::begin(int address)
{
    _address = address;
    Wire.beginTransmission(_address);
    Wire.write(_PinVal);
    int result = Wire.endTransmission();
    return (result == 0);
}

uint8_t PCF8574::digitalRead(int pin)
{
    Wire.requestFrom(_address, 1);
    return (Wire.read() & (1 << pin)) ? HIGH : LOW;
}

uint8_t PCF8574::readAll()
{
    Wire.requestFrom(_address, 1);
    return Wire.read();
}

#endif
