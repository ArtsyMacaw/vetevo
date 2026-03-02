#ifndef DISPLAY_H
#define DISPLAY_H

/* Magic numbers for display commands */
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
#define MOSI_PIN    GPIO_NUM_15
#define SCK_PIN     GPIO_NUM_17
#define CS_PIN      GPIO_NUM_3
#define DC_PIN      GPIO_NUM_4
#define RST_PIN     GPIO_NUM_9
#define BUSY_PIN    GPIO_NUM_18

/* GPIO Pin states */
#define HIGH    1
#define LOW     0

/* Display dimensions */
#define EPD_WIDTH  800
#define EPD_HEIGHT 480

/* Clock coordinates for display */
#define CLOCK_X 432
#define CLOCK_Y 100

#endif
