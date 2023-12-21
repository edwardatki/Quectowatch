#include <stdint.h>
#include <stdio.h>

static const char *_TAG = "MAIN";

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
#include "nimble-nordic-uart.h"
#include "driver/gpio.h"
#include "esp_pm.h"
#include "cJSON.h"
#include "esp_sntp.h"
#include "notifications.h"
#include "lcd.h"
#include "esp_sleep.h"

#define BUTTON_GPIO 9
#define LED_L_GPIO 12
#define LED_R_GPIO 13

static QueueHandle_t gpio_event_queue = NULL;

int bluetooth_connected = 0;
int music_playing = 0;

void update_lcd() {
  // Take lvgl mutex, blocking
  lvgl_lock(-1);

  // Update LCD
  ESP_LOGI(_TAG, "update LCD");

  lv_obj_t *scr = lv_disp_get_scr_act(disp);

  static bool first_update = 1;
  static lv_obj_t *label_time, *label_status, *notification_container, *label_n_title, *label_n_body, *label_n_count;
  static lv_style_t style;
  if (first_update){
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

    lv_style_init(&style);
    lv_style_set_text_font(&style, &lv_font_montserrat_18); // Default font
    lv_style_set_text_color(&style, lv_color_white());

    label_time = lv_label_create(scr);
    lv_obj_align(label_time, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_style(label_time, &style, 0);
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_48, 0); // Change font

    label_status = lv_label_create(scr);
    lv_obj_align(label_status, LV_ALIGN_TOP_RIGHT, -2, 2);
    lv_obj_add_style(label_status, &style, 0);
    lv_obj_set_style_text_color(label_status, lv_palette_main(LV_PALETTE_GREY), 0);

    notification_container = lv_obj_create(scr);
    lv_obj_align(notification_container, LV_ALIGN_BOTTOM_MID, 0, 54); // 54 is to start off screen
    lv_obj_set_width(notification_container, 240);
    lv_obj_set_height(notification_container, 52);
    lv_obj_set_style_outline_width(notification_container, 2, 0);
    lv_obj_set_style_outline_color(notification_container, lv_color_white(), 0);
    lv_obj_set_style_pad_all(notification_container, 4, 0);
    lv_obj_set_style_bg_color(notification_container, lv_color_black(), LV_PART_MAIN);
    lv_obj_clear_flag(notification_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(notification_container, LV_SCROLLBAR_MODE_OFF);

    label_n_title = lv_label_create(scr);
    lv_obj_align(label_n_title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_width(label_n_title, 200);
    lv_label_set_long_mode(label_n_title, LV_LABEL_LONG_DOT);
    lv_obj_add_style(label_n_title, &style, 0);
    lv_obj_set_parent(label_n_title, notification_container);

    label_n_body = lv_label_create(scr);
    lv_obj_align(label_n_body, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_width(label_n_body, 234);
    lv_label_set_long_mode(label_n_body, LV_LABEL_LONG_DOT);
    lv_obj_add_style(label_n_body, &style, 0);
    lv_obj_set_parent(label_n_body, notification_container);

    label_n_count = lv_label_create(scr);
    lv_obj_align(label_n_count, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_label_set_long_mode(label_n_count, LV_LABEL_LONG_DOT);
    lv_obj_add_style(label_n_count, &style, 0);
    lv_obj_set_style_text_color(label_n_count, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_parent(label_n_count, notification_container);

    first_update = 0;
  }

  time_t now;
  struct tm timeinfo;
  time(&now); // Get time
  localtime_r(&now, &timeinfo);
  lv_label_set_text_fmt(label_time, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

  lv_label_set_text_fmt(label_status, "%s   %s", music_playing ? LV_SYMBOL_AUDIO : "", bluetooth_connected ? LV_SYMBOL_BLUETOOTH : "");

  // lv_obj_t * sw = lv_event_get_target(e);
  // lv_obj_t * label = lv_event_get_user_data(e);

  struct notification_t* n = latest_notification();
  if (n) {
      lv_label_set_text_fmt(label_n_title, "%s", n->title);
      lv_label_set_text_fmt(label_n_body, "%s", n->body);

      lv_anim_t a;
      if (lv_obj_get_y(notification_container) == 242) {
        lv_anim_init(&a);
        lv_anim_set_var(&a, notification_container); // Object to animate
        lv_anim_set_values(&a, 54, 0); // Range
        lv_anim_set_time(&a, 250); // Duration
        lv_anim_set_exec_cb(&a, anim_y_cb); // Callback
        lv_anim_set_path_cb(&a, lv_anim_path_overshoot); // Easing
        lv_anim_start(&a);
      }
  } else {
      lv_anim_t a;
      if (lv_obj_get_y(notification_container) == 188) {
        lv_anim_init(&a);
        lv_anim_set_var(&a, notification_container); // Object to animate
        lv_anim_set_values(&a, 0, 54); // Range
        lv_anim_set_time(&a, 250); // Duration
        lv_anim_set_exec_cb(&a, anim_y_cb); // Callback
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in); // Easing
        lv_anim_start(&a);
      }
  }
  lv_label_set_text_fmt(label_n_count, "%d", notification_count());

  lvgl_reactivate();

  // Release mutex
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

    update_lcd();

    // Delay until next minute digit change, plus extra 500ms so we don't wake early and get 59 seconds
    vTaskDelay((((60 - timeinfo.tm_sec) * 1000) + 200) / portTICK_PERIOD_MS);
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
        update_lcd();
      }
    } else if (strcmp(command, "notify-") == 0) {
      int id = 0;
      if (cJSON_GetObjectItem(root, "id")) id = cJSON_GetObjectItem(root, "id")->valueint;

      // Clear notification if it exists
      clear_notification(id);
      ESP_LOGI(_TAG, "%d active notifications", notification_count());
      update_lcd();
    } else if (strcmp(command, "musicstate") == 0) {
      char* state = "";
      if (cJSON_GetObjectItem(root, "state")) state = cJSON_GetObjectItem(root, "state")->valuestring;
      
      if (strcmp(state, "play") == 0) music_playing = 1;
      else if (strcmp(state, "pause") == 0) music_playing = 0;
      ESP_LOGI(_TAG, "music state: \"%s\", %d", state, music_playing);
      update_lcd();
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
        
        update_lcd();

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
  if (callback_type == NORDIC_UART_CONNECTED) {
    bluetooth_connected = 1;
  } else if (callback_type == NORDIC_UART_DISCONNECTED) {
    bluetooth_connected = 0;
    music_playing = 0;
  }

  update_lcd();
}

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_event_queue, &gpio_num, NULL);
}

void app_main(void) {
  init_lcd();
  update_lcd();

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
  pm_config.max_freq_mhz = 80;
  pm_config.min_freq_mhz = 40;
  pm_config.light_sleep_enable = false; // Recommend disabling during development as can cause programming issues over USB
  if (esp_pm_configure((&pm_config)) != ESP_OK) ESP_LOGE(_TAG, "failed to configure light sleep");

  _nordic_uart_start("Nordic UART", &nordic_uart_callback);
  xTaskCreate(gadgetbridge_task, "gadgetbridge_task", 5000, NULL, 1, NULL);
  xTaskCreate(clock_task, "clock_task", 5000, NULL, 1, NULL);

  gpio_event_queue = xQueueCreate(10, sizeof(uint32_t));

  gpio_set_intr_type(BUTTON_GPIO, GPIO_INTR_NEGEDGE);
  // gpio_intr_enable(BUTTON_GPIO);
  gpio_install_isr_service(0);
  gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, (void*) BUTTON_GPIO);

  while (1) {
    uint32_t gpio_number;
    if (xQueueReceive(gpio_event_queue, &gpio_number, portMAX_DELAY)) {
      ESP_LOGI(_TAG, "gpio %lu interrupt triggered", gpio_number);

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
     
    }
  }
}