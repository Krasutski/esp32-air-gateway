#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define MODEM_CONNECT_BIT  BIT0
#define MODEM_GOT_DATA_BIT BIT2

EventGroupHandle_t bsp_modem_eventgroup(void);
esp_modem_dce_t *air_gateway_get_modem_dce(void);
esp_netif_t *air_gateway_get_modem_netif(void);
void bsp_modem_setup(void);
void bsp_modem_deinit(void);
void bsp_modem_power_up_por(void);
void bsp_modem_disable(void);

#ifdef __cplusplus
}
#endif
