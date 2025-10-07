#include "esp_event.h"
#include "esp_log.h"
#include "esp_modem_api.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "sdkconfig.h"
#include <string.h>

#include "dhcpserver/dhcpserver.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "lwip/lwip_napt.h"
#include "nvs_flash.h"

#include "app_blinking.h"
#include "bsp_battery.h"
#include "bsp_led.h"
#include "bsp_modem.h"

static const char *TAG = "air_gateway.c";

/* -------------------------------------------------------------------------- */

static esp_err_t set_dhcps_dns(esp_netif_t *netif, uint32_t addr) {
    esp_netif_dns_info_t dns;
    dns.ip.u_addr.ip4.addr = addr;
    dns.ip.type = IPADDR_TYPE_V4;
    dhcps_offer_t dhcps_dns_value = OFFER_DNS;
    ESP_ERROR_CHECK(esp_netif_dhcps_option(netif,
                                           ESP_NETIF_OP_SET,
                                           ESP_NETIF_DOMAIN_NAME_SERVER,
                                           &dhcps_dns_value,
                                           sizeof(dhcps_dns_value)));
    ESP_ERROR_CHECK(esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns));
    return ESP_OK;
}

/* -------------------------------------------------------------------------- */

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
        app_blinking_station_connected();
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
        app_blinking_station_disconnected();
    }
}

/* -------------------------------------------------------------------------- */

void wifi_init_softap(void) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = CONFIG_AIR_GATEWAY_AP_WIFI_SSID,
            .ssid_len = strlen(CONFIG_AIR_GATEWAY_AP_WIFI_SSID),
            .channel = CONFIG_AIR_GATEWAY_AP_WIFI_CHANNEL,
            .password = CONFIG_AIR_GATEWAY_AP_WIFI_PASS,
            .max_connection = CONFIG_AIR_GATEWAY_AP_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };
    if (strlen(CONFIG_AIR_GATEWAY_AP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG,
             "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             CONFIG_AIR_GATEWAY_AP_WIFI_SSID,
             CONFIG_AIR_GATEWAY_AP_WIFI_PASS,
             CONFIG_AIR_GATEWAY_AP_WIFI_CHANNEL);
}

/* -------------------------------------------------------------------------- */

static void _periodic_system_status_log(void) {

    ESP_LOGI(TAG,
             "Battery: %dmV (%d%%), Solar %dmV",
             bsp_battery_read_voltage_mv(),
             bsp_battery_get_capacity(),
             bsp_solar_battery_read_voltage_mv());
}

/* -------------------------------------------------------------------------- */

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    bsp_led_init();
    bsp_battery_init();

    bsp_modem_power_up_por();

    _periodic_system_status_log();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    bsp_modem_setup();

    ESP_LOGI(TAG, "Waiting for IP address...");
    xEventGroupWaitBits(bsp_modem_eventgroup(), MODEM_CONNECT_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    assert(ap_netif);
    esp_netif_dns_info_t dns;

    esp_netif_t *modem_netif;
    modem_netif = air_gateway_get_modem_netif();
    assert(modem_netif);
    ESP_ERROR_CHECK(esp_netif_get_dns_info(modem_netif, ESP_NETIF_DNS_MAIN, &dns));
    set_dhcps_dns(ap_netif, dns.ip.u_addr.ip4.addr);

    wifi_init_softap();
    ip_napt_enable(_g_esp_netif_soft_ap_ip.ip.addr, 1);

    ESP_LOGW(TAG, "Hotspot should now be functional...");
    app_blinking_init();

    uint32_t periodic_log_ts_ticks = 0;
    while (1) {
        if ((xTaskGetTickCount() - periodic_log_ts_ticks) > pdMS_TO_TICKS(30000)) {
            _periodic_system_status_log();
            periodic_log_ts_ticks = xTaskGetTickCount();
        }
        vTaskDelay(100);
    }

    bsp_modem_deinit();
}

/* -------------------------------------------------------------------------- */
