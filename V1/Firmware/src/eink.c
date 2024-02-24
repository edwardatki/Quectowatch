#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_ssd1681.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"

#include "ssd1681_waveshare_1in54_lut.h"

static const char *TAG = "EINK";

#define LCD_HOST                SPI2_HOST

#define LCD_PIXEL_CLOCK_HZ      (20 * 1000 * 1000)
#define PIN_NUM_MOSI            2
#define PIN_NUM_SCLK            3
#define PIN_NUM_MISO            (-1)
#define PIN_NUM_EPD_CS          4
#define PIN_NUM_EPD_DC          5
#define PIN_NUM_EPD_RST         6
#define PIN_NUM_EPD_BUSY        7

#define LCD_H_RES               200
#define LCD_V_RES               200

#define LCD_CMD_BITS            8
#define LCD_PARAM_BITS          8

#define LVGL_TICK_PERIOD_MS     50

static SemaphoreHandle_t panel_refreshing_sem = NULL;

extern void lvgl_ui_init(lv_disp_t *disp);
extern int lvgl_ui_update();

int refresh_mode;

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

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map) {
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    int len_bits = (abs(offsetx1 - offsetx2) + 1) * (abs(offsety1 - offsety2) + 1);

    DMA_ATTR static uint8_t converted_buffer_black[LCD_V_RES * LCD_H_RES / 8];
    memset(converted_buffer_black, 0x00, len_bits / 8);
    
    for (int i = 0; i < len_bits; i++) {
        converted_buffer_black[i / 8] |= (((lv_color_brightness(color_map[i])) < 251) << (7 - (i % 8)));
    }

    esp_lcd_panel_disp_on_off(panel_handle, true);
    ESP_ERROR_CHECK(epaper_panel_set_bitmap_color(panel_handle, SSD1681_EPAPER_BITMAP_BLACK));
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, converted_buffer_black));
    if (refresh_mode == 1) ESP_ERROR_CHECK(epaper_panel_set_custom_lut(panel_handle, SSD1681_WAVESHARE_1IN54_V2_LUT_FAST_REFRESH, sizeof(SSD1681_WAVESHARE_1IN54_V2_LUT_FAST_REFRESH)));
    if (refresh_mode == 2) ESP_ERROR_CHECK(epaper_panel_set_custom_lut(panel_handle, SSD1681_WAVESHARE_1IN54_V2_LUT_FULL_REFRESH, sizeof(SSD1681_WAVESHARE_1IN54_V2_LUT_FULL_REFRESH)));
    ESP_ERROR_CHECK(epaper_panel_refresh_screen(panel_handle));
    esp_lcd_panel_disp_on_off(panel_handle, false);
}

static void lvgl_wait_cb(struct _lv_disp_drv_t *disp_drv) {
    xSemaphoreTake(panel_refreshing_sem, portMAX_DELAY);
}

// Rotate display and touch, called when driver parameters are updated
static void lvgl_port_update_callback(lv_disp_drv_t *drv) {
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

static void increase_lvgl_tick(void *arg) {
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

void lcd_task(void* parameter) {
    static lv_disp_draw_buf_t disp_buf; // Contains internal graphic buffer(s) called draw buffer(s)
    static lv_disp_drv_t disp_drv;      // Contains callback functions

    panel_refreshing_sem = xSemaphoreCreateBinary();
    xSemaphoreGive(panel_refreshing_sem);

    ESP_LOGI(TAG, "Initialize SPI bus");
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_SCLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_V_RES / 8,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_NUM_EPD_DC,
        .cs_gpio_num = PIN_NUM_EPD_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = NULL,
    };
    
    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t) LCD_HOST, &io_config, &io_handle));
    esp_lcd_panel_handle_t panel_handle = NULL;

    // Create esp_lcd panel
    esp_lcd_ssd1681_config_t epaper_ssd1681_config = {
        .busy_gpio_num = PIN_NUM_EPD_BUSY,
        .non_copy_mode = true,
    };
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_EPD_RST,
        .flags.reset_active_high = false,
        .vendor_config = &epaper_ssd1681_config
    };
    gpio_install_isr_service(0);
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1681(io_handle, &panel_config, &panel_handle));

    // Reset the display
    ESP_LOGI(TAG, "Resetting display...");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Initialize panel
    ESP_LOGI(TAG, "Initializing display...");
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    // Register the refresh done callback
    epaper_panel_callbacks_t cbs = {
        .on_epaper_refresh_done = epaper_flush_ready_callback
    };
    epaper_panel_register_event_callbacks(panel_handle, &cbs, &disp_drv);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Turn on display
    ESP_LOGI(TAG, "Turning display on...");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Configure the screen
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, false));
    esp_lcd_panel_invert_color(panel_handle, false);
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // Initialize LVGL
    ESP_LOGI(TAG, "Initialize LVGL");
    lv_init();

    // Allocate LVGL draw buffers
    lv_color_t *buf1 = (lv_color_t*)malloc(LCD_H_RES * LCD_V_RES * sizeof(lv_color_t));
    assert(buf1);
    // lv_color_t *buf2 = (lv_color_t*)malloc(LCD_H_RES * LCD_V_RES * sizeof(lv_color_t));
    // assert(buf2);

    // Initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf1, NULL, LCD_V_RES * LCD_H_RES);
    
    // Initialize LVGL display driver
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_H_RES;
    disp_drv.ver_res = LCD_V_RES;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.wait_cb = lvgl_wait_cb;
    disp_drv.drv_update_cb = lvgl_port_update_callback;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    // NOTE: The ssd1681 e-paper is monochrome and 1 byte represents 8 pixels
    // so full_refresh is MANDATORY because we cannot set position to bitmap at pixel level
    disp_drv.full_refresh = true;
    
    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    // Init LVGL tick (using esp_timer to generate periodic event)
    ESP_LOGI(TAG, "Install LVGL tick timer");
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    ESP_LOGI(TAG, "Initialize UI");
    lvgl_ui_init(disp);

    while (1) {
        if (lvgl_ui_update()) {
            ESP_LOGI(TAG, "LVGL update");
            lv_timer_handler();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}