#include <stdio.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"


// ============================================================
// 74HC595 PINS
// ============================================================

#define SR_DATA     GPIO_NUM_16
#define SR_CLOCK    GPIO_NUM_17
#define SR_LATCH    GPIO_NUM_18


// ============================================================
// 7-SEGMENT DIGIT SELECT PINS
// ============================================================

#define DIG1        GPIO_NUM_12
#define DIG2        GPIO_NUM_13
#define DIG3        GPIO_NUM_14
#define DIG4        GPIO_NUM_15


// ============================================================
// DIGIT PATTERNS
//
// Bit 0 = A
// Bit 1 = B
// Bit 2 = C
// Bit 3 = D
// Bit 4 = E
// Bit 5 = F
// Bit 6 = G
// Bit 7 = DP
//
// Common cathode:
// 1 = segment ON
// 0 = segment OFF
// ============================================================

const uint8_t digit_patterns[10] =
{
    0b00111111,   // 0
    0b00000110,   // 1
    0b01011011,   // 2
    0b01001111,   // 3
    0b01100110,   // 4
    0b01101101,   // 5
    0b01111101,   // 6
    0b00000111,   // 7
    0b01111111,   // 8
    0b01101111    // 9
};


// ============================================================
// SEND 8 BITS TO 74HC595
// ============================================================

void shift_out(uint8_t data)
{
    // Latch LOW while sending
    gpio_set_level(SR_LATCH, 0);

    // Send bits from bit 7 to bit 0
    for (int i = 7; i >= 0; i--)
    {
        // Put current bit on DATA
        gpio_set_level(
            SR_DATA,
            (data >> i) & 0x01
        );

        // Generate clock pulse
        gpio_set_level(SR_CLOCK, 1);
        gpio_set_level(SR_CLOCK, 0);
    }

    // Transfer shifted data to outputs
    gpio_set_level(SR_LATCH, 1);
}


// ============================================================
// TURN ALL FOUR DIGITS OFF
//
// Common cathode:
// LOW  = ON
// HIGH = OFF
// ============================================================

void digits_off(void)
{
    gpio_set_level(DIG1, 1);
    gpio_set_level(DIG2, 1);
    gpio_set_level(DIG3, 1);
    gpio_set_level(DIG4, 1);
}


// ============================================================
// DISPLAY ONE DIGIT
// ============================================================

void show_digit(
    int digit,
    gpio_num_t digit_pin,
    int decimal_point
)
{
    // Prevent ghosting
    digits_off();

    // Get digit pattern
    uint8_t pattern = digit_patterns[digit];

    // Turn DP on if requested
    if (decimal_point)
    {
        pattern |= 0b10000000;
    }

    // Send pattern to 74HC595
    shift_out(pattern);

    // Enable selected digit
    gpio_set_level(digit_pin, 0);

    // Keep digit visible
    vTaskDelay(pdMS_TO_TICKS(2));

    // Disable digit
    gpio_set_level(digit_pin, 1);
}


// ============================================================
// DISPLAY TASK
// ============================================================

void display_task(void *pvParameters)
{
    while (1)
    {
        // First digit = 7
        show_digit(7, DIG1, 0);

        // Second digit = 0
        show_digit(0, DIG2, 0);

        // Third digit = 2
        show_digit(2, DIG3, 0);

        // Fourth digit = 1 with decimal point
        show_digit(1, DIG4, 1);
    }
}


// ============================================================
// MAIN
// ============================================================

void app_main(void)
{
    // 74HC595 control pins
    gpio_set_direction(SR_DATA, GPIO_MODE_OUTPUT);
    gpio_set_direction(SR_CLOCK, GPIO_MODE_OUTPUT);
    gpio_set_direction(SR_LATCH, GPIO_MODE_OUTPUT);

    // Digit select pins
    gpio_set_direction(DIG1, GPIO_MODE_OUTPUT);
    gpio_set_direction(DIG2, GPIO_MODE_OUTPUT);
    gpio_set_direction(DIG3, GPIO_MODE_OUTPUT);
    gpio_set_direction(DIG4, GPIO_MODE_OUTPUT);

    // Initial states
    gpio_set_level(SR_DATA, 0);
    gpio_set_level(SR_CLOCK, 0);
    gpio_set_level(SR_LATCH, 0);

    // Turn displays off initially
    digits_off();

    // Start display task
    xTaskCreate(
        display_task,
        "display_task",
        2048,
        NULL,
        1,
        NULL
    );
}