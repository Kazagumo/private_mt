#define LED_PIN 15 // If defined we will blink this LED
#define LED_STATE_ON 1

#define I2C_SDA 1 // I2C pins for this board
#define I2C_SCL 2

#define BUTTON_PIN 9 // shared with boot pin,don't press when powering on.
#define PIN_BUZZER 3 // PWM Buzzer

#define BATTERY_PIN 6 // A battery voltage measurement pin, voltage divider connected here to measure battery voltage
#define ADC_CHANNEL ADC_CHANNEL_6
#define ADC_MULTIPLIER 2.11

#define PIN_SERIAL1_RX 17 // U0RXD
#define PIN_SERIAL1_TX 16 // U0TXD Both can be reconfigured for other purpose.
/*
#define HAS_TRACKBALL 1
#define TB_UP 5
#define TB_DOWN 0
#define TB_LEFT 4
#define TB_RIGHT 7
#define TB_PRESS 15
#define TB_DIRECTION FALLING
*/

#define USE_SX1262
#define LORA_SCK 18
#define LORA_MISO 20
#define LORA_MOSI 19
#define LORA_CS 11
#define LORA_RESET 21
#define SX126X_CS LORA_CS
#define SX126X_DIO1 23
#define SX126X_BUSY 22
#define SX126X_RESET LORA_RESET
#define SX126X_RXEN 10
#define SX126X_DIO2_AS_RF_SWITCH
#define SX126X_TXEN RADIOLIB_NC
#define SX126X_DIO3_TCXO_VOLTAGE 1.8
#define TCXO_OPTIONAL