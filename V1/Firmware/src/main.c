#include <stdint.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
#include "esp_pm.h"
#include "nimble-nordic-uart.h"

void echoTask(void *parameter) {
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

                nordic_uart_sendln(mbuf);
                puts(mbuf);
                vRingbufferReturnItem(nordic_uart_rx_buf_handle, (void *)item);
            }
        } else {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

  vTaskDelete(NULL);
}

void app_main(void) {
    esp_pm_config_esp32_t pm_config = {
        .max_freq_mhz = 160, // e.g. 80, 160, 240
        .min_freq_mhz = 80, // e.g. 40
        .light_sleep_enable = false // enable light sleep
    };
    ESP_ERROR_CHECK( esp_pm_configure(&pm_config) );

    nordic_uart_start("Nordic UART", NULL);
    xTaskCreate(echoTask, "echoTask", 5000, NULL, 1, NULL);
}