#include "soc/soc_ulp.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
    .bss
    .macro spi_delay
        WAIT 10
    .endm
    .macro CS_HIGH
        WRITE_RTC_REG(RTC_GPIO_OUT_W1TS_REG, RTC_GPIO_OUT_DATA_W1TS_S+12, 1, 1)
    .endm    
    .macro CS_LOW
        WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TC_S+12, 1, 1)
    .endm
    .macro DC_HIGH
        WRITE_RTC_REG(RTC_GPIO_OUT_W1TS_REG, RTC_GPIO_OUT_DATA_W1TS_S+11, 1, 1)
    .endm
    .macro DC_LOW
        WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TC_S+11, 1, 1)
    .endm
    .macro MOSI_HIGH
        WRITE_RTC_REG(RTC_GPIO_OUT_W1TS_REG, RTC_GPIO_OUT_DATA_W1TS_S+6, 1, 1)
    .endm
    .macro MOSI_LOW
        WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TC_S+6, 1, 1)
    .endm
    .macro SCK_HIGH
        WRITE_RTC_REG(RTC_GPIO_OUT_W1TS_REG, RTC_GPIO_OUT_DATA_W1TS_S+7, 1, 1)
    .endm
    .macro SCK_LOW
        WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TC_S+7, 1, 1)
    .endm
    .macro RST_HIGH
        WRITE_RTC_REG(RTC_GPIO_OUT_W1TS_REG, RTC_GPIO_OUT_DATA_W1TS_S+14, 1, 1)
    .endm
    .macro RST_LOW
        WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TC_S+14, 1, 1)
    .endm
    .macro READ_BUSY
            READ_RTC_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S+16, 1)
    .endm

    /* Four general 16 bit registers are available
     * R0 - Used to increment through loops
     * R1 - Function argument
     * R2 - Temp storage used during functions
     * R3 - Return address after functions done */

    .text
    .global entry
entry:
    MOVE R0, 0
epd_reset:
    /* 20ms = 172, 4ms = 29 
     * Numbers indicate clock cycles */
    RST_HIGH
    WAIT 172
    RST_LOW
    WAIT 29
    RST_HIGH
    WAIT 172
    MOVE R3, epd_init
epd_init:
    MOVE R1, 0x06
    JUMPR spi_write_command, 0, EQ
    MOVE R1, 0x17
    JUMPR spi_write_data, 1, EQ
    MOVE R1, 0x17
    JUMPR spi_write_data, 2, EQ
    MOVE R1, 0x27
    JUMPR spi_write_data, 3, EQ
    MOVE R1, 0x17
    JUMPR spi_write_data, 4, EQ
    MOVE R1, 0x01
    JUMPR spi_write_command, 5, EQ
    MOVE R1, 0x07
    JUMPR spi_write_data, 6, EQ
    MOVE R1, 0x17
    JUMPR spi_write_data, 7, EQ
    MOVE R1, 0x3f
    JUMPR spi_write_data, 8, EQ
    MOVE R1, 0x3f
    JUMPR spi_write_data, 9, EQ
    MOVE R1, 0x04
    JUMPR spi_write_command, 10, EQ
    JUMPR epd_wait_busy, 11, EQ
    MOVE R1, 0x00
    JUMPR spi_write_command, 12, EQ
    MOVE R1, 0x1f
    JUMPR spi_write_data, 13, EQ
    MOVE R1, 0x61
    JUMPR spi_write_command, 14, EQ
    MOVE R1, 0x03
    JUMPR spi_write_data, 15, EQ
    MOVE R1, 0x20
    JUMPR spi_write_data, 16, EQ
    MOVE R1, 0x01
    JUMPR spi_write_data, 17, EQ
    MOVE R1, 0xe0
    JUMPR spi_write_data, 18, EQ
    MOVE R1, 0x15
    JUMPR spi_write_command, 19, EQ
    MOVE R1, 0x00
    JUMPR spi_write_data, 20, EQ
    MOVE R1, 0x60
    JUMPR spi_write_command, 21, EQ
    MOVE R1, 0x22
    JUMPR spi_write_data, 22, EQ
    MOVE R1, 0x50
    JUMPR spi_write_command, 23, EQ
    MOVE R1, 0x10
    JUMPR spi_write_data, 24, EQ
    MOVE R1, 0x07
    JUMPR spi_write_data, 25, EQ
    MOVE R0, 0
    MOVE R3, display_frame
display_frame:
    JUMPR epd_wait_busy, 0, EQ
    MOVE R1, 0x13
    JUMPR spi_write_command, 1, EQ
    MOVE R0, 0
    MOVE R3, write_frame_one
write_frame_one:
    MOVE R1, 0xFF
    JUMPR spi_write_data, 24000, LT
    MOVE R0, 0
    MOVE R3, write_frame_two
write_frame_two:
    MOVE R1, 0xFF
    JUMPR spi_write_data, 24000, LT
    MOVE R0, 0
    MOVE R3, epd_off
epd_off:
    MOVE R1, 0x12
    JUMPR spi_write_command, 0, EQ
    JUMPR epd_wait_busy, 1, EQ
end:
    HALT

/* Use bit banging to write a byte over SPI */
spi_write_byte: STAGE_INC 1
    AND R2, R1, 0x80
    SUB R2, R2, 0x80
    /* JUMP checks if the ALU is equal to 0 */
    JUMP write_one, EQ
    MOSI_LOW
    JUMP write_finish
write_one:
    MOSI_HIGH
write_finish:
    SCK_HIGH
    spi_delay
    SCK_LOW
    LSH R1, R1, 1
    JUMPS spi_write_byte, 8, LT
    STAGE_RST
    CS_HIGH
    JUMP R3

spi_write_command: ADD R0, R0, 1
    DC_LOW
    CS_LOW
    JUMP spi_write_byte

spi_write_data: ADD R0, R0, 1
    DC_HIGH
    CS_LOW
    JUMP spi_write_byte

epd_wait_busy: ADD R0, R0, 1
    /* Reading from GPIO pin always reads it in to R0
     * so store increment progress in R2 temporarily */
    MOVE R2, R0
epd_loop:
    READ_BUSY
    JUMPR epd_loop, 1, EQ
    MOVE R0, R2
    JUMP R3
