#pragma once

#include <stdbool.h>

void bsp_led_init(void);

bool bsp_led_is_on(void);
void bsp_led_on(void);
void bsp_led_off(void);
void bsp_led_toggle(void);
