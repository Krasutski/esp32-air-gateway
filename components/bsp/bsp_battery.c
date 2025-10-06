#include "bsp_battery.h"
#include "bsp_board.h"

#include <math.h>

#include <driver/gpio.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_adc/adc_oneshot.h>

/* -------------------------------------------------------------------------- */

static adc_oneshot_unit_handle_t _p_adc1;
static adc_cali_handle_t _p_battery_calibration;
static adc_cali_handle_t _p_solar_calibration;
static bool _battery_calibration_ok = false;
static bool _solar_calibration_ok = false;

/* -------------------------------------------------------------------------- */

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED == 1

static bool _create_calibration(adc_channel_t chan, adc_cali_handle_t *out) {
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .chan = chan,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    return adc_cali_create_scheme_curve_fitting(&cali_cfg, out) == ESP_OK;
}

#endif  // ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED == 1
/* -------------------------------------------------------------------------- */

static uint32_t _read_voltage_mv(adc_channel_t chan, adc_cali_handle_t cal, bool is_cal_ok, uint32_t divider_ratio) {
    int raw = 0;

    if (adc_oneshot_read(_p_adc1, chan, &raw) != ESP_OK) {
        return 0;
    }

    int mv = raw;
    if ((is_cal_ok == true) && (adc_cali_raw_to_voltage(cal, raw, &mv) != ESP_OK)) {
        return 0;
    } else {
        mv = (int)roundf(((float)raw / 4095.0f) * 3600.0f);
    }
    const float CORRECTION = 1.02f;
    float millivolts_f = (float)mv * CORRECTION;
    return (uint32_t)lroundf(millivolts_f * (float)divider_ratio);
}

/* -------------------------------------------------------------------------- */

void bsp_battery_init(void) {

    const gpio_config_t ADC_IN_CFG = {
        .pin_bit_mask = (1ULL << BSP_PIN_ADC_BAT) | (1ULL << BSP_PIN_ADC_SOLAR),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&ADC_IN_CFG));

    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_DIGI_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &_p_adc1));

    ///

    adc_oneshot_chan_cfg_t ch_cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(_p_adc1, ADC_CHANNEL_BAT, &ch_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(_p_adc1, ADC_CHANNEL_SOLAR, &ch_cfg));

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED == 1
    _battery_calibration_ok = _create_calibration(ADC_CHANNEL_BAT, &_p_battery_calibration);
    _solar_calibration_ok = _create_calibration(ADC_CHANNEL_SOLAR, &_p_solar_calibration);
#endif  // ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED == 1
}

/* -------------------------------------------------------------------------- */

uint32_t bsp_battery_read_voltage_mv(void) {
    return _read_voltage_mv(ADC_CHANNEL_BAT, _p_battery_calibration, _battery_calibration_ok, ADC_BAT_DIVIDER_RATIO);
}

/* -------------------------------------------------------------------------- */

size_t bsp_battery_get_capacity(void) {

    static const int MAX_PERCENTAGE = 100;
    static const int MAX_VOLTAGE_MV = 4200;
    static const int MIN_VOLTAGE_MV = 3300;

    uint32_t voltage_mv = bsp_battery_read_voltage_mv();

    if (voltage_mv <= MIN_VOLTAGE_MV) {
        return 0;
    }

    if (voltage_mv >= MAX_VOLTAGE_MV) {
        return MAX_PERCENTAGE;
    }

    static const float COEFFICIENT = 100.0f / (MAX_VOLTAGE_MV - MIN_VOLTAGE_MV);
    float capacity_f = ((float)voltage_mv - (float)MIN_VOLTAGE_MV) * COEFFICIENT;
    size_t capacity = (size_t)capacity_f;
    return capacity;
}

/* -------------------------------------------------------------------------- */

uint32_t bsp_solar_battery_read_voltage_mv(void) {
    return _read_voltage_mv(ADC_CHANNEL_SOLAR, _p_solar_calibration, _solar_calibration_ok, ADC_SOLAR_DIVIDER_RATIO);
}

/* -------------------------------------------------------------------------- */
