#ifndef LCD_H
#define LCD_H

#include <stdint.h>
#include "driver/gpio.h"

/* ---------------- LCD pins (matching lcd_parallel) ---------------- */
#define LCD_RS   GPIO_NUM_4      /* Register Select: 0 = command, 1 = data */
#define LCD_EN   GPIO_NUM_5      /* Enable: high-to-low pulse latches data */
#define LCD_D4   GPIO_NUM_6
#define LCD_D5   GPIO_NUM_7
#define LCD_D6   GPIO_NUM_15
#define LCD_D7   GPIO_NUM_16

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
