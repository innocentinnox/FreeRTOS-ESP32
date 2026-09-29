/* 16x2 LCD in 4-bit PARALLEL mode — no I2C backpack.
 * Follows lecture 05 (slides 34, 35, 37, 39): the ESP32 drives RS, EN and
 * DB4..DB7 directly through GPIO. R/W is tied to GND (write only).
 *
 * Wiring (ESP32-S3-DevKitC-1):
 *   LCD VSS -> GND        LCD VDD -> 5V       LCD V0 -> pot wiper (contrast)
 *   LCD RS  -> GPIO 4     LCD RW  -> GND      LCD E  -> GPIO 5
 *   LCD D4  -> GPIO 6     LCD D5  -> GPIO 7   LCD D6 -> GPIO 15   LCD D7 -> GPIO 16
 *   LCD A   -> 5V (backlight +)   LCD K -> GND
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"

/* ---------------- LCD pins (slide 34) ---------------- */
#define LCD_RS   GPIO_NUM_4      /* Register Select: 0 = command, 1 = data */
#define LCD_EN   GPIO_NUM_5      /* Enable: high-to-low pulse latches data */
#define LCD_D4   GPIO_NUM_6
#define LCD_D5   GPIO_NUM_7
#define LCD_D6   GPIO_NUM_15
#define LCD_D7   GPIO_NUM_16

/* ---------------- LCD command codes (slide 37) -------------- */
#define CMD_CLEAR_DISPLAY       0x01
#define CMD_RETURN_HOME         0x02
#define CMD_ENTRY_INC_CURSOR    0x06
#define CMD_DISPLAY_ON_NO_CUR   0x0C
#define CMD_DISPLAY_ON_BLINK    0x0F
#define CMD_FUNC_4BIT_2LINE     0x28
#define CMD_CURSOR_LINE1        0x80
#define CMD_CURSOR_LINE2        0xC0

/* ================= Layer 1: configure GPIOs (slides 6-8) ================= */
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
static void lcd_write_nibble(uint8_t nibble)          /* uses bits 3..0 */
{
    gpio_set_level(LCD_D4, (nibble >> 0) & 0x01);     /* same trick as slide 19 */
    gpio_set_level(LCD_D5, (nibble >> 1) & 0x01);
    gpio_set_level(LCD_D6, (nibble >> 2) & 0x01);
    gpio_set_level(LCD_D7, (nibble >> 3) & 0x01);

    gpio_set_level(LCD_EN, 1);                        /* EN high ...          */
    esp_rom_delay_us(1);
    gpio_set_level(LCD_EN, 0);                        /* ... to low: latched  */
    esp_rom_delay_us(50);
}

static void lcd_write_byte(uint8_t byte, uint8_t rs)
{
    gpio_set_level(LCD_RS, rs);                       /* pick the register    */
    lcd_write_nibble(byte >> 4);                      /* high nibble first    */
    lcd_write_nibble(byte & 0x0F);                    /* then low nibble      */
}

/* ================= Layer 3: RS selects the register (slide 35) ================= */
void lcd_command(uint8_t cmd) { lcd_write_byte(cmd, 0); }
void lcd_data(uint8_t ch)     { lcd_write_byte(ch, 1);  }

/* ================= Layer 4: helpers ================= */
void lcd_clear(void)
{
    lcd_command(CMD_CLEAR_DISPLAY);
    vTaskDelay(pdMS_TO_TICKS(2));
}

void lcd_puts(const char *s)
{
    while (*s) lcd_data((uint8_t)*s++);
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

/* ================= Application ================= */
void app_main(void)
{
    lcd_init();

    lcd_command(CMD_CURSOR_LINE1);
    lcd_puts("Hello Wokwi");

    lcd_command(CMD_CURSOR_LINE2);
    lcd_puts("Year: 2026");
}