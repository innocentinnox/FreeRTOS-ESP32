#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "lcd.h"
#include "ds18b20.h"

static const char *TAG = "APP_MAIN";

typedef struct {
    bool valid;
    float temperature;
} temp_reading_t;

static QueueHandle_t s_temp_queue = NULL;

/**
 * @brief Temperature Sensor Task (Producer)
 * Periodically initiates DS18B20 conversion, non-blockingly waits 750ms
 * with vTaskDelay (yielding CPU to other tasks), reads the temperature,
 * and pushes the reading into a FreeRTOS queue.
 */
static void temp_sensor_task(void *pvParameters)
{
    temp_reading_t reading;

    while (1) {
        if (ds18b20_start_conversion(DS18B20_GPIO) == ESP_OK) {
            /* 750ms conversion delay: non-blocking, yields CPU to other tasks */
            vTaskDelay(pdMS_TO_TICKS(750));

            if (ds18b20_read_temp(DS18B20_GPIO, &reading.temperature) == ESP_OK) {
                reading.valid = true;
                ESP_LOGI(TAG, "Temperature: %.2f C", reading.temperature);
            } else {
                reading.valid = false;
                ESP_LOGW(TAG, "Failed to read scratchpad");
            }
        } else {
            reading.valid = false;
            ESP_LOGW(TAG, "DS18B20 sensor not detected on GPIO %d", DS18B20_GPIO);
        }

        if (s_temp_queue != NULL) {
            xQueueOverwrite(s_temp_queue, &reading);
        }

        /* Wait remainder of 2-second sampling period non-blockingly */
        vTaskDelay(pdMS_TO_TICKS(1250));
    }
}

/**
 * @brief LCD Display Task (Consumer)
 * Blocks until a new temperature update arrives via the queue,
 * then updates the LCD without wasting CPU cycles.
 */
static void lcd_display_task(void *pvParameters)
{
    temp_reading_t reading;
    char buffer[32];

    lcd_set_cursor(0, 0);
    lcd_puts("DS18B20 Monitor ");

    while (1) {
        if (xQueueReceive(s_temp_queue, &reading, portMAX_DELAY) == pdTRUE) {
            lcd_set_cursor(1, 0);
            if (reading.valid) {
                /* Exactly 16 characters for the 16x2 LCD */
                /* "Temp: " (6) + %5.2f (5) + ' ' (1) + '\xDF' (1) + 'C' (1) + "  " (2) = 16 */
                snprintf(buffer, sizeof(buffer), "Temp: %5.2f \xDF" "C  ", reading.temperature);
                lcd_puts(buffer);
            } else {
                lcd_puts("Sensor Error    ");
            }
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing DS18B20 + LCD System...");

    /* 1. Initialize LCD (4-bit mode through the I2C backpack) */
    lcd_init();
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_puts("System Starting ");
    lcd_set_cursor(1, 0);
    lcd_puts("Initializing... ");

    /* 2. Initialize DS18B20 1-Wire pin */
    ds18b20_init(DS18B20_GPIO);
    ds18b20_diagnose(DS18B20_GPIO); /* TEMPORARY: remove once the sensor reads correctly */

    /* 3. Create FreeRTOS Queue for temperature updates */
    s_temp_queue = xQueueCreate(1, sizeof(temp_reading_t));
    if (s_temp_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create FreeRTOS Queue");
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
    lcd_clear();

    /* 4. Start concurrent FreeRTOS tasks */
    xTaskCreate(temp_sensor_task, "temp_task", 4096, NULL, 5, NULL);
    xTaskCreate(lcd_display_task, "lcd_task",  4096, NULL, 4, NULL);
}
