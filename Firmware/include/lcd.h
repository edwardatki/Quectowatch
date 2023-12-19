#ifndef _LCD_H_
#define _LCD_H_

#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_types.h"
#include "lvgl.h"

#define LCD_HOST  SPI2_HOST

#define LCD_PIXEL_CLOCK_HZ (20 * 1000 * 1000)
#define LCD_SCLK_PIN 2
#define LCD_MOSI_PIN 3
#define LCD_MISO_PIN 0
#define LCD_DC_PIN 7
#define LCD_RST_PIN 6
#define LCD_CS_PIN 10
#define LCD_BL_PIN 5

#define LCD_H_RES 240
#define LCD_V_RES 240

#define LCD_CMD_BITS 8
#define LCD_PARAM_BITS 8

#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_DUTY_RES LEDC_TIMER_13_BIT
#define LEDC_DUTY 4096 // 50%
#define LEDC_FREQUENCY 500

#define LVGL_TICK_PERIOD_MS 10
#define LVGL_TASK_MAX_DELAY_MS 1000
#define LVGL_TASK_MIN_DELAY_MS 1
#define LVGL_TASK_STACK_SIZE 4096
#define LVGL_TASK_PRIORITY 2

lv_disp_t *disp;

static SemaphoreHandle_t lvgl_mux = NULL;

// Tell lvgl that disaply refresh is finished
static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
  lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
  lv_disp_flush_ready(disp_driver);
  return false;
}

// Copy buffer content to a specific area of the LCD
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_data) {
  esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
  esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_data);
}

// Called when driver parameters are updated, only used for changing rotation
static void lvgl_port_update_callback(lv_disp_drv_t *drv) {
  esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;

  switch (drv->rotated) {
    case LV_DISP_ROT_NONE:
      // Rotate LCD
      esp_lcd_panel_swap_xy(panel_handle, false);
      esp_lcd_panel_mirror(panel_handle, true, false);
      break;
    case LV_DISP_ROT_90:
      // Rotate LCD LCD
      esp_lcd_panel_swap_xy(panel_handle, true);
      esp_lcd_panel_mirror(panel_handle, true, true);
      break;
    case LV_DISP_ROT_180:
      // Rotate LCD LCD
      esp_lcd_panel_swap_xy(panel_handle, false);
      esp_lcd_panel_mirror(panel_handle, false, true);
      break;
    case LV_DISP_ROT_270:
      // Rotate LCD LCD
      esp_lcd_panel_swap_xy(panel_handle, true);
      esp_lcd_panel_mirror(panel_handle, false, false);
      break;
  }
}

// Update lvgl's internal tick system
static void increase_lvgl_tick(void *arg) {
  lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

bool lvgl_lock(int timeout_ms) {
  // If timeout_ms is -1 will block indefinetly waiting for mutex
  const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : (timeout_ms / portTICK_PERIOD_MS);
  ESP_LOGD(_TAG, "waiting for lvgl mutex");
  return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

void lvgl_unlock(void) {
  ESP_LOGD(_TAG, "releasing lvgl mutex");
  xSemaphoreGiveRecursive(lvgl_mux);
}

// Main LVGL task
static void lvgl_port_task(void *arg) {
  ESP_LOGI(_TAG, "Starting LVGL task");
  uint32_t task_delay_ms = 1000;
  while (1) {
    // Lock the mutex due as LVGL APIs are not thread-safe
    if (lvgl_lock(-1)) {
      // Do update
      task_delay_ms = lv_timer_handler();
      // Release the mutex
      lvgl_unlock();
    }
    // Clamp delay time
    if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS) task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
    else if (task_delay_ms < LVGL_TASK_MIN_DELAY_MS) task_delay_ms = LVGL_TASK_MIN_DELAY_MS;

    // Delay as long as LVGL requested
    ESP_LOGI(_TAG, "lvgl_port_task done, waiting %lu", task_delay_ms);
    vTaskDelay(task_delay_ms / portTICK_PERIOD_MS);
  }
}

void init_lcd() {
  static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
  static lv_disp_drv_t disp_drv;      // contains callback functions

  ESP_LOGI(_TAG, "Initialize backlight PWM");
  gpio_sleep_sel_dis(LCD_BL_PIN);
  // Prepare and then apply the LEDC PWM timer configuration
  ledc_timer_config_t ledc_timer = {
      .speed_mode       = LEDC_MODE,
      .duty_resolution  = LEDC_DUTY_RES,
      .timer_num        = LEDC_TIMER,
      .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 4 kHz
      .clk_cfg          = LEDC_USE_RC_FAST_CLK  // Use RTC clock in order to keep working in light sleep
  };
  ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

  // Prepare and then apply the LEDC PWM channel configuration
  ledc_channel_config_t ledc_channel = {
      .speed_mode     = LEDC_MODE,
      .channel        = LEDC_CHANNEL,
      .timer_sel      = LEDC_TIMER,
      .intr_type      = LEDC_INTR_DISABLE,
      .gpio_num       = LCD_BL_PIN,
      .duty           = 0, // Set duty to 0%
      .hpoint         = 0
  };
  ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
  

  ESP_LOGI(_TAG, "Initialize SPI bus");
  spi_bus_config_t buscfg = {
      .sclk_io_num = LCD_SCLK_PIN,
      .mosi_io_num = LCD_MOSI_PIN,
      .miso_io_num = LCD_MISO_PIN,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = LCD_H_RES * 80 * sizeof(uint16_t),
  };
  // Initialize LCD
  ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

  ESP_LOGI(_TAG, "Install panel IO");
  esp_lcd_panel_io_handle_t io_handle = NULL;
  esp_lcd_panel_io_spi_config_t io_config = {
      .dc_gpio_num = LCD_DC_PIN,
      .cs_gpio_num = LCD_CS_PIN,
      .pclk_hz = LCD_PIXEL_CLOCK_HZ,
      .lcd_cmd_bits = LCD_CMD_BITS,
      .lcd_param_bits = LCD_PARAM_BITS,
      .spi_mode = 0,
      .trans_queue_depth = 10,
      .on_color_trans_done = notify_lvgl_flush_ready,
      .user_ctx = &disp_drv,
  };
  // Attach the LCD to the SPI bus
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

  esp_lcd_panel_handle_t panel_handle = NULL;
  esp_lcd_panel_dev_config_t panel_config = {
      .reset_gpio_num = LCD_RST_PIN,
      .color_space = LCD_RGB_ENDIAN_BGR,
      .bits_per_pixel = 16,
  };
  ESP_LOGI(_TAG, "Install ST7789 panel driver");
  ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

  // LCD reset and init
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));

  // LCD on
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
  
  // Set backlight brightness
  ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY));
  ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

  // ---

  ESP_LOGI(_TAG, "Initialize LVGL library");
  lv_init();

  // alloc draw buffers used by LVGL
  // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 LCD sized
  lv_color_t *buf1 = heap_caps_malloc(LCD_H_RES * 20 * sizeof(lv_color_t), MALLOC_CAP_DMA);
  assert(buf1);
  lv_color_t *buf2 = heap_caps_malloc(LCD_H_RES * 20 * sizeof(lv_color_t), MALLOC_CAP_DMA);
  assert(buf2);
  // initialize LVGL draw buffers
  lv_disp_draw_buf_init(&disp_buf, buf1, buf2, LCD_H_RES * 20);

  ESP_LOGI(_TAG, "Register LCD driver to LVGL");
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = LCD_H_RES;
  disp_drv.ver_res = LCD_V_RES;
  disp_drv.flush_cb = lvgl_flush_cb;
  disp_drv.drv_update_cb = lvgl_port_update_callback;
  disp_drv.draw_buf = &disp_buf;
  disp_drv.user_data = panel_handle;
  disp = lv_disp_drv_register(&disp_drv);

  ESP_LOGI(_TAG, "Install LVGL tick timer");
  // Tick interface for LVGL (using esp_timer to generate periodic event)
  const esp_timer_create_args_t lvgl_tick_timer_args = {
      .callback = &increase_lvgl_tick,
      .name = "lvgl_tick"
  };
  esp_timer_handle_t lvgl_tick_timer = NULL;
  ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

  // Create lvgl mutex
  lvgl_mux = xSemaphoreCreateRecursiveMutex();
  assert(lvgl_mux);

  // ESP_LOGI(_TAG, "Create LVGL task");
  // xTaskCreate(lvgl_port_task, "LVGL", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL);
}

#endif