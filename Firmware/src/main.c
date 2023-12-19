#include <stdint.h>
#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
#include "nimble-nordic-uart.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_pm.h"
#include "cJSON.h"
#include "esp_sntp.h"
#include "notifications.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_types.h"
#include "lvgl.h"

static const char *_TAG = "MAIN";

#define BUTTON_GPIO 9
#define LED_L_GPIO 12
#define LED_R_GPIO 13

#define LCD_HOST  SPI2_HOST

#define LCD_PIXEL_CLOCK_HZ (20 * 1000 * 1000)
#define LCD_SCLK_PIN 2
#define LCD_MOSI_PIN 3
#define LCD_MISO_PIN 0
#define LCD_DC_PIN 7
#define LCD_RST_PIN 6
#define LCD_CS_PIN 10

#define LCD_H_RES 240
#define LCD_V_RES 240

#define LCD_CMD_BITS 8
#define LCD_PARAM_BITS 8

int bluetooth_connected = 0;
int music_playing = 0;

lv_disp_t *disp;

static SemaphoreHandle_t lvgl_mux = NULL;

static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
  lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
  lv_disp_flush_ready(disp_driver);
  return false;
}

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
  esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
  int offsetx1 = area->x1;
  int offsetx2 = area->x2;
  int offsety1 = area->y1;
  int offsety2 = area->y2;
  // copy a buffer's content to a specific area of the display
  esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

/* Rotate display and touch, when rotated screen in LVGL. Called when driver parameters are updated. */
static void lvgl_port_update_callback(lv_disp_drv_t *drv)
{
  esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;

  switch (drv->rotated) {
    case LV_DISP_ROT_NONE:
      // Rotate LCD display
      esp_lcd_panel_swap_xy(panel_handle, false);
      esp_lcd_panel_mirror(panel_handle, true, false);
      break;
    case LV_DISP_ROT_90:
      // Rotate LCD display
      esp_lcd_panel_swap_xy(panel_handle, true);
      esp_lcd_panel_mirror(panel_handle, true, true);
      break;
    case LV_DISP_ROT_180:
      // Rotate LCD display
      esp_lcd_panel_swap_xy(panel_handle, false);
      esp_lcd_panel_mirror(panel_handle, false, true);
      break;
    case LV_DISP_ROT_270:
      // Rotate LCD display
      esp_lcd_panel_swap_xy(panel_handle, true);
      esp_lcd_panel_mirror(panel_handle, false, false);
      break;
  }
}

static void increase_lvgl_tick(void *arg)
{
  /* Tell LVGL how many milliseconds has elapsed */
  lv_tick_inc(2);
}

bool lvgl_lock(int timeout_ms)
{
  // Convert timeout in milliseconds to FreeRTOS ticks
  // If `timeout_ms` is set to -1, the program will block until the condition is met
  const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
  return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

void lvgl_unlock(void)
{
  xSemaphoreGiveRecursive(lvgl_mux);
}

static void lvgl_port_task(void *arg)
{
  ESP_LOGI(_TAG, "Starting LVGL task");
  uint32_t task_delay_ms = 500;
  while (1) {
    // Lock the mutex due to the LVGL APIs are not thread-safe
    if (lvgl_lock(-1)) {
      task_delay_ms = lv_timer_handler();
      // Release the mutex
      lvgl_unlock();
    }
    if (task_delay_ms > 500) {
      task_delay_ms = 500;
    } else if (task_delay_ms < 1) {
      task_delay_ms = 1;
    }
    vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
  }
}

void init_screen() {
  static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
  static lv_disp_drv_t disp_drv;      // contains callback functions

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

  // ---

  ESP_LOGI(_TAG, "Initialize LVGL library");
  lv_init();

  // alloc draw buffers used by LVGL
  // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
  lv_color_t *buf1 = heap_caps_malloc(LCD_H_RES * 20 * sizeof(lv_color_t), MALLOC_CAP_DMA);
  assert(buf1);
  lv_color_t *buf2 = heap_caps_malloc(LCD_H_RES * 20 * sizeof(lv_color_t), MALLOC_CAP_DMA);
  assert(buf2);
  // initialize LVGL draw buffers
  lv_disp_draw_buf_init(&disp_buf, buf1, buf2, LCD_H_RES * 20);

  ESP_LOGI(_TAG, "Register display driver to LVGL");
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = LCD_H_RES;
  disp_drv.ver_res = LCD_V_RES;
  disp_drv.flush_cb = lvgl_flush_cb;
  disp_drv.drv_update_cb = lvgl_port_update_callback;
  disp_drv.draw_buf = &disp_buf;
  disp_drv.user_data = panel_handle;
  disp = lv_disp_drv_register(&disp_drv);

  ESP_LOGI(_TAG, "Install LVGL tick timer");
  // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
  const esp_timer_create_args_t lvgl_tick_timer_args = {
      .callback = &increase_lvgl_tick,
      .name = "lvgl_tick"
  };
  esp_timer_handle_t lvgl_tick_timer = NULL;
  ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, 2 * 1000));

  lvgl_mux = xSemaphoreCreateRecursiveMutex();
  assert(lvgl_mux);
  ESP_LOGI(_TAG, "Create LVGL task");
  xTaskCreate(lvgl_port_task, "LVGL", 4 * 1024, NULL, 2, NULL);

  // Lock the mutex due to the LVGL APIs are not thread-safe
  if (lvgl_lock(-1)) {
      // Release the mutex
      lvgl_unlock();
  }
}

void update_screen() {

  // Take lvgl, blocking
  ESP_LOGI(_TAG, "waiting for lvgl mutex");
  lvgl_lock(-1);

  // Update screen
  ESP_LOGI(_TAG, "update screen");

  lv_obj_t *scr = lv_disp_get_scr_act(disp);

  static lv_obj_t *label = NULL;
  static lv_style_t style;
  if (label == NULL){
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x003a57), LV_PART_MAIN);

    lv_style_init(&style);
    lv_style_set_text_font(&style, &lv_font_montserrat_24);

    label = lv_label_create(scr);
    lv_obj_set_style_text_color(scr, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_style(label, &style, 0);
  }

  time_t now;
  struct tm timeinfo;
  time(&now); // Get time
  localtime_r(&now, &timeinfo);

  lv_label_set_text_fmt(label, "%02d:%02d\n\n%s\n%d notifications", timeinfo.tm_hour, timeinfo.tm_min, bluetooth_connected ? "Connected": "Disconnected", notification_count());

  // Release mutex
  ESP_LOGI(_TAG, "releasing lvgl mutex");
  lvgl_unlock();
}

void clock_task(void *parameter) {
  while (1) {
    time_t now;
    struct tm timeinfo;
    time(&now); // Get time
    localtime_r(&now, &timeinfo);

    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(_TAG, "current date/time is: %s", strftime_buf);

    update_screen();

    vTaskDelay((60 - timeinfo.tm_sec) * 1000 / portTICK_PERIOD_MS);
  }

  ESP_LOGE(_TAG, "gadgetbridge_task crashed");
  vTaskDelete(NULL);
}

// Send gadgetbridge message
void gadgetbridge_send(const char *message) {
  if (!bluetooth_connected) return;

  ESP_LOGI(_TAG, "tx: %s", message);
  _nordic_uart_send(message);
  _nordic_uart_send(" \n");
}

// Process received gadgetbridge message
void gadgetbridge_handle_receive(const char *message) {
  ESP_LOGI(_TAG, "rx: %s", message);

  char *json = malloc(strlen(message));
  // If message is wrapped in GB(...) then do a nasty little hack to leave the json only
  if (message[1] == 'G') {
    for (int i = 0; i < strlen(message)-5; i++) json[i] = message[i+4];
    json[strlen(message)-5] = '\0';
  } else for (int i = 0; i < strlen(message); i++) json[i] = message[i];
  ESP_LOGD(_TAG, "json: %s", json);

  cJSON *root = cJSON_Parse(json);
  if (cJSON_GetObjectItem(root, "t")) {
    char *command = cJSON_GetObjectItem(root, "t")->valuestring;

    if (strcmp(command, "notify") == 0) {
      struct notification_t *n = new_notification();
      
      // Extract fields, making copy of strings beacuse cJSON has ownership of originals
      if (cJSON_GetObjectItem(root, "id")) n->id = cJSON_GetObjectItem(root, "id")->valueint;
      if (cJSON_GetObjectItem(root, "src")) n->src = strdup(cJSON_GetObjectItem(root, "src")->valuestring);
      if (cJSON_GetObjectItem(root, "title")) n->title = strdup(cJSON_GetObjectItem(root, "title")->valuestring);
      if (cJSON_GetObjectItem(root, "subject")) n->subject = strdup(cJSON_GetObjectItem(root, "subject")->valuestring);
      if (cJSON_GetObjectItem(root, "body")) n->body = strdup(cJSON_GetObjectItem(root, "body")->valuestring);
      if (cJSON_GetObjectItem(root, "sender")) n->sender = strdup(cJSON_GetObjectItem(root, "sender")->valuestring);
      if (cJSON_GetObjectItem(root, "tel")) n->tel = strdup(cJSON_GetObjectItem(root, "tel")->valuestring);

      if (n->id == 0) {
        ESP_LOGW(_TAG, "no id so discarding");
        delete_notification(n);
      } else if (n->src == NULL) {
        ESP_LOGW(_TAG, "no src so discarding as these are usually junk, id: %d", n->id);
        delete_notification(n);
      } else {
        add_notification(n);
        ESP_LOGI(_TAG, "%d active notifications", notification_count());
        update_screen();
      }
    } else if (strcmp(command, "notify-") == 0) {
      int id = 0;
      if (cJSON_GetObjectItem(root, "id")) id = cJSON_GetObjectItem(root, "id")->valueint;

      // Clear notification if it exists
      clear_notification(id);
      ESP_LOGI(_TAG, "%d active notifications", notification_count());
      update_screen();
    } else if (strcmp(command, "musicstate") == 0) {
      char* state = "";
      if (cJSON_GetObjectItem(root, "state")) state = cJSON_GetObjectItem(root, "state")->valuestring;
      
      if (strcmp(state, "play") == 0) music_playing = 1;
      else if (strcmp(state, "pause") == 0) music_playing = 0;
      ESP_LOGI(_TAG, "music state: \"%s\"", state);
    } else {
      ESP_LOGI(_TAG, "unsupported command");
    }
  } else {
    // Not JSON, probably time set command
    if (strstr(json,"setTime") != NULL) {
      int utc = 0;
      float gmt_offset = 0.0;
      if (sscanf(json, "%*[^(]%*c%d%*[^(]%*c%f", &utc, &gmt_offset) == 2) {
        char timezone_buf [8];
        sprintf(timezone_buf, "GMT+%d", (int)gmt_offset);
        setenv("TZ", timezone_buf, 1);
        tzset();

        struct timeval tv;
        tv.tv_usec = 0;
        tv.tv_sec = utc;
        settimeofday(&tv, NULL);
        
        update_screen();

        ESP_LOGI(_TAG, "time set command, utc: %d, timezone: %s", utc, timezone_buf);
      } else {
        ESP_LOGE(_TAG, "failed to process time set command");;
      }
    } else {
      ESP_LOGI(_TAG, "couldn't extract command");
    }
  }

  cJSON_Delete(root);
  free(json);

  gpio_set_level(LED_R_GPIO, notification_count());
}

void gadgetbridge_task(void *parameter) {
  while (1) {
    if (nordic_uart_rx_buf_handle) {
      // Get latest from FIFO buffer, blocking
      size_t item_size;
      const char *message = (char *)xRingbufferReceive(nordic_uart_rx_buf_handle, &item_size, portMAX_DELAY);

      // If got a valid reference
      if (message) {
        gadgetbridge_handle_receive(message);
        vRingbufferReturnItem(nordic_uart_rx_buf_handle, (void *)message); // Return reference to be removed from buffer
      }
    } else {
      // Buffer not initialized so sleep
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }

  ESP_LOGE(_TAG, "gadgetbridge_task crashed");
  vTaskDelete(NULL);
}

void nordic_uart_callback (enum nordic_uart_callback_type callback_type) {
  if (callback_type == NORDIC_UART_CONNECTED) bluetooth_connected = 1;
  else if (callback_type == NORDIC_UART_DISCONNECTED) bluetooth_connected = 0;

  gpio_set_level(LED_L_GPIO, bluetooth_connected);
}

void app_main(void) {
  ESP_LOGI(_TAG, "INIT SCREEN START");
  init_screen();
  ESP_LOGI(_TAG, "INIT SCREEN END");

  gpio_reset_pin(BUTTON_GPIO);
  gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
  gpio_set_pull_mode(BUTTON_GPIO, GPIO_PULLUP_ONLY);

  gpio_reset_pin(LED_L_GPIO);
  gpio_set_direction(LED_L_GPIO, GPIO_MODE_OUTPUT);
  gpio_set_level(LED_L_GPIO, 0);

  gpio_reset_pin(LED_R_GPIO);
  gpio_set_direction(LED_R_GPIO, GPIO_MODE_OUTPUT);
  gpio_set_level(LED_R_GPIO, 0);

  // Configure automatic light-sleep
  esp_pm_config_t pm_config;
  pm_config.max_freq_mhz = 160;
  pm_config.min_freq_mhz = 40;
  pm_config.light_sleep_enable = true;
  if (esp_pm_configure((&pm_config)) != ESP_OK) ESP_LOGE(_TAG, "failed to configure light sleep");

  _nordic_uart_start("Nordic UART", &nordic_uart_callback);
  xTaskCreate(gadgetbridge_task, "gadgetbridge_task", 5000, NULL, 1, NULL);
  xTaskCreate(clock_task, "clock_task", 5000, NULL, 1, NULL);

  while (1) {
    if (!gpio_get_level(BUTTON_GPIO)) {
      // gadgetbridge_send("{t:\"info\", msg:\"beep boop\"}");
      // gadgetbridge_send("{t:\"status\", bat:86, volt:4.1, chg:1}");

      // if (music_playing) gadgetbridge_send("{t:\"music\", n:\"pause\"}");
      // else gadgetbridge_send("{t:\"music\", n:\"play\"}");

      struct notification_t *n = next_notification(NULL);
      if (n != NULL) {
        char buf[100];
        sprintf(buf, "{t:\"notify\", id:%d, n:\"DISMISS\"}", n->id);
        gadgetbridge_send(buf);
      }
      while (n != NULL) {
        ESP_LOGI(_TAG, "list, id: %d", n->id);
        n = next_notification(n);
      }

      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}