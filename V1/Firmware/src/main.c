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

// Using SPI2 in the example
#define LCD_HOST  SPI2_HOST

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ     (20 * 1000 * 1000)
#define EXAMPLE_PIN_NUM_MOSI           2
#define EXAMPLE_PIN_NUM_SCLK           3
#define EXAMPLE_PIN_NUM_MISO           (-1)   // Unused
#define EXAMPLE_PIN_NUM_EPD_CS         4
#define EXAMPLE_PIN_NUM_EPD_DC         5
#define EXAMPLE_PIN_NUM_EPD_RST        6
#define EXAMPLE_PIN_NUM_EPD_BUSY       7

// The pixel number in horizontal and vertical
#define EXAMPLE_LCD_H_RES              200
#define EXAMPLE_LCD_V_RES              200

// Bit number used to represent command and parameter
#define EXAMPLE_LCD_CMD_BITS           8
#define EXAMPLE_LCD_PARAM_BITS         8

#define EXAMPLE_LVGL_TICK_PERIOD_MS    50

static SemaphoreHandle_t panel_refreshing_sem = NULL;

int bluetooth_connected = 0;

extern void example_lvgl_demo_ui(lv_disp_t *disp);
extern int lvgl_clock_update();

IRAM_ATTR bool epaper_flush_ready_callback(const esp_lcd_panel_handle_t handle, const void *edata, void *user_data) {
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *) user_data;
    lv_disp_flush_ready(disp_driver);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(panel_refreshing_sem, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken == pdTRUE) {
        return true;
    }
    return false;
}

static void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map) {
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    int len_bits = (abs(offsetx1 - offsetx2) + 1) * (abs(offsety1 - offsety2) + 1);

    DMA_ATTR static uint8_t converted_buffer_black[EXAMPLE_LCD_V_RES * EXAMPLE_LCD_H_RES / 8];
    memset(converted_buffer_black, 0x00, len_bits / 8);
    
    for (int i = 0; i < len_bits; i++) {
        converted_buffer_black[i / 8] |= (((lv_color_brightness(color_map[i])) < 251) << (7 - (i % 8)));
    }

    esp_lcd_panel_disp_on_off(panel_handle, true);
    ESP_ERROR_CHECK(epaper_panel_set_bitmap_color(panel_handle, SSD1681_EPAPER_BITMAP_BLACK));
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, converted_buffer_black));
    ESP_ERROR_CHECK(epaper_panel_refresh_screen(panel_handle));
    esp_lcd_panel_disp_on_off(panel_handle, false);
}

static void example_lvgl_wait_cb(struct _lv_disp_drv_t *disp_drv) {
    xSemaphoreTake(panel_refreshing_sem, portMAX_DELAY);
}

/* Rotate display and touch, when rotated screen in LVGL. Called when driver parameters are updated. */
static void example_lvgl_port_update_callback(lv_disp_drv_t *drv) {
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;

    switch (drv->rotated) {
    case LV_DISP_ROT_NONE:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, false);
        break;
    case LV_DISP_ROT_90:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, false, true);
        break;
    case LV_DISP_ROT_180:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, true, true);
        break;
    case LV_DISP_ROT_270:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, false);
        break;
    }
}


static void example_increase_lvgl_tick(void *arg) {
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

void lcdTask(void* parameter) {
    static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
    static lv_disp_drv_t disp_drv;      // contains callback functions

    panel_refreshing_sem = xSemaphoreCreateBinary();
    xSemaphoreGive(panel_refreshing_sem);

    ESP_LOGI(TAG, "Initialize SPI bus");
    spi_bus_config_t buscfg = {
        .sclk_io_num = EXAMPLE_PIN_NUM_SCLK,
        .mosi_io_num = EXAMPLE_PIN_NUM_MOSI,
        .miso_io_num = EXAMPLE_PIN_NUM_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES / 8,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = EXAMPLE_PIN_NUM_EPD_DC,
        .cs_gpio_num = EXAMPLE_PIN_NUM_EPD_CS,
        .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS,
        .lcd_param_bits = EXAMPLE_LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = NULL,
    };
    // --- Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t) LCD_HOST, &io_config, &io_handle));
    esp_lcd_panel_handle_t panel_handle = NULL;

    // --- Create esp_lcd panel
    esp_lcd_ssd1681_config_t epaper_ssd1681_config = {
        .busy_gpio_num = EXAMPLE_PIN_NUM_EPD_BUSY,
        .non_copy_mode = true,
    };
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_EPD_RST,
        .flags.reset_active_high = false,
        .vendor_config = &epaper_ssd1681_config
    };
    gpio_install_isr_service(0);
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1681(io_handle, &panel_config, &panel_handle));

    // --- Reset the display
    ESP_LOGI(TAG, "Resetting e-Paper display...");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    vTaskDelay(100 / portTICK_PERIOD_MS);
    // --- Initialize panel
    ESP_LOGI(TAG, "Initializing e-Paper display...");
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    // --- Register the e-Paper refresh done callback
    epaper_panel_callbacks_t cbs = {
        .on_epaper_refresh_done = epaper_flush_ready_callback
    };
    epaper_panel_register_event_callbacks(panel_handle, &cbs, &disp_drv);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    // --- Turn on display
    ESP_LOGI(TAG, "Turning e-Paper display on...");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    vTaskDelay(100 / portTICK_PERIOD_MS);
    // --- Configurate the screen
    // NOTE: the configurations below are all FALSE by default
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, false));
    esp_lcd_panel_invert_color(panel_handle, false);
    // NOTE: Calling esp_lcd_panel_disp_on_off(panel_handle, true) will reset the LUT to the panel built-in one,
    // custom LUT will not take effect any more after calling esp_lcd_panel_disp_on_off(panel_handle, true)
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // --- Initialize LVGL
    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    // alloc draw buffers used by LVGL
    // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
    // lv_color_t *buf1 = heap_caps_malloc(EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * sizeof(lv_color_t), MALLOC_CAP_DMA);
    // static lv_color_t buf1[EXAMPLE_LCD_V_RES * EXAMPLE_LCD_H_RES * sizeof(lv_color_t) / 2];
    lv_color_t *buf1 = (lv_color_t*)malloc(EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * sizeof(lv_color_t));//heap_caps_malloc(EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * sizeof(lv_color_t) / 2, 0);
    assert(buf1);
    // lv_color_t *buf2 = heap_caps_malloc(EXAMPLE_LCD_H_RES * 200 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    // DMA_ATTR static lv_color_t buf2[EXAMPLE_LCD_V_RES * (EXAMPLE_LCD_H_RES/5) * sizeof(lv_color_t)];
    // assert(buf2);
    // alloc bitmap buffer to draw
    // converted_buffer_black = heap_caps_malloc(EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES / 8, MALLOC_CAP_DMA);
    // converted_buffer_red = heap_caps_malloc(EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES / 8, MALLOC_CAP_DMA);
    // initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf1, NULL, EXAMPLE_LCD_V_RES * EXAMPLE_LCD_H_RES);
    // initialize LVGL display driver
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = EXAMPLE_LCD_H_RES;
    disp_drv.ver_res = EXAMPLE_LCD_V_RES;
    disp_drv.flush_cb = example_lvgl_flush_cb;
    disp_drv.wait_cb = example_lvgl_wait_cb;
    disp_drv.drv_update_cb = example_lvgl_port_update_callback;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    // NOTE: The ssd1681 e-paper is monochrome and 1 byte represents 8 pixels
    // so full_refresh is MANDATORY because we cannot set position to bitmap at pixel level
    disp_drv.full_refresh = true;
    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);
    // init lvgl tick
    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

    ESP_LOGI(TAG, "Display LVGL Meter Widget");
    example_lvgl_demo_ui(disp);

    while (1) {
        if (lvgl_clock_update(bluetooth_connected)) {
            ESP_LOGI(TAG, "LVGL update");
            lv_timer_handler();
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

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

    while (1) {
        ESP_LOGI(TAG, "BEEP BOOP");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}