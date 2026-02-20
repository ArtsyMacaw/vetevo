#include "ulp_riscv.h"
#include "ulp_riscv_gpio.h"
#include "ulp_riscv_utils.h"

#define HIGH        1
#define LOW         0

/* Global time variables to track the current time from the main CPU */
volatile uint32_t hours = 0;
volatile uint32_t minutes = 0;

/* Global debug variables to track the state of the ULP program from the main CPU */
volatile uint32_t wakeups = 0;
volatile uint32_t launched = 0;
volatile uint32_t wait_flag = 0;
volatile uint32_t bytes_written = 0;
volatile uint32_t epd_started = 0;
volatile uint32_t epd_cleared = 0;
volatile uint32_t epd_reset_done = 0;

static uint8_t font_24x48[96 * 144 / 8] = {0}; // Placeholder for font data

static void spi_write_byte(uint8_t byte)
{
    for (int i = 0; i < 8; i++)
    {
        ulp_riscv_gpio_output_level(GPIO_NUM_15, (byte & 0x80) ? HIGH : LOW);
        byte <<= 1;
        ulp_riscv_gpio_output_level(GPIO_NUM_17, HIGH);
        ulp_riscv_gpio_output_level(GPIO_NUM_17, LOW);
    }
    bytes_written++;
    ulp_riscv_gpio_output_level(GPIO_NUM_3, HIGH);
}

static void spi_write_command(uint8_t command)
{
    ulp_riscv_gpio_output_level(GPIO_NUM_4, LOW);
    ulp_riscv_gpio_output_level(GPIO_NUM_3, LOW);
    spi_write_byte(command);
}

static void spi_write_data(uint8_t data)
{
    ulp_riscv_gpio_output_level(GPIO_NUM_4, HIGH);
    ulp_riscv_gpio_output_level(GPIO_NUM_3, LOW);
    spi_write_byte(data);
}

static void epd_reset(void)
{
    ulp_riscv_gpio_output_level(GPIO_NUM_9, HIGH);
    ulp_riscv_delay_cycles(20 * ULP_RISCV_CYCLES_PER_MS);
    ulp_riscv_gpio_output_level(GPIO_NUM_9, LOW);
    ulp_riscv_delay_cycles(20 * ULP_RISCV_CYCLES_PER_MS);
    ulp_riscv_gpio_output_level(GPIO_NUM_9, HIGH);
    ulp_riscv_delay_cycles(20 * ULP_RISCV_CYCLES_PER_MS);
    epd_reset_done = 1;
}

static void epd_wait_until_idle(void)
{
    wait_flag = 1;
    while (ulp_riscv_gpio_get_level(GPIO_NUM_18) == HIGH)
    {
        ulp_riscv_delay_cycles(10 * ULP_RISCV_CYCLES_PER_MS);
    }
    wait_flag = 0;
}

static void epd_init(void)
{
    epd_reset();

    spi_write_command(0x06);
    spi_write_data(0x17);
    spi_write_data(0x17);
    spi_write_data(0x27);
    spi_write_data(0x17);

    spi_write_command(0x01);
    spi_write_data(0x07);
    spi_write_data(0x17);
    spi_write_data(0x3f);
    spi_write_data(0x3f);

    spi_write_command(0x04);
    epd_wait_until_idle();

    spi_write_command(0x00);
    spi_write_data(0x1f);

    spi_write_command(0x61);
    spi_write_data(0x03);
    spi_write_data(0x20);
    spi_write_data(0x01);
    spi_write_data(0xe0);

    spi_write_command(0x15);
    spi_write_data(0x00);

    spi_write_command(0x60);
    spi_write_data(0x22);

    spi_write_command(0x50);
    spi_write_data(0x10);
    spi_write_data(0x07);
    epd_started = 1;
}

static void epd_clear(void)
{
    epd_wait_until_idle();
    spi_write_command(0x13);
    for (int i = 0; i < (800 * (480 / 8)); i++)
    {
        spi_write_data(0x00);
    }
    spi_write_command(0x12);
    epd_wait_until_idle();
    epd_cleared = 1;
}

int main(void)
{
    /* First wakeup is just to initialize variables and let them be set by the main CPU */
    wakeups++;
    if (wakeups == 1)
    {
        return 0;
    }
    launched = 1;

    epd_init();
    epd_clear();
}
