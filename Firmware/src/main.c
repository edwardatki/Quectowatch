#include <stdint.h>
#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
#include "nimble-nordic-uart.h"
#include "driver/gpio.h"
#include "cJSON.h"
#include "emlist.h"

static const char *_TAG = "MAIN";

const int BUTTON_GPIO = 9;
const int LED_L_GPIO = 12;
const int LED_R_GPIO = 13;

int bluetooth_connected = 0;
int music_playing = 0;

LinkedList* active_notifications;

struct notification_t {
  int id;
  char* src;
  char* title;
  char* subject;
  char* body;
  char* sender;
  char* tel;
};

// Send gadgetbridge message
void gadgetbridge_send(const char* message) {
  if (!bluetooth_connected) return;

  ESP_LOGI(_TAG, "tx: %s", message);
  _nordic_uart_send(message);
  _nordic_uart_send(" \n");
}

// Process received gadgetbridge message
void gadgetbridge_handle_receive(const char* message) {
  ESP_LOGI(_TAG, "rx: %s", message);

  char* json = malloc(strlen(message));
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
      // Extract fields
      struct notification_t n = {0, "", "", "", "", "", ""};
      if (cJSON_GetObjectItem(root, "id")) n.id = cJSON_GetObjectItem(root, "id")->valueint;
      if (cJSON_GetObjectItem(root, "src")) n.src = cJSON_GetObjectItem(root, "src")->valuestring;
      if (cJSON_GetObjectItem(root, "title")) n.title = cJSON_GetObjectItem(root, "title")->valuestring;
      if (cJSON_GetObjectItem(root, "subject")) n.subject = cJSON_GetObjectItem(root, "subject")->valuestring;
      if (cJSON_GetObjectItem(root, "body")) n.body = cJSON_GetObjectItem(root, "body")->valuestring;
      if (cJSON_GetObjectItem(root, "sender")) n.sender = cJSON_GetObjectItem(root, "sender")->valuestring;
      if (cJSON_GetObjectItem(root, "tel")) n.tel = cJSON_GetObjectItem(root, "tel")->valuestring;

      if (strcmp(n.src, "") == 0) {
        ESP_LOGW(_TAG, "no src so discarding as usually these are junk, id: %d", n.id);
      } else {
        if (emlist_insert(active_notifications, (void*)n.id)) ESP_LOGI(_TAG, "created notification, id: %d, src: \"%s\", title: \"%s\", subject: \"%s\", body: \"%s\" sender: \"%s\"", n.id, n.src, n.title, n.subject, n.body, n.sender);
        else ESP_LOGE(_TAG, "failed to created notification, %d", n.id);
      }
    } else if (strcmp(command, "notify-") == 0) {
      int id = 0;
      if (cJSON_GetObjectItem(root, "id")) id = cJSON_GetObjectItem(root, "id")->valueint;

      if (emlist_contains(active_notifications, (void*)id)) {
        emlist_remove(active_notifications, (void*)id);
        ESP_LOGI(_TAG, "cleared notification, id: %d", id);
      }
      else ESP_LOGW(_TAG, "failed to cleared notification, id: %d", id);
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
    ESP_LOGI(_TAG, "couldn't extract command");
  }

  ESP_LOGI(_TAG, "%d active notifications", emlist_size(active_notifications));

  cJSON_Delete(root);
  free(json);
}

void gadgetbridge_task(void *parameter) {
  active_notifications = emlist_create();

  while (1) {
    if (nordic_uart_rx_buf_handle) {
      // Get latest from FIFO buffer, allow to timeout
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

  ESP_LOGE(_TAG, "echoTask crashed");
  vTaskDelete(NULL);
}

void nordic_uart_callback (enum nordic_uart_callback_type callback_type) {
  if (callback_type == NORDIC_UART_CONNECTED) bluetooth_connected = 1;
  else if (callback_type == NORDIC_UART_DISCONNECTED) bluetooth_connected = 0;
}

void app_main(void) {
  gpio_reset_pin(BUTTON_GPIO);
  gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
  gpio_set_pull_mode(BUTTON_GPIO, GPIO_PULLUP_ONLY);

  gpio_reset_pin(LED_L_GPIO);
  gpio_set_direction(LED_L_GPIO, GPIO_MODE_OUTPUT);
  gpio_set_level(LED_L_GPIO, 0);

  gpio_reset_pin(LED_R_GPIO);
  gpio_set_direction(LED_R_GPIO, GPIO_MODE_OUTPUT);
  gpio_set_level(LED_R_GPIO, 0);

  _nordic_uart_start("Nordic UART", &nordic_uart_callback);
  xTaskCreate(gadgetbridge_task, "gadgetbridge_task", 5000, NULL, 1, NULL);

  while (1) {
    gpio_set_level(LED_L_GPIO, bluetooth_connected);
    if (active_notifications != NULL) gpio_set_level(LED_R_GPIO, emlist_size(active_notifications));

    if (!gpio_get_level(BUTTON_GPIO)) {
      // gadgetbridge_send("{t:\"info\", msg:\"beep boop\"}");
      // gadgetbridge_send("{t:\"status\", bat:86, volt:4.1, chg:1}");

      if (music_playing) gadgetbridge_send("{t:\"music\", n:\"pause\"}");
      else gadgetbridge_send("{t:\"music\", n:\"play\"}");

      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}