#include "bsp_led.h"

#include "bsp_board.h"

#include <driver/gpio.h>

/* -------------------------------------------------------------------------- */

static bool _is_blue_led_on = false;

/* -------------------------------------------------------------------------- */

void bsp_led_init(void) {

    gpio_reset_pin(BSP_PIN_BLUE_LED);
    gpio_set_direction(BSP_PIN_BLUE_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(BSP_PIN_BLUE_LED, 0);
    _is_blue_led_on = false;
}

/* -------------------------------------------------------------------------- */

void bsp_led_set(bool is_on) {

    gpio_set_level(BSP_PIN_BLUE_LED, is_on ? 1 : 0);
    _is_blue_led_on = is_on;
}

/* -------------------------------------------------------------------------- */

void bsp_led_on(void) {
    bsp_led_set(true);
}

/* -------------------------------------------------------------------------- */

void bsp_led_off(void) {
    bsp_led_set(false);
}

/* -------------------------------------------------------------------------- */

void bsp_led_toggle(void) {
    int new_level = _is_blue_led_on == false ? 1 : 0;
    gpio_set_level(BSP_PIN_BLUE_LED, new_level);
}

/* -------------------------------------------------------------------------- */

bool bsp_led_is_on(void) {
    return _is_blue_led_on == true;
}

/* -------------------------------------------------------------------------- */
