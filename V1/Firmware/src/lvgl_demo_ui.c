#include "lvgl.h"
#include "esp_sntp.h"
#include "eink.h"
#include "gadgetbridge.h"
#include "notifications.h"

lv_obj_t *label_time;
lv_obj_t *label_status;
lv_obj_t *label_notification_title;
lv_obj_t *label_notification_body;

LV_FONT_DECLARE(dseg7_classic_mini_bold_56);

void lvgl_ui_init(lv_disp_t *disp) {
    lv_obj_t *scr = lv_disp_get_scr_act(disp);

    lv_obj_set_style_bg_color(scr, lv_color_white(), LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

    lv_style_t style;
    lv_style_init(&style);
    lv_style_set_text_font(&style, &lv_font_montserrat_18);
    lv_style_set_text_color(&style, lv_color_black());

    label_time = lv_label_create(scr);
    lv_obj_align(label_time, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_style(label_time, &style, LV_PART_MAIN);
    lv_obj_set_style_text_font(label_time, &dseg7_classic_mini_bold_56, LV_PART_MAIN);

    label_status = lv_label_create(scr);
    lv_obj_align(label_status, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_style(label_status, &style, LV_PART_MAIN);
    lv_obj_set_style_text_font(label_status, &lv_font_montserrat_24, LV_PART_MAIN);

    label_notification_title = lv_label_create(scr);
    lv_obj_align(label_notification_title, LV_ALIGN_BOTTOM_LEFT, 0, -40);
    lv_obj_add_style(label_notification_title, &style, LV_PART_MAIN);
    lv_obj_set_style_text_font(label_notification_title, &lv_font_montserrat_18, LV_PART_MAIN);

    label_notification_body = lv_label_create(scr);
    lv_obj_align(label_notification_body, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_add_style(label_notification_body, &style, LV_PART_MAIN);
    lv_obj_set_style_text_font(label_notification_body, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_width(label_notification_body, 200);
    lv_obj_set_height(label_notification_body, 40);
    lv_label_set_long_mode(label_notification_body, LV_LABEL_LONG_WRAP);
}

int lvgl_ui_update() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    static int last_minute = -1;
    static int last_bluetooth_connected = -1;
    static int last_music_playing = -1;
    static int last_notification_count = -1;
    if ((last_minute == timeinfo.tm_min) && (last_bluetooth_connected == bluetooth_connected) && (last_music_playing == music_playing) && (last_notification_count == notification_count())) return 0;

    if (last_bluetooth_connected != bluetooth_connected) refresh_mode = 1;
    if (last_music_playing != music_playing) refresh_mode = 1;
    if (last_notification_count != notification_count()) refresh_mode = 1;
    if (last_minute != timeinfo.tm_min) refresh_mode = 2;

    lv_label_set_text_fmt(label_time, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    if (notification_count() > 0) lv_label_set_text_fmt(label_status, "%d %s", notification_count(), bluetooth_connected ? LV_SYMBOL_BLUETOOTH : "");
    else lv_label_set_text_fmt(label_status, "%s  %s", music_playing ? LV_SYMBOL_AUDIO : "", bluetooth_connected ? LV_SYMBOL_BLUETOOTH : "");

    if (notification_count() > 0) {
        lv_label_set_text_fmt(label_notification_title, "%s", latest_notification()->title);
        lv_label_set_text_fmt(label_notification_body, "%s", latest_notification()->body);
    } else {
        lv_label_set_text_fmt(label_notification_title, "None");
        lv_label_set_text_fmt(label_notification_body, "...");
    }

    last_minute = timeinfo.tm_min;
    last_bluetooth_connected = bluetooth_connected;
    last_music_playing = music_playing;
    last_notification_count = notification_count();
    return 1;
}