/* Fridge billing challenge — FreeRTOS software timers on ESP32 (ESP-IDF)
 *
 * green  LED = fridge ON        red LED = fridge OFF
 * orange LED = bill over 8,000  -> paybill task clears 3/4 of the bill
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define GREEN_LED   GPIO_NUM_4
#define RED_LED     GPIO_NUM_16
#define ORANGE_LED  GPIO_NUM_17

#define RATE_PER_HALF_SEC  500     /* UGX charged per half-second ON   */
#define SAVING_THRESHOLD   6000    /* above this: short ON, long OFF   */
#define PAY_THRESHOLD      8000    /* above this: orange LED + paybill */

static const char *TAG = "FRIDGE";

/* ---- shared state ---------------------------------------------------- */
static int  bill = 0;                 /* protected by bill_mutex          */
static bool fridge_on = false;        /* only touched by timer callbacks  */
static bool awaiting_payment = false;

static SemaphoreHandle_t bill_mutex;
static TimerHandle_t     meter_timer;   /* auto-reload, every 500 ms      */
static TimerHandle_t     cycle_timer;   /* one-shot, re-armed each cycle  */
static TaskHandle_t      paybill_handle;

/* ---- helpers --------------------------------------------------------- */
static int read_bill(void)
{
    xSemaphoreTake(bill_mutex, portMAX_DELAY);
    int b = bill;
    xSemaphoreGive(bill_mutex);
    return b;
}

/* Charge one half-second of fridge time. Called at the START of every
 * half-second the fridge is on. */
static void charge_half_second(void)
{
    xSemaphoreTake(bill_mutex, portMAX_DELAY);
    bill += RATE_PER_HALF_SEC;
    int b = bill;
    xSemaphoreGive(bill_mutex);
    ESP_LOGI(TAG, "  +%d  -> bill UGX %d", RATE_PER_HALF_SEC, b);

    if (b > PAY_THRESHOLD && !awaiting_payment) {
        awaiting_payment = true;
        gpio_set_level(ORANGE_LED, 1);
        ESP_LOGW(TAG, "Bill over %d! Orange LED on, notifying paybill", PAY_THRESHOLD);
        xTaskNotifyGive(paybill_handle);        /* wake the paybill task */
    }
}

/* ---- timer callbacks (run in the FreeRTOS timer service task) --------- */

/* Fires every 500 ms. Charges only if the fridge is on AND the current
 * ON period has not ended at this exact tick. */
static void meter_cb(TimerHandle_t t)
{
    if (!fridge_on) return;
    if (xTimerGetExpiryTime(cycle_timer) <= xTaskGetTickCount()) return;
    charge_half_second();
}

/* Fires once per ON or OFF period. Toggles the fridge, picks the next
 * period from the bill, and re-arms itself. */
static void cycle_cb(TimerHandle_t t)
{
    bool saving = read_bill() >= SAVING_THRESHOLD;

    fridge_on = !fridge_on;
    gpio_set_level(GREEN_LED, fridge_on);
    gpio_set_level(RED_LED,  !fridge_on);

    int ms;
    if (fridge_on) {
        ms = saving ? 500 : 1000;
        xTimerReset(meter_timer, 0);   /* align meter ticks with this ON period */
        charge_half_second();          /* first half-second starts now */
    } else {
        ms = saving ? 2000 : 500;
    }

    ESP_LOGI(TAG, "Fridge %s for %d ms  [%s mode]",
             fridge_on ? "ON " : "OFF", ms, saving ? "saving" : "normal");

    xTimerChangePeriod(cycle_timer, pdMS_TO_TICKS(ms), 0);   /* also restarts it */
}

/* ---- paybill task ---------------------------------------------------- */
static void paybill_task(void *arg)
{
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);   /* block until notified */

        xSemaphoreTake(bill_mutex, portMAX_DELAY);
        int paid = bill * 3 / 4;
        bill -= paid;
        int balance = bill;
        xSemaphoreGive(bill_mutex);

        ESP_LOGW(TAG, "PAYBILL: paid UGX %d, total balance UGX %d", paid, balance);
        gpio_set_level(ORANGE_LED, 0);
        awaiting_payment = false;
    }
}

/* ---- setup ----------------------------------------------------------- */
static void led_init(gpio_num_t pin)
{
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0);
}

void app_main(void)
{
    led_init(GREEN_LED);
    led_init(RED_LED);
    led_init(ORANGE_LED);

    bill_mutex = xSemaphoreCreateMutex();

    xTaskCreate(paybill_task, "paybill", 2048, NULL, 2, &paybill_handle);

    meter_timer = xTimerCreate("meter", pdMS_TO_TICKS(500), pdTRUE,  NULL, meter_cb);
    cycle_timer = xTimerCreate("cycle", pdMS_TO_TICKS(10),  pdFALSE, NULL, cycle_cb);

    if (!bill_mutex || !meter_timer || !cycle_timer) {
        ESP_LOGE(TAG, "Failed to create RTOS objects");
        return;
    }

    xTimerStart(meter_timer, 0);
    xTimerStart(cycle_timer, 0);   /* first expiry turns the fridge ON */
    /* app_main returns here — timers and paybill keep running */
}