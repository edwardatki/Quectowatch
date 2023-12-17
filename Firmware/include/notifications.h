#ifndef _NOTIFICATIONS_H_
#define _NOTIFICATIONS_H_

#include <stdlib.h>
#include "esp_log.h"
#include "esp_check.h"

struct notification_t;

struct notification_t {
  int id;
  char *src;
  char *title;
  char *subject;
  char *body;
  char *sender;
  char *tel;
  struct notification_t* next;
};

struct notification_t* new_notification();
void delete_notification(struct notification_t* n);
void add_notification(struct notification_t* n);
struct notification_t* get_notification(int id);
esp_err_t clear_notification(int id);
int notification_count();
struct notification_t* next_notification(struct notification_t* n);

#endif