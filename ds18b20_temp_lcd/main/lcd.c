#include "lcd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_rom_sys.h"

static i2c_master_dev_handle_t lcd_dev;

/* ================= Layer 1: configure the I2C bus ================= */
static void lcd_i2c_init(void)
{
    i2c_master_bus_config_t bus_conf = {
        .i2c_port          = I2C_NUM_0,
        .sda_io_num        = LCD_I2C_SDA,
        .scl_io_num        = LCD_I2C_SCL,
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_conf, &bus));

    i2c_device_config_t dev_conf = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = LCD_I2C_ADDR,
        .scl_speed_hz    = LCD_I2C_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_conf, &lcd_dev));
}

/* ================= Layer 2: put 4 bits on the bus, pulse EN ================= */
static void lcd_write_nibble(uint8_t nibble, uint8_t rs)
{
    uint8_t pins = (uint8_t)(nibble << 4) | PCF_BL | (rs ? PCF_RS : 0);

    /* The PCF8574 updates its pins after every byte it receives, so two
     * bytes in one transaction give the EN pulse. */
    uint8_t buf[2] = {
        pins | PCF_EN,  /* EN high ...         */
        pins,           /* ... to low: latched */
    };
    ESP_ERROR_CHECK(i2c_master_transmit(lcd_dev, buf, sizeof(buf), 100));
    esp_rom_delay_us(50);
}

static void lcd_write_byte(uint8_t byte, uint8_t rs)
{
    lcd_write_nibble(byte >> 4, rs);
    lcd_write_nibble(byte & 0x0F, rs);
}

/* ================= Layer 3: Command & Data functions ================= */
void lcd_command(uint8_t cmd)
{
    lcd_write_byte(cmd, 0);
}

void lcd_data(uint8_t ch)
{
    lcd_write_byte(ch, 1);
}

/* ================= Layer 4: High-level helpers ================= */
void lcd_clear(void)
{
    lcd_command(CMD_CLEAR_DISPLAY);
    vTaskDelay(pdMS_TO_TICKS(2));
}

void lcd_puts(const char *s)
{
    while (*s) {
        lcd_data((uint8_t)*s++);
    }
}

void lcd_set_cursor(uint8_t row, uint8_t col)
{
    uint8_t addr = (row == 0) ? CMD_CURSOR_LINE1 : CMD_CURSOR_LINE2;
    lcd_command(addr + col);
}

void lcd_init(void)
{
    lcd_i2c_init();

    /* HD44780 power-on reset: three 0x3 nibbles, then 0x2 = 4-bit mode */
    vTaskDelay(pdMS_TO_TICKS(50));
    lcd_write_nibble(0x3, 0);  vTaskDelay(pdMS_TO_TICKS(5));
    lcd_write_nibble(0x3, 0);  esp_rom_delay_us(150);
    lcd_write_nibble(0x3, 0);  esp_rom_delay_us(150);
    lcd_write_nibble(0x2, 0);

    lcd_command(CMD_FUNC_4BIT_2LINE);
    lcd_command(CMD_DISPLAY_ON_NO_CUR);
    lcd_command(CMD_ENTRY_INC_CURSOR);
    lcd_clear();
}
