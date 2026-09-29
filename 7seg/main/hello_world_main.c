#include <stdio.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"


// ============================================================
// 7-SEGMENT PINS
// ============================================================

#define SEG_A   GPIO_NUM_4
#define SEG_B   GPIO_NUM_16
#define SEG_C   GPIO_NUM_18
#define SEG_D   GPIO_NUM_19
#define SEG_E   GPIO_NUM_21
#define SEG_F   GPIO_NUM_22
#define SEG_G   GPIO_NUM_23
#define SEG_DP  GPIO_NUM_25


// ============================================================
// 7-SEGMENT DIGIT PATTERNS
//
// Common Cathode
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
// 1 = ON
// 0 = OFF
// ============================================================

const uint8_t digit_patterns[10] =
{
    0b00111111,  // 0
    0b00000110,  // 1
    0b01011011,  // 2
    0b01001111,  // 3
    0b01100110,  // 4
    0b01101101,  // 5
    0b01111101,  // 6
    0b00000111,  // 7
    0b01111111,  // 8
    0b01101111   // 9
};


// ============================================================
// DISPLAY ONE DIGIT
// ============================================================

void display_digit(uint8_t digit, uint8_t decimal_point)
{
    // Get the pattern for the digit
    uint8_t pattern = digit_patterns[digit];

    // Turn ON DP if requested
    if (decimal_point)
    {
        pattern |= (1 << 7);
    }

    // Set segment outputs
    gpio_set_level(
        SEG_A,
        (pattern >> 0) & 0x01
    );

    gpio_set_level(
        SEG_B,
        (pattern >> 1) & 0x01
    );

    gpio_set_level(
        SEG_C,
        (pattern >> 2) & 0x01
    );

    gpio_set_level(
        SEG_D,
        (pattern >> 3) & 0x01
    );

    gpio_set_level(
        SEG_E,
        (pattern >> 4) & 0x01
    );

    gpio_set_level(
        SEG_F,
        (pattern >> 5) & 0x01
    );

    gpio_set_level(
        SEG_G,
        (pattern >> 6) & 0x01
    );

    // Decimal point
    gpio_set_level(
        SEG_DP,
        (pattern >> 7) & 0x01
    );
}


// ============================================================
// FREERTOS TASK
// ============================================================

void seven_segment_task(void *pvParameters)
{
    while (1)
    {
        // ------------------------------------
        // Display 7
        // DP OFF
        // ------------------------------------

        display_digit(7, 0);

        vTaskDelay(
            pdMS_TO_TICKS(1000)
        );


        // ------------------------------------
        // Display 0
        // DP OFF
        // ------------------------------------

        display_digit(0, 0);

        vTaskDelay(
            pdMS_TO_TICKS(1000)
        );


        // ------------------------------------
        // Display 2
        // DP OFF
        // ------------------------------------

        display_digit(2, 0);

        vTaskDelay(
            pdMS_TO_TICKS(1000)
        );


        // ------------------------------------
        // Display 2.
        // DP ON
        // ------------------------------------

        display_digit(2, 1);

        vTaskDelay(
            pdMS_TO_TICKS(1000)
        );
    }
}


// ============================================================
// MAIN APPLICATION
// ============================================================

void app_main(void)
{
    // Configure segment GPIOs as outputs

    gpio_set_direction(
        SEG_A,
        GPIO_MODE_OUTPUT
    );

    gpio_set_direction(
        SEG_B,
        GPIO_MODE_OUTPUT
    );

    gpio_set_direction(
        SEG_C,
        GPIO_MODE_OUTPUT
    );

    gpio_set_direction(
        SEG_D,
        GPIO_MODE_OUTPUT
    );

    gpio_set_direction(
        SEG_E,
        GPIO_MODE_OUTPUT
    );

    gpio_set_direction(
        SEG_F,
        GPIO_MODE_OUTPUT
    );

    gpio_set_direction(
        SEG_G,
        GPIO_MODE_OUTPUT
    );

    gpio_set_direction(
        SEG_DP,
        GPIO_MODE_OUTPUT
    );


    // Start FreeRTOS task

    xTaskCreate(
        seven_segment_task,
        "seven_segment_task",
        2048,
        NULL,
        1,
        NULL
    );
}