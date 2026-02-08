#ifndef DISPLAY_H
#define DISPLAY_H

/* Type definitions for display data */
#define UBYTE uint8_t
#define UWORD uint16_t
#define UDOUBLE uint32_t

/* Magic numbers for display commands in order of use in startup */
#define POWER_SETTING       0x01
#define BOOSTER_SOFT_START  0x06
#define POWER_ON            0x04
#define PANEL_SETTING       0x00
#define RESOLUTION_SETTING  0x61
#define DUAL_SPI            0x15
#define TCON_SETTING        0x60
#define VCOM_DATA_INTERVAL  0x50
#define VCM_DC              0x82
#define TRANSFER_DATA_1     0x10
#define TRANSFER_DATA_2     0x13
#define DISPLAY_REFRESH     0x12
#define POWER_OFF           0x02
#define DEEP_SLEEP          0x07

/* GPIO Pin mappings */
#define MOSI_PIN    23
#define SCK_PIN     18
#define CS_PIN      2
#define DC_PIN      0
#define RST_PIN     13
#define BUSY_PIN    14

/* GPIO Pin states */
#define HIGH    1
#define LOW     0

/* Display dimensions */
#define EPD_WIDTH 800
#define EPD_HEIGHT 480

#endif
