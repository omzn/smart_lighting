#ifndef ESP_LIGHTING_H
#define ESP_LIGHTING_H

#define DEBUG

//#define PIN_SDA          (2)
//#define PIN_SCL          (5)

#define PIN_LIGHT         (15)
// pin #15 is hw pulled down

//#define PIN_RX             (4)
//#define PIN_TX            (13)
//#define PIN_TFT_DC       (4)
//#define PIN_TFT_CS      (15)
//#define PIN_SD_CS       (16)
//#define PIN_SPI_CLK   (14)
//#define PIN_SPI_MOSI  (13)
//#define PIN_SPI_MISO  (12)

#define EEPROM_SSID_ADDR             (0)
#define EEPROM_PASS_ADDR            (32)
#define EEPROM_MDNS_ADDR            (96)
#define EEPROM_SCHEDULE_ADDR       (128)
#define EEPROM_DIM_ADDR            (128+5)
#define EEPROM_MAX_ADDR            (133+2)
#define EEPROM_POWER_ADDR          (135+2)
#define EEPROM_LAST_ADDR           (137+144)

#define SECONDS_UTC_TO_JST (32400)

#endif
