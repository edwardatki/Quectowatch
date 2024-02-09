#include "gadgetbridge.h"

#include "esp_log.h"
#include "esp_sntp.h"
#include "nimble-nordic-uart.h"
#include "cJSON.h"
#include "notifications.h"

static const char *_TAG = "GADGETBRIDGE";

extern void update_lcd();

int bluetooth_connected = 0;
int music_playing = 0;

// Send gadgetbridge message
void gadgetbridge_send(const char *message) {
  if (!bluetooth_connected) return;

  ESP_LOGI(_TAG, "tx: %s", message);
  _nordic_uart_send(message);
  _nordic_uart_send(" \n");
}

// Process received gadgetbridge message
static void gadgetbridge_handle_receive(const char *message) {
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
        update_lcd();
      }
    } else if (strcmp(command, "notify-") == 0) {
      int id = 0;
      if (cJSON_GetObjectItem(root, "id")) id = cJSON_GetObjectItem(root, "id")->valueint;

      // Clear notification if it exists
      clear_notification(id);
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

static void gadgetbridge_task(void *parameter) {
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

static void nordic_uart_callback (enum nordic_uart_callback_type callback_type) {
  if (callback_type == NORDIC_UART_CONNECTED) {
    ESP_LOGI(_TAG, "CONNECTED");
    bluetooth_connected = 1;
  } else if (callback_type == NORDIC_UART_DISCONNECTED) {
    ESP_LOGI(_TAG, "DISCONNECTED");
    bluetooth_connected = 0;
    music_playing = 0;
  }

  // update_lcd();
}

void gadgetbridge_init() {
    _nordic_uart_start("ESP32C3 Smartwatch", &nordic_uart_callback);
    xTaskCreate(gadgetbridge_task, "gadgetbridge_task", 5000, NULL, 1, NULL);
}