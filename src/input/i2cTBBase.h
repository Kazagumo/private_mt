#pragma once

#include "InputBroker.h"
#include "mesh/NodeDB.h"

#if HAS_TRACKBALL_I2C

#include <atomic>

#ifndef TB_DIRECTION
#if ARCH_PORTDUINO
#include "PortduinoGlue.h"
#define TB_DIRECTION (PinStatus) portduino_config.lora_usb_vid
#else
#define TB_DIRECTION RISING
#endif
#endif

class i2cTBBase : public Observable<const InputEvent *>, public concurrency::OSThread
{
  public:
    explicit i2cTBBase(const char *name);
    void init(uint8_t pinInt, input_broker_event eventDown, input_broker_event eventUp, input_broker_event eventLeft,
              input_broker_event eventRight, input_broker_event eventPressed, input_broker_event eventPressedLong,
              void (*onInt)());
    void intPressHandler();
    void intDownHandler();
    void intUpHandler();
    void intLeftHandler();
    void intRightHandler();
    uint32_t lastTime = 0;

    virtual int32_t runOnce() override;

  protected:
    enum i2cTBBaseActionType {
        TB_ACTION_NONE,
        TB_ACTION_PRESSED,
        TB_ACTION_PRESSED_LONG,
        TB_ACTION_UP,
        TB_ACTION_DOWN,
        TB_ACTION_LEFT,
        TB_ACTION_RIGHT,
        TB_ACTION_USER,
        TB_ACTION_CANCEL,

    };
    uint8_t _pinDown = 0;
    uint8_t _pinUp = 0;
    uint8_t _pinLeft = 0;
    uint8_t _pinRight = 0;
    uint8_t _pinPress = 0;

    volatile i2cTBBaseActionType action = TB_ACTION_NONE;

    // Long press detection for press button
    uint32_t pressStartTime = 0;
    uint32_t directionStartTime = 0;
    uint8_t directionInterval = 0;
    std::atomic<bool> pcf8574IntFlag;
    bool pressDetected = false;
    bool directionDetected = false;
    uint32_t lastPressKeyTime = 0;
    uint32_t lastDirectionPressEventTime = 0;
    static const uint32_t LONG_PRESS_DURATION = 300;        // ms
    static const uint32_t LONG_PRESS_REPEAT_INTERVAL = 300; // ms - interval between repeated long press events

  private:
    input_broker_event _eventDown = INPUT_BROKER_NONE;
    input_broker_event _eventUp = INPUT_BROKER_NONE;
    input_broker_event _eventLeft = INPUT_BROKER_NONE;
    input_broker_event _eventRight = INPUT_BROKER_NONE;
    input_broker_event _eventPressed = INPUT_BROKER_NONE;
    input_broker_event _eventPressedLong = INPUT_BROKER_NONE;
    const char *_originName;
    i2cTBBaseActionType lastEvent = TB_ACTION_NONE;
};

#endif