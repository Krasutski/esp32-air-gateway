#include "app_blinking.h"

#include "bsp_led.h"

#include <stdbool.h>
#include <stdint.h>

#include <esp_err.h>
#include <esp_timer.h>

/* -------------------------------------------------------------------------- */

static esp_timer_handle_t _blink_timer;
static uint32_t _sta_count;

#define LED_ON_TIME_US             (100000ULL)
#define LED_PERIOD_CONNECTED_US    (5000000ULL)
#define LED_PERIOD_DISCONNECTED_US (1000000ULL)

/* -------------------------------------------------------------------------- */

static uint64_t _current_period_us(void) {
    return (_sta_count > 0) ? LED_PERIOD_CONNECTED_US : LED_PERIOD_DISCONNECTED_US;
}

/* -------------------------------------------------------------------------- */

static void _blink_timer_cb(void *arg) {
    (void)arg;

    if (bsp_led_is_on() == true) {
        bsp_led_off();
        ESP_ERROR_CHECK(esp_timer_start_once(_blink_timer, _current_period_us()));
        return;
    }

    bsp_led_on();
    ESP_ERROR_CHECK(esp_timer_start_once(_blink_timer, LED_ON_TIME_US));
}

/* -------------------------------------------------------------------------- */

static void _reschedule_when_off(void) {
    if (_blink_timer == NULL) {
        return;
    }

    if (bsp_led_is_on() == true) {
        return;
    }

    if (esp_timer_is_active(_blink_timer)) {
        ESP_ERROR_CHECK(esp_timer_stop(_blink_timer));
    }
    ESP_ERROR_CHECK(esp_timer_start_once(_blink_timer, _current_period_us()));
}

/* -------------------------------------------------------------------------- */

void app_blinking_init(void) {

    bsp_led_init();

    const esp_timer_create_args_t TIMER_ARGS = {
        .callback = &_blink_timer_cb,
        .name = "led_blink",
    };
    ESP_ERROR_CHECK(esp_timer_create(&TIMER_ARGS, &_blink_timer));

    _sta_count = 0;

    bsp_led_on();
    ESP_ERROR_CHECK(esp_timer_start_once(_blink_timer, LED_ON_TIME_US));
}

/* -------------------------------------------------------------------------- */

void app_blinking_station_connected(void) {
    if (_sta_count < UINT32_MAX) {
        _sta_count++;
    }
    _reschedule_when_off();
}

/* -------------------------------------------------------------------------- */

void app_blinking_station_disconnected(void) {
    if (_sta_count > 0) {
        _sta_count--;
    }
    _reschedule_when_off();
}

/* -------------------------------------------------------------------------- */
