#include <stdio.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

// 7-segment pins

#define SEG_A 4
#define SEG_B 16
#define SEG_C 18
#define SEG_D 19
#define SEG_E 21
#define SEG_F 22
#define SEG_G 23


// Common Cathode
//
// Bit 0 = A
// Bit 1 = B
// Bit 2 = C
// Bit 3 = D
// Bit 4 = E
// Bit 5 = F
// Bit 6 = G

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


void display_digit(uint8_t digit)
{
    uint8_t pattern = digit_patterns[digit];

    gpio_set_level(SEG_A, (pattern >> 0) & 0x01);
    gpio_set_level(SEG_B, (pattern >> 1) & 0x01);
    gpio_set_level(SEG_C, (pattern >> 2) & 0x01);
    gpio_set_level(SEG_D, (pattern >> 3) & 0x01);
    gpio_set_level(SEG_E, (pattern >> 4) & 0x01);
    gpio_set_level(SEG_F, (pattern >> 5) & 0x01);
    gpio_set_level(SEG_G, (pattern >> 6) & 0x01);
}


void seven_segment_task(void *pvParameters)
{
    while (1)
    {
        for (int digit = 0; digit <= 9; digit++)
        {
            display_digit(digit);

            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}


void app_main(void)
{
    // Configure all segment GPIOs as outputs

    gpio_set_direction(SEG_A, GPIO_MODE_OUTPUT);
    gpio_set_direction(SEG_B, GPIO_MODE_OUTPUT);
    gpio_set_direction(SEG_C, GPIO_MODE_OUTPUT);
    gpio_set_direction(SEG_D, GPIO_MODE_OUTPUT);
    gpio_set_direction(SEG_E, GPIO_MODE_OUTPUT);
    gpio_set_direction(SEG_F, GPIO_MODE_OUTPUT);
    gpio_set_direction(SEG_G, GPIO_MODE_OUTPUT);


    // Create FreeRTOS task

    xTaskCreate(
        seven_segment_task,
        "seven_segment_task",
        2048,
        NULL,
        1,
        NULL
    );
}