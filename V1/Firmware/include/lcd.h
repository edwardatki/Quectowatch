#ifndef _LCD_H_
#define _LCD_H_

#include "freertos/semphr.h"
#include "lvgl.h"

extern SemaphoreHandle_t lvgl_mux;
extern lv_disp_t *disp;

void anim_x_cb(void *var, int32_t v);
void anim_y_cb(void *var, int32_t v);

bool lvgl_lock(int timeout_ms);
void lvgl_unlock();
void lvgl_reactivate();
void init_lcd();

#endif