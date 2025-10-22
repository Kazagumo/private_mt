#pragma once
#include "i2cTBBase.h"

#if HAS_TRACKBALL_I2C

class I2cTBImpl1 : public i2cTBBase
{
  public:
    I2cTBImpl1();
    void init(uint8_t pinInt);
    static void handleInt();
};

class PCF8574
{
  public:
    PCF8574();
    bool begin(int address);
    uint8_t digitalRead(int pin);
    void digitalWrite(int pin, int value);
    uint8_t readAll();

  private:
    int _address;
    int _PinVal;
};

extern I2cTBImpl1 *i2cTBImpl1;
extern PCF8574 *pcf8574;

#endif
