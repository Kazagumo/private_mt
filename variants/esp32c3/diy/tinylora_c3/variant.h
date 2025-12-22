#ifndef _VARIANT_TINYLORA_C3_
#define _VARIANT_TINYLORA_C3_

// Board pins configuration for TinyLoRa-C3
// https://github.com/Mymetu/TinyLora-C3

// Ext Notif
#ifndef TL_NOTIF_EXCLUDE
#define PIN_BUZZER 11
#define HAS_NEOPIXEL                         // Enable the use of neopixels
#define NEOPIXEL_COUNT 1                     // How many neopixels are connected
#define NEOPIXEL_DATA 13                     // gpio pin used to send data to the neopixels
#define NEOPIXEL_TYPE (NEO_GRB + NEO_KHZ800) // type of neopixels in use
#endif

// I2C
#define WIRE_INTERFACES_COUNT 1
#define I2C_SDA 0
#define I2C_SCL 1

// ADC
#ifndef TL_NOTIF_EXCLUDE
#define BATTERY_IMMUTABLE
#define BATTERY_PIN 2 // A battery voltage measurement pin, voltage divider connected here to measure battery voltage
#define ADC_CHANNEL ADC1_CHANNEL_2
#define ADC_MULTIPLIER 2.0
#endif

// Trackball
#define HAS_TRACKBALL_I2C 1
#ifndef TL_NOTIF_EXCLUDE
#define TB_INT 9
#else
#define TB_INT 2
#endif

// Screen
#define HAS_SCREEN 1
#define USE_SSD1306

// GPS
#ifdef TL_GPS
#define HAS_GPS 1
#define GPS_RX_PIN 21
#define GPS_TX_PIN 20
#define PIN_GPS_EN 12
#define GPS_EN_ACTIVE 1
#endif

// LoRa chips
#define USE_LLCC68
#define USE_SX1262
#define USE_SX1268

// SPI
#define LORA_SCK 10
#define LORA_MISO 6
#define LORA_MOSI 7
#define LORA_CS 8

// LoRa
#define LORA_DIO0 RADIOLIB_NC // NC
#define LORA_DIO1 3           // IRQ
#define LORA_DIO2 RADIOLIB_NC // Attached internally to rfswitch.
#define LORA_BUSY 4
#define LORA_RESET 5

#define SX126X_CS LORA_CS
#define SX126X_DIO1 LORA_DIO1
#define SX126X_BUSY LORA_BUSY
#define SX126X_RESET LORA_RESET
#define SX126X_DIO2_AS_RF_SWITCH

/*

Tested modules:
| Mfr          | Module           | TCXO | RF Switch | Notes                                 |
| ------------ | ---------------- | ---- | --------- | ------------------------------------- |
| 255 Mesh     | 255MN-L03        | No   | Int       | LLCC68                                |
| AI-Thinker   | RA-01SC          | No   | Int       | LLCC68                                |
| AI-Thinker   | RA-01SC-P        | No   | Int       | LLCC68,29dbm tx power                 |
| AI-Thinker   | RA-01S           | No   | Int       | SX1268                                |
| AI-Thinker   | RA-01S-P         | No   | Int       | SX1268,29dbm tx power                 |
| AI-Thinker   | RA-01SH          | No   | Int       | SX1262                                |
| Heltec       | HT-RA62          | Yes  | Int       | SX1262                                |

*/

#define SX126X_DIO3_TCXO_VOLTAGE 1.8 // reserved for HT-RA62
#define TCXO_OPTIONAL                // make it so that the firmware can try both TCXO and XTAL

#endif