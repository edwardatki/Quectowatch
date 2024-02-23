#include <stdio.h>
#include <esp_lcd_panel_ssd1681.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "esp_pm.h"
#include "nimble-nordic-uart.h"

// #include "ssd1681_waveshare_1in54_lut.h"

static const char *TAG = "example";

int bluetooth_connected = 0;

#include "eink.h"

void echoTask(void *parameter) {
    ESP_LOGI(TAG, "Start echoTask");

    static char mbuf[CONFIG_NORDIC_UART_MAX_LINE_LENGTH + 1];
    for (;;) {
        size_t item_size;
        if (nordic_uart_rx_buf_handle) {
            const char *item = (char *)xRingbufferReceive(nordic_uart_rx_buf_handle, &item_size, portMAX_DELAY);

            if (item) {
                int i;
                for (i = 0; i < item_size; ++i) {
                if (item[i] >= 'a' && item[i] <= 'z')
                    mbuf[i] = item[i] - 0x20;
                else
                    mbuf[i] = item[i];
                }
                mbuf[item_size] = '\0';

                // nordic_uart_sendln(mbuf);
                // puts(mbuf);
                ESP_LOGI(TAG, "RECEIVE: %s", mbuf);
                vRingbufferReturnItem(nordic_uart_rx_buf_handle, (void *)item);
            }
        } else {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

  vTaskDelete(NULL);
}

void nordic_callback(enum nordic_uart_callback_type callback_type) {
    if (callback_type == NORDIC_UART_CONNECTED) bluetooth_connected = 1;
    else bluetooth_connected = 0;
}

void app_main() {
    // esp_pm_config_t pm_config = {
    //     .max_freq_mhz = 160,
    //     .min_freq_mhz = 80,
    //     .light_sleep_enable = false
    // };
    // ESP_ERROR_CHECK(esp_pm_configure(&pm_config));

    ESP_LOGI(TAG, "Start Nordic UART");
    nordic_uart_start("Nordic UART", &nordic_callback);
    xTaskCreate(echoTask, "echoTask", 5000, NULL, 1, NULL);

    ESP_LOGI(TAG, "Start LCD");
    xTaskCreate(lcdTask, "lcdTask", 5000, NULL, 1, NULL);

    return;
}