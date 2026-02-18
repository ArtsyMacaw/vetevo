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
#define PARTIAL_WINDOW      0x90
#define PARTIAL_IN          0x91
#define PARTIAL_OUT         0x92

/* GPIO Pin mappings */
#define MOSI_PIN    25  // 6
#define SCK_PIN     26  // 7
#define CS_PIN      2   // 12
#define DC_PIN      0   // 11
#define RST_PIN     13  // 14
#define BUSY_PIN    14  // 16

/* GPIO Pin states */
#define HIGH    1
#define LOW     0

/* Display dimensions */
#define EPD_WIDTH 800
#define EPD_HEIGHT 480

extern unsigned char background[];

#endif
