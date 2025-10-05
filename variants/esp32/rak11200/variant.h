#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define LED_GREEN 12
#define LED_BLUE 2

#define LED_BUILTIN LED_GREEN

static const uint8_t TX = 1;
static const uint8_t RX = 3;

#define TX1 21
#define RX1 19

#define WB_IO1 14
#define WB_IO2 27
#define WB_IO3 26
#define WB_IO4 23
#define WB_IO5 13
#define WB_IO6 22
#define WB_SW1 34
#define WB_A0 36
#define WB_A1 39
#define WB_CS 32
#define WB_LED1 12
#define WB_LED2 2

static const uint8_t SDA = 4;
static const uint8_t SCL = 5;

static const uint8_t SS = 32;
static const uint8_t MOSI = 25;
static const uint8_t MISO = 35;
static const uint8_t SCK = 33;
#endif /* Pins_Arduino_h */

/* -------- Meshtastic pins -------- */
#define I2C_SDA SDA
#define I2C_SCL SCL

#define LED_PIN LED_BLUE

#define PIN_VBAT WB_A0
#define BATTERY_PIN PIN_VBAT
#define ADC_CHANNEL ADC1_GPIO36_CHANNEL

// https://docs.rakwireless.com/Product-Categories/WisBlock/RAK13300/

#define LORA_DIO0 13  // BUSY
#define LORA_RESET 23 // RST for SX1276, and for SX1262/SX1268
#define LORA_DIO1 22  // IRQ for SX1262/SX1268

// In transmitting, set TXEN as high communication level，RXEN pin is low level;
// In receiving, set RXEN as high communication level, TXEN is lowlevel;
// Before powering off, set TXEN、RXEN as low level.
#define LORA_RXEN 27

#undef LORA_SCK
#define LORA_SCK 33
#undef LORA_MISO
#define LORA_MISO 35
#undef LORA_MOSI
#define LORA_MOSI 25
#undef LORA_CS
#define LORA_CS 32

#define USE_SX1268
// common pinouts for SX126X modules
#define SX126X_CS LORA_CS
#define SX126X_DIO1 LORA_DIO1
#define SX126X_DIO2_AS_RF_SWITCH
#define SX126X_BUSY LORA_DIO0
#define SX126X_RESET LORA_RESET
#define SX126X_RXEN LORA_RXEN
#define SX126X_TXEN RADIOLIB_NC

#define SX126X_MAX_POWER 18
#define SX126X_DIO3_TCXO_VOLTAGE 1.8
#define TCXO_OPTIONAL // make it so that the firmware can try both TCXO and XTAL
