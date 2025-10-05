// For OLED LCD
#define I2C_SDA 21
#define I2C_SCL 22

// GPS
#undef GPS_RX_PIN
#define GPS_RX_PIN 15

#define BUTTON_PIN 2 // The middle button GPIO on the T-Beam
#define BUTTON_NEED_PULLUP
#define EXT_NOTIFY_OUT 12 // Overridden default pin to use for Ext Notify Module (#975).

// supported modules list
#define USE_RF95 // RFM95/SX127x
#define USE_SX1262
#define USE_SX1268
#define USE_LLCC68

#ifdef EBYTE_E22
// Internally the TTGO module hooks the SX126x-DIO2 in to control the TX/RX switch
// (which is the default for the sx1262interface code)
#define SX126X_DIO3_TCXO_VOLTAGE 1.8
#define TCXO_OPTIONAL // make it so that the firmware can try both TCXO and XTAL
#endif
