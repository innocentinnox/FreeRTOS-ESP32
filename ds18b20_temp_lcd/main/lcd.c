#include "lcd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"

/* ================= Layer 1: configure GPIOs ================= */
static void lcd_gpio_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LCD_RS) | (1ULL << LCD_EN) |
                        (1ULL << LCD_D4) | (1ULL << LCD_D5) |
                        (1ULL << LCD_D6) | (1ULL << LCD_D7),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
}

/* ================= Layer 2: put 4 bits on the bus, pulse EN ================= */
static void lcd_write_nibble(uint8_t nibble)
{
    gpio_set_level(LCD_D4, (nibble >> 0) & 0x01);
    gpio_set_level(LCD_D5, (nibble >> 1) & 0x01);
    gpio_set_level(LCD_D6, (nibble >> 2) & 0x01);
    gpio_set_level(LCD_D7, (nibble >> 3) & 0x01);

    gpio_set_level(LCD_EN, 1);
    esp_rom_delay_us(1);
    gpio_set_level(LCD_EN, 0);
    esp_rom_delay_us(50);
}

static void lcd_write_byte(uint8_t byte, uint8_t rs)
{
    gpio_set_level(LCD_RS, rs);
    lcd_write_nibble(byte >> 4); // 00110110
    lcd_write_nibble(byte & 0x0F);
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
    lcd_gpio_init();

    /* HD44780 power-on reset: three 0x3 nibbles, then 0x2 = 4-bit mode */
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(LCD_RS, 0);
    lcd_write_nibble(0x3);  vTaskDelay(pdMS_TO_TICKS(5));
    lcd_write_nibble(0x3);  esp_rom_delay_us(150);
    lcd_write_nibble(0x3);  esp_rom_delay_us(150);
    lcd_write_nibble(0x2);

    lcd_command(CMD_FUNC_4BIT_2LINE);
    lcd_command(CMD_DISPLAY_ON_NO_CUR);
    lcd_command(CMD_ENTRY_INC_CURSOR);
    lcd_clear();
}
