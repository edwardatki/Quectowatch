#include <stdio.h>
#include <esp_lcd_panel_ssd1681.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_pm.h"
// #include "nimble-nordic-uart.h"
#include "eink.h"
#include "gadgetbridge.h"

static const char *TAG = "MAIN";

void app_main() {
    // esp_pm_config_t pm_config = {
    //     .max_freq_mhz = 160,
    //     .min_freq_mhz = 80,
    //     .light_sleep_enable = false
    // };
    // ESP_ERROR_CHECK(esp_pm_configure(&pm_config));

    xTaskCreate(gadgetbridge_task, "gadgetbridge_task", 5000, NULL, 1, NULL);
    xTaskCreate(lcd_task, "lcd_task", 5000, NULL, 1, NULL);

    return;
}