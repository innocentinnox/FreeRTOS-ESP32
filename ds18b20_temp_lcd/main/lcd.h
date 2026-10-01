#ifndef LCD_H
#define LCD_H

#include <stdint.h>
#include "driver/gpio.h"

/* ---------------- I2C bus (PCF8574 backpack, matching lcd_i2c) ---------------- */
#define LCD_I2C_SDA      GPIO_NUM_8
#define LCD_I2C_SCL      GPIO_NUM_9
#define LCD_I2C_FREQ_HZ  100000
#define LCD_I2C_ADDR     0x27        /* PCF8574 default (0x3F on PCF8574A) */

/* ---------------- PCF8574 bits (P4..P7 carry D4..D7) ---------------- */
#define PCF_RS   0x01                /* Register Select: 0 = command, 1 = data */
#define PCF_RW   0x02                /* always 0: write only                   */
#define PCF_EN   0x04                /* Enable: high-to-low pulse latches data */
#define PCF_BL   0x08                /* backlight on                           */

/* ---------------- LCD command codes ---------------- */
#define CMD_CLEAR_DISPLAY       0x01
#define CMD_RETURN_HOME         0x02
#define CMD_ENTRY_INC_CURSOR    0x06
#define CMD_DISPLAY_ON_NO_CUR   0x0C
#define CMD_DISPLAY_ON_BLINK    0x0F
#define CMD_FUNC_4BIT_2LINE     0x28
#define CMD_CURSOR_LINE1        0x80
#define CMD_CURSOR_LINE2        0xC0

/* Public API */
void lcd_init(void);
void lcd_command(uint8_t cmd);
void lcd_data(uint8_t ch);
void lcd_clear(void);
void lcd_puts(const char *s);
void lcd_set_cursor(uint8_t row, uint8_t col);

#endif /* LCD_H */
