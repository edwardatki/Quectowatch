#include "lvgl.h"
#include "esp_sntp.h"

extern int bluetooth_connected;

lv_obj_t *label_time;

void example_lvgl_demo_ui(lv_disp_t *disp) {
    lv_obj_t *scr = lv_disp_get_scr_act(disp);

    lv_obj_set_style_bg_color(scr, lv_color_white(), LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

    lv_style_t style;
    lv_style_init(&style);
    lv_style_set_text_font(&style, &lv_font_montserrat_18); // Default text font
    lv_style_set_text_color(&style, lv_color_black()); // Default text color

    label_time = lv_label_create(scr);
    lv_obj_align(label_time, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_style(label_time, &style, LV_PART_MAIN);
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_48, LV_PART_MAIN);
}

int lvgl_clock_update() {
    time_t now;
    struct tm timeinfo;
    time(&now); // Get time
    localtime_r(&now, &timeinfo);

    static int last_minute = -1;
    static int last_bluetooth_connected = -1;
    if ((last_minute == timeinfo.tm_min) && (last_bluetooth_connected == bluetooth_connected)) return 0;

    lv_label_set_text_fmt(label_time, "%02d:%02d %c", timeinfo.tm_hour, timeinfo.tm_min, bluetooth_connected ? 'Y' : 'N');

    last_minute = timeinfo.tm_min;
    last_bluetooth_connected = bluetooth_connected;
    return 1;
}