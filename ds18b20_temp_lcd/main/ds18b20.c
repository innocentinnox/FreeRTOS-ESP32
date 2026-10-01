#include "ds18b20.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define DS18B20_SCRATCHPAD_LEN 9
#define DS18B20_ROM_LEN        8

static const char *TAG = "DS18B20";

/* 64-bit ROM code of the sensor, read once with Read ROM and then used to
 * address it with Match ROM. */
static uint8_t s_rom[DS18B20_ROM_LEN];
static bool s_rom_valid = false;

/* TEMPORARY: write-slot low times, adjustable by the diagnostics */
static uint32_t s_write1_low_us = 6;
static uint32_t s_write0_low_us = 60;

/* Bit slots are timing-critical down to a few microseconds, so interrupts
 * (FreeRTOS tick, etc.) are masked for the duration of each slot. */
static portMUX_TYPE s_onewire_mux = portMUX_INITIALIZER_UNLOCKED;

/* Helper primitives for an open-drain bus:
 * The pin is configured once as GPIO_MODE_INPUT_OUTPUT_OD, so the input stage
 * stays enabled the whole time and each transition is a single register write.
 * - Level 0 drives the bus LOW.
 * - Level 1 releases the bus so the 4.7k pull-up resistor pulls it HIGH.
 * Open-drain never drives HIGH, so the ESP32 can't fight the DS18B20.
 */
static inline void onewire_low(gpio_num_t pin)
{
    gpio_set_level(pin, 0);
}

static inline void onewire_release(gpio_num_t pin)
{
    gpio_set_level(pin, 1);
}

static inline int onewire_read(gpio_num_t pin)
{
    return gpio_get_level(pin);
}

static esp_err_t onewire_reset(gpio_num_t pin)
{
    onewire_low(pin);
    esp_rom_delay_us(480);              /* Master reset pulse: 480us */

    portENTER_CRITICAL(&s_onewire_mux);
    onewire_release(pin);
    esp_rom_delay_us(70);               /* Wait for DS18B20 presence pulse */
    int presence = onewire_read(pin);   /* Sample: 0 = present, 1 = not found */
    portEXIT_CRITICAL(&s_onewire_mux);

    esp_rom_delay_us(410);              /* Complete 480us timeslot */

    return (presence == 0) ? ESP_OK : ESP_ERR_NOT_FOUND;
}

static void onewire_write_bit(gpio_num_t pin, uint8_t bit)
{
    portENTER_CRITICAL(&s_onewire_mux);
    if (bit) {
        /* Write 1: pull low for 6us, then release high for the rest of the 70us slot */
        onewire_low(pin);
        esp_rom_delay_us(s_write1_low_us);
        onewire_release(pin);
        esp_rom_delay_us(70 - s_write1_low_us);
    } else {
        /* Write 0: pull low for 60us, then release high for the rest of the 70us slot */
        onewire_low(pin);
        esp_rom_delay_us(s_write0_low_us);
        onewire_release(pin);
        esp_rom_delay_us(70 - s_write0_low_us);
    }
    portEXIT_CRITICAL(&s_onewire_mux);
}

static uint8_t onewire_read_bit(gpio_num_t pin)
{
    portENTER_CRITICAL(&s_onewire_mux);
    /* Read slot: master pulls low for 3us, then releases */
    onewire_low(pin);
    esp_rom_delay_us(3);
    onewire_release(pin);
    /* Sample bus at ~13us from start of slot (DS18B20 data is only valid for 15us) */
    esp_rom_delay_us(10);
    uint8_t bit = onewire_read(pin) ? 1 : 0;
    portEXIT_CRITICAL(&s_onewire_mux);

    /* Finish the remaining slot time */
    esp_rom_delay_us(53);
    return bit;
}

static void onewire_write_byte(gpio_num_t pin, uint8_t byte)
{
    for (int i = 0; i < 8; i++) {
        onewire_write_bit(pin, (byte >> i) & 0x01);
    }
}

static uint8_t onewire_read_byte(gpio_num_t pin)
{
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) {
        if (onewire_read_bit(pin)) {
            byte |= (1 << i);
        }
    }
    return byte;
}

/* Dallas/Maxim CRC-8 (polynomial x^8 + x^5 + x^4 + 1, LSB first) */
static uint8_t onewire_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        for (int b = 0; b < 8; b++) {
            uint8_t mix = (crc ^ byte) & 0x01;
            crc >>= 1;
            if (mix) {
                crc ^= 0x8C;
            }
            byte >>= 1;
        }
    }
    return crc;
}

/* Read the sensor's ROM code (only valid with a single device on the bus) */
static esp_err_t ds18b20_read_rom(gpio_num_t pin)
{
    if (onewire_reset(pin) != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }
    onewire_write_byte(pin, DS18B20_CMD_READ_ROM);
    for (int i = 0; i < DS18B20_ROM_LEN; i++) {
        s_rom[i] = onewire_read_byte(pin);
    }
    s_rom_valid = (onewire_crc8(s_rom, DS18B20_ROM_LEN - 1) == s_rom[DS18B20_ROM_LEN - 1]);
    return s_rom_valid ? ESP_OK : ESP_ERR_INVALID_CRC;
}

esp_err_t ds18b20_init(gpio_num_t pin)
{
    gpio_set_level(pin, 1); /* Pre-load output latch to 1 so the bus starts released */

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode         = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        return err;
    }
    return ds18b20_read_rom(pin);
}

/* ================= TEMPORARY bus diagnostics ================= */

/* Reset, address the sensor (Match ROM or Skip ROM), Read Scratchpad, log 9 bytes */
static void diag_read_scratchpad(gpio_num_t pin, bool match, const char *label)
{
    uint8_t sp[DS18B20_SCRATCHPAD_LEN];
    esp_err_t err = onewire_reset(pin);
    if (match) {
        onewire_write_byte(pin, DS18B20_CMD_MATCH_ROM);
        for (int i = 0; i < DS18B20_ROM_LEN; i++) {
            onewire_write_byte(pin, s_rom[i]);
        }
    } else {
        onewire_write_byte(pin, DS18B20_CMD_SKIP_ROM);
    }
    onewire_write_byte(pin, DS18B20_CMD_READ_SCRATCHPAD);
    for (int i = 0; i < DS18B20_SCRATCHPAD_LEN; i++) {
        sp[i] = onewire_read_byte(pin);
    }
    ESP_LOGI(TAG, "diag %s: reset=%s crc_%s  %02x %02x %02x %02x %02x %02x %02x %02x %02x",
             label, esp_err_to_name(err),
             (onewire_crc8(sp, 8) == sp[8]) ? "ok" : "BAD",
             sp[0], sp[1], sp[2], sp[3], sp[4], sp[5], sp[6], sp[7], sp[8]);
}

void ds18b20_diagnose(gpio_num_t pin)
{
    ESP_LOGI(TAG, "diag ROM: valid=%d  %02x %02x %02x %02x %02x %02x %02x %02x", s_rom_valid,
             s_rom[0], s_rom[1], s_rom[2], s_rom[3], s_rom[4], s_rom[5], s_rom[6], s_rom[7]);

    diag_read_scratchpad(pin, true,  "M1 match+read");
    diag_read_scratchpad(pin, true,  "M2 match+read");
    diag_read_scratchpad(pin, false, "S1 skip+read ");
    diag_read_scratchpad(pin, false, "S2 skip+read ");

    /* Same as S1 but with Arduino OneWire write-slot timing */
    s_write1_low_us = 10;
    s_write0_low_us = 65;
    diag_read_scratchpad(pin, false, "S3 skip+read (slow writes)");
    s_write1_low_us = 6;
    s_write0_low_us = 60;

    esp_err_t err = ds18b20_start_conversion(pin);
    vTaskDelay(pdMS_TO_TICKS(800));
    ESP_LOGI(TAG, "diag CV: skip+convert=%s, waited 800ms", esp_err_to_name(err));
    diag_read_scratchpad(pin, true,  "M3 match+read");
    diag_read_scratchpad(pin, false, "S4 skip+read ");
}

/* ============================================================== */

esp_err_t ds18b20_start_conversion(gpio_num_t pin)
{
    /* Sensor may have been missing at init: retry the ROM read */
    if (!s_rom_valid && ds18b20_read_rom(pin) != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }
    if (onewire_reset(pin) != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }
    onewire_write_byte(pin, DS18B20_CMD_SKIP_ROM);
    onewire_write_byte(pin, DS18B20_CMD_CONVERT_TEMP);
    return ESP_OK;
}

esp_err_t ds18b20_read_temp(gpio_num_t pin, float *temperature)
{
    if (!temperature) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_rom_valid || onewire_reset(pin) != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }
    onewire_write_byte(pin, DS18B20_CMD_MATCH_ROM);
    for (int i = 0; i < DS18B20_ROM_LEN; i++) {
        onewire_write_byte(pin, s_rom[i]);
    }
    onewire_write_byte(pin, DS18B20_CMD_READ_SCRATCHPAD);

    /* Read the full scratchpad so the CRC (byte 8) can be verified.
     * A sensor that never answers leaves the bus idle-high, i.e. all 0xFF,
     * which would otherwise decode as a plausible-looking -0.06 C. */
    uint8_t scratchpad[DS18B20_SCRATCHPAD_LEN];
    for (int i = 0; i < DS18B20_SCRATCHPAD_LEN; i++) {
        scratchpad[i] = onewire_read_byte(pin);
    }
    if (onewire_crc8(scratchpad, DS18B20_SCRATCHPAD_LEN - 1) != scratchpad[DS18B20_SCRATCHPAD_LEN - 1]) {
        ESP_LOGW(TAG, "Scratchpad CRC mismatch:");
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, scratchpad, DS18B20_SCRATCHPAD_LEN, ESP_LOG_WARN);
        return ESP_ERR_INVALID_CRC;
    }

    int16_t raw = (int16_t)((scratchpad[1] << 8) | scratchpad[0]);
    *temperature = (float)raw * 0.0625f;

    return ESP_OK;
}
