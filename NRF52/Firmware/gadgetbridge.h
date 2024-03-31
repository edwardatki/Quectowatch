#include "cJSON.h"
#include "notifications.h"

int music_playing = 0;

// Process received gadgetbridge message
void gadgetbridge_handle_receive(const char *message) {
  printf("rx: %s\n", message);

  char *json = (char*)malloc(strlen(message));
  // If message is wrapped in GB(...) then do a nasty little hack to leave the json only
  if (message[1] == 'G') {
    for (int i = 0; i < strlen(message)-5; i++) json[i] = message[i+4];
    json[strlen(message)-5] = '\0';
  } else for (int i = 0; i < strlen(message); i++) json[i] = message[i];
  printf("json: %s\n", json);

  cJSON *root = cJSON_Parse(json);
  printf("parsed\n");
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
        printf("no id so discarding\n");
        delete_notification(n);
      } else if (n->src == NULL) {
        printf("no src so discarding as these are usually junk, id: %d\n", n->id);
        delete_notification(n);
      } else {
        add_notification(n);
        // update_lcd();
      }
    } else if (strcmp(command, "notify-") == 0) {
      printf("NOTIFY-\n");
      int id = 0;
      if (cJSON_GetObjectItem(root, "id")) id = cJSON_GetObjectItem(root, "id")->valueint;

      // // Clear notification if it exists
      printf("try clear\n");
      // clear_notification(id);
      // printf("cleared\n");
      // update_lcd();
    } else if (strcmp(command, "musicstate") == 0) {
      char* state = (char*)"";
      if (cJSON_GetObjectItem(root, "state")) state = cJSON_GetObjectItem(root, "state")->valuestring;
      
      if (strcmp(state, "play") == 0) music_playing = 1;
      else if (strcmp(state, "pause") == 0) music_playing = 0;
      printf("music state: \"%s\", %d\n", state, music_playing);
      // update_lcd();
    } else {
      printf("unsupported command\n");
    }
  } else {
    // Not JSON, probably time set command
    if (strstr(json,"setTime") != NULL) {
      int utc = 0;
      float gmt_offset = 0.0;
      if (sscanf(json, "%*[^(]%*c%d%*[^(]%*c%f", &utc, &gmt_offset) == 2) {
        // char timezone_buf [8];
        // sprintf(timezone_buf, "GMT+%d", (int)gmt_offset);
        // setenv("TZ", timezone_buf, 1);
        // tzset();

        // struct timeval tv;
        // tv.tv_usec = 0;
        // tv.tv_sec = utc;
        // settimeofday(&tv, NULL);
        
        // update_lcd();

        // ESP_LOGI(_TAG, "time set command, utc: %d, timezone: %s", utc, timezone_buf);
      } else {
        printf("failed to process time set command\n");
      }
    } else {
      printf("couldn't extract command\n");
    }
  }

  cJSON_Delete(root);
  free(json);
}