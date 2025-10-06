#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_modem_api.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>

#include "bsp_board.h"
#include "bsp_modem.h"
#include "sdkconfig.h"

/* -------------------------------------------------------------------------- */

static const char *TAG = "gw_modem.c";

static EventGroupHandle_t _event_group = NULL;
static const int USB_DISCONNECTED_BIT =
    BIT3;  // Used only with USB DTE but we define it unconditionally, to avoid too many #ifdefs in the code

static esp_modem_dce_t *_dce;
static esp_netif_t *_esp_modem_netif;

#define CHECK_USB_DISCONNECTION(_event_group)

/* -------------------------------------------------------------------------- */

static void _gpio_init(void) {
    gpio_reset_pin(BSP_PIN_MODEM_FLIGHT);
    gpio_set_direction(BSP_PIN_MODEM_FLIGHT, GPIO_MODE_OUTPUT);
    gpio_set_level(BSP_PIN_MODEM_FLIGHT, 1);

    gpio_set_direction(BSP_PIN_MODEM_DTR, GPIO_MODE_OUTPUT);
    gpio_set_level(BSP_PIN_MODEM_DTR, 0);

    gpio_reset_pin(BSP_PIN_BLUE_LED);
    gpio_set_direction(BSP_PIN_BLUE_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(BSP_PIN_BLUE_LED, 0);

    gpio_reset_pin(BSP_PIN_MODEM_PWR_KEY);
    gpio_set_direction(BSP_PIN_MODEM_PWR_KEY, GPIO_MODE_OUTPUT);
    gpio_set_level(BSP_PIN_MODEM_PWR_KEY, 0);

    gpio_set_direction(BSP_PIN_MODEM_STATUS, GPIO_MODE_INPUT);
}

/* -------------------------------------------------------------------------- */

static void _power_on_button_press(bool is_on) {
    gpio_set_level(BSP_PIN_MODEM_PWR_KEY, 0);
    gpio_set_level(BSP_PIN_BLUE_LED, 0);
    vTaskDelay(pdMS_TO_TICKS(500));

    gpio_set_level(BSP_PIN_MODEM_PWR_KEY, 1);
    gpio_set_level(BSP_PIN_BLUE_LED, 1);
    vTaskDelay(is_on == true ? pdMS_TO_TICKS(500) : pdMS_TO_TICKS(3000));

    gpio_set_level(BSP_PIN_MODEM_PWR_KEY, 0);
    gpio_set_level(BSP_PIN_BLUE_LED, 0);
    vTaskDelay(pdMS_TO_TICKS(500));
}

/* -------------------------------------------------------------------------- */

static void _on_ppp_changed(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    ESP_LOGI(TAG, "PPP state changed event %d", (int)event_id);
    if (event_id == NETIF_PPP_ERRORUSER) {
        esp_netif_t *netif = event_data;
        ESP_LOGI(TAG, "User interrupted event from netif:%p", netif);
    }
}

/* -------------------------------------------------------------------------- */

static void _on_ip_event(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "IP event! %d", (int)event_id);
    if (event_id == IP_EVENT_PPP_GOT_IP) {
        esp_netif_dns_info_t dns_info;

        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_netif_t *netif = event->esp_netif;

        ESP_LOGI(TAG, "Modem Connect to PPP Server");
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        ESP_LOGI(TAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
        esp_netif_get_dns_info(netif, 0, &dns_info);
        ESP_LOGI(TAG, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        esp_netif_get_dns_info(netif, 1, &dns_info);
        ESP_LOGI(TAG, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        xEventGroupSetBits(_event_group, MODEM_CONNECT_BIT);

        ESP_LOGI(TAG, "GOT ip event!!!");
    } else if (event_id == IP_EVENT_PPP_LOST_IP) {
        ESP_LOGI(TAG, "Modem Disconnect from PPP Server");
    } else if (event_id == IP_EVENT_GOT_IP6) {
        ESP_LOGI(TAG, "GOT IPv6 event!");

        ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
        ESP_LOGI(TAG, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
    }
}

/* -------------------------------------------------------------------------- */

EventGroupHandle_t bsp_modem_eventgroup(void) {
    return _event_group;
}

/* -------------------------------------------------------------------------- */

esp_modem_dce_t *air_gateway_get_modem_dce(void) {
    return _dce;
}

/* -------------------------------------------------------------------------- */

esp_netif_t *air_gateway_get_modem_netif(void) {
    return _esp_modem_netif;
}

/* -------------------------------------------------------------------------- */

void bsp_modem_setup(void) {
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &_on_ip_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &_on_ppp_changed, NULL));

    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(CONFIG_GATEWAY_MODEM_PPP_APN);
    esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
    _esp_modem_netif = esp_netif_new(&netif_ppp_config);
    assert(_esp_modem_netif);

    _event_group = xEventGroupCreate();

    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();

    dte_config.uart_config.port_num = CONFIG_GATEWAY_MODEM_UART_NUM;
    dte_config.uart_config.tx_io_num = BSP_PIN_MODEM_UART_TXD;
    dte_config.uart_config.rx_io_num = BSP_PIN_MODEM_UART_RXD;
    dte_config.uart_config.rts_io_num = BSP_PIN_MODEM_UART_RTS;
    dte_config.uart_config.cts_io_num = BSP_PIN_MODEM_UART_CTS;
    dte_config.uart_config.baud_rate = 115200;

    dte_config.uart_config.flow_control = ESP_MODEM_FLOW_CONTROL_NONE;
    dte_config.uart_config.rx_buffer_size = CONFIG_GATEWAY_MODEM_UART_RX_BUFFER_SIZE;
    dte_config.uart_config.tx_buffer_size = CONFIG_GATEWAY_MODEM_UART_TX_BUFFER_SIZE;
    dte_config.uart_config.event_queue_size = CONFIG_GATEWAY_MODEM_UART_EVENT_QUEUE_SIZE;
    dte_config.task_stack_size = CONFIG_GATEWAY_MODEM_UART_EVENT_TASK_STACK_SIZE;
    dte_config.task_priority = CONFIG_GATEWAY_MODEM_UART_EVENT_TASK_PRIORITY;
    dte_config.dte_buffer_size = CONFIG_GATEWAY_MODEM_UART_RX_BUFFER_SIZE / 2;

    ESP_LOGI(TAG, "Initializing esp_modem for the SIM7600 module...");
    _dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM7600, &dte_config, &dce_config, _esp_modem_netif);
    assert(_dce);

    xEventGroupClearBits(_event_group, MODEM_CONNECT_BIT | MODEM_GOT_DATA_BIT | USB_DISCONNECTED_BIT);

    /* Run the modem demo app */
#if GATEWAY_NEED_SIM_PIN == 1
    // check if PIN needed
    bool pin_ok = false;
    if (esp_modem_read_pin(_dce, &pin_ok) == ESP_OK && pin_ok == false) {
        if (esp_modem_set_pin(_dce, GATEWAY_SIM_PIN) == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            abort();
        }
    }
#endif

    esp_err_t err = esp_modem_set_baud(_dce, 3000000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set baud rate: %d", err);
    } else {
        ESP_ERROR_CHECK(uart_set_baudrate(dte_config.uart_config.port_num, 3000000));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));

    int rssi = 0;
    int ber = 0;
    err = esp_modem_get_signal_quality(_dce, &rssi, &ber);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_modem_get_signal_quality failed with %d %s", err, esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "Signal quality: rssi=%d, ber=%d", rssi, ber);

    err = esp_modem_set_mode(_dce, ESP_MODEM_MODE_DATA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_modem_set_mode(ESP_MODEM_MODE_DATA) failed with %d", err);
        return;
    }
}

/* -------------------------------------------------------------------------- */

void bsp_modem_deinit(void) {
    esp_err_t err = esp_modem_set_mode(_dce, ESP_MODEM_MODE_COMMAND);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_modem_set_mode(ESP_MODEM_MODE_COMMAND) failed with %d", err);
        return;
    }

#if 0
    char imsi[32];
    err = esp_modem_get_imsi(_dce, imsi);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_modem_get_imsi failed with %d", err);
        return;
    }
    ESP_LOGI(TAG, "IMSI=%s", imsi);
#endif

    esp_modem_destroy(_dce);
    esp_netif_destroy(_esp_modem_netif);
}

/* -------------------------------------------------------------------------- */

void bsp_modem_power_up_por(void) {
    _gpio_init();

    bool is_modem_on = gpio_get_level(BSP_PIN_MODEM_STATUS) == 1;
    if (is_modem_on == true) {
        ESP_LOGW(TAG, "Turning off modem...");
        _power_on_button_press(false);

        while (gpio_get_level(BSP_PIN_MODEM_STATUS) == 1) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "Turning on modem...");
    _power_on_button_press(true);

    while (gpio_get_level(BSP_PIN_MODEM_STATUS) == 0) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    vTaskDelay(pdMS_TO_TICKS(17000));
    ESP_LOGI(TAG, "Modem is powered up and ready");
}

/* -------------------------------------------------------------------------- */

void bsp_modem_disable(void) {
    _gpio_init();
}

/* -------------------------------------------------------------------------- */
