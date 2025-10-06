#pragma once

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stddef.h>
#include <stdint.h>

void bsp_battery_init(void);
uint32_t bsp_battery_read_voltage_mv(void);
uint32_t bsp_solar_battery_read_voltage_mv(void);
size_t bsp_battery_get_capacity(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
