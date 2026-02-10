#define I2C_SDA 1 // I2C pins for this board
#define I2C_SCL 8

// #define LED_PIN 7      // If defined we will blink this LED
// #define LED_STATE_ON 0 // State when LED is lit
#define BUTTON_PIN 9

#define BATTERY_PIN 6 // A battery voltage measurement pin, voltage divider connected here to measure battery voltage
#define ADC_CHANNEL ADC1_GPIO6_CHANNEL
#define ADC_MULTIPLIER 2.11

#define HAS_NEOPIXEL                         // Enable the use of neopixels
#define NEOPIXEL_COUNT 1                     // How many neopixels are connected
#define NEOPIXEL_DATA 2                      // gpio pin used to send data to the neopixels
#define NEOPIXEL_TYPE (NEO_GRB + NEO_KHZ800) // type of neopixels in use

#define HAS_TRACKBALL 1
#define TB_UP 5
#define TB_DOWN 0
#define TB_LEFT 4
#define TB_RIGHT 7
#define TB_PRESS 15
#define TB_DIRECTION FALLING

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

#define SERIAL_PRINT_PORT 1
