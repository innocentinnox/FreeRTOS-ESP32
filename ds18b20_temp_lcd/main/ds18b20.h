#ifndef DS18B20_H
#define DS18B20_H

#include "driver/gpio.h"
#include "esp_err.h"

#define DS18B20_GPIO  GPIO_NUM_17

/* DS18B20 1-Wire Commands */
#define DS18B20_CMD_READ_ROM        0x33
#define DS18B20_CMD_MATCH_ROM       0x55
#define DS18B20_CMD_SKIP_ROM        0xCC
#define DS18B20_CMD_CONVERT_TEMP    0x44
#define DS18B20_CMD_READ_SCRATCHPAD 0xBE

/**
 * @brief Initialize 1-Wire pin in open-drain mode with pullup.
 */
esp_err_t ds18b20_init(gpio_num_t pin);

/**
 * @brief Trigger temperature conversion without waiting.
 *        Returns immediately so that FreeRTOS tasks can delay non-blockingly.
 */
esp_err_t ds18b20_start_conversion(gpio_num_t pin);

/**
 * @brief Read temperature from scratchpad after conversion completes.
 */
esp_err_t ds18b20_read_temp(gpio_num_t pin, float *temperature);

/**
 * @brief TEMPORARY: log 1-Wire bus timing measurements for debugging.
 */
void ds18b20_diagnose(gpio_num_t pin);

#endif /* DS18B20_H */
