/*
 * lcd.h
 *
 *  Created on: Aug 24, 2024
 *      Author: Ed
 */

#ifndef INC_LCD_H_
#define INC_LCD_H_

#include <stdio.h>
#include "main.h"
#include "spi.h"
#include "rtc.h"
#include "u8g2.h"
#include "dseg7_mini_bold_13.h"
#include "dseg7_mini_bold_26.h"
#include "bq27441.h"

u8g2_t lcd;

char* day_strings[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

// U8g2 driver function
uint8_t u8x8_gpio_and_delay_stm32(U8X8_UNUSED u8x8_t *u8x8, U8X8_UNUSED uint8_t msg, U8X8_UNUSED uint8_t arg_int, U8X8_UNUSED void *arg_ptr) {
	switch (msg) {
	case U8X8_MSG_GPIO_AND_DELAY_INIT:
		HAL_Delay(1);
		break;
	case U8X8_MSG_DELAY_MILLI:
		HAL_Delay(arg_int);
		break;
	case U8X8_MSG_GPIO_CS:
		HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, (GPIO_PinState)arg_int);
		break;
	case U8X8_MSG_GPIO_DC:
		// Not required
		break;
	case U8X8_MSG_GPIO_RESET:
		// Not required
		break;
	}
	return 1;
}

// U8g2 driver function
uint8_t u8x8_byte_4wire_sw_spi_stm32(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
	switch (msg) {
	case U8X8_MSG_BYTE_SEND:
		if(HAL_SPI_Transmit(&hspi1, (uint8_t*) arg_ptr, arg_int, 10000) != HAL_OK) return 0;;
		break;
	case U8X8_MSG_BYTE_INIT:
		u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_disable_level);
		break;
	case U8X8_MSG_BYTE_SET_DC:
		// Not required
		break;
	case U8X8_MSG_BYTE_START_TRANSFER:
		u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_enable_level);
//		HAL_Delay(1);
		break;
	case U8X8_MSG_BYTE_END_TRANSFER:
		HAL_Delay(1);
		u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_disable_level);
		break;
	default:
		return 0;
	}
	return 1;
}

void lcd_init() {
	u8g2_Setup_ls013b7dh03_128x128_f(&lcd, U8G2_R2, u8x8_byte_4wire_sw_spi_stm32, u8x8_gpio_and_delay_stm32);
	u8g2_InitDisplay(&lcd);
	u8g2_SetPowerSave(&lcd, 0);
}

void lcd_update(int8_t editing) {
	uint8_t flash = (HAL_GetTick() / 200) % 2;

	RTC_TimeTypeDef current_time = {0};
	RTC_DateTypeDef current_date = {0};
	HAL_RTC_GetTime(&hrtc, &current_time, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&hrtc, &current_date, RTC_FORMAT_BIN);

	u8g2_ClearBuffer(&lcd);

	u8g2_SetDrawColor(&lcd, 1);
	u8g2_DrawBox(&lcd, 0, 0, 128, 128);

	char buffer[32] = {0};

	if (editing == 0 && flash) {
		sprintf(buffer, "--:%02d", current_time.Minutes);
	} else if (editing == 1 && flash) {
		sprintf(buffer, "%02d:--", current_time.Hours);
	} else {
		sprintf(buffer, "%02d:%02d", current_time.Hours, current_time.Minutes);
	}

	u8g2_SetDrawColor(&lcd, 0);
	u8g2_SetFont(&lcd, u8g2_font_dseg7_mini_bold_26_hf);
	u8g2_DrawStr(&lcd, 2, 82, buffer);

	if (editing == 2 && flash) {
		sprintf(buffer, "--");
	} else {
		sprintf(buffer, "%02d", current_time.Seconds);
	}
	u8g2_SetDrawColor(&lcd, 0);
	u8g2_SetFont(&lcd, u8g2_font_dseg7_mini_bold_13_hf);
	u8g2_DrawStr(&lcd, 94, 38, buffer);

	sprintf(buffer, "%s", day_strings[current_date.WeekDay]);
	u8g2_SetDrawColor(&lcd, 0);
	u8g2_SetFont(&lcd, u8g2_font_6x12_tr);
	u8g2_DrawStrX2(&lcd, 6, 36, buffer);

	if (editing == 3 && flash) {
		sprintf(buffer, "---%02d-%02d", current_date.Month, current_date.Year);
	} else if (editing == 4 && flash) {
		sprintf(buffer, "%02d----%02d", current_date.Date, current_date.Year);
	} else if (editing == 5 && flash) {
		sprintf(buffer, "%02d-%02d---", current_date.Date, current_date.Month);
	} else {
		sprintf(buffer, "%02d-%02d-%02d", current_date.Date, current_date.Month, current_date.Year);
	}
	u8g2_SetDrawColor(&lcd, 0);
	u8g2_SetFont(&lcd, u8g2_font_dseg7_mini_bold_13_hf);
	u8g2_DrawStr(&lcd, 4, 108, buffer);

	sprintf(buffer, "%2d'C %3dmA %3d%%",  BQ27441_temperature(INTERNAL_TEMP)/100, BQ27441_current(AVG), BQ27441_soc(FILTERED));
	u8g2_SetDrawColor(&lcd, 0);
	u8g2_SetFont(&lcd, u8g2_font_squeezed_b7_tr);
	u8g2_DrawStr(&lcd, 128-2-u8g2_GetStrWidth(&lcd, buffer), 9, buffer);

	if (editing != -1) {
		u8g2_SetDrawColor(&lcd, 0);
		u8g2_SetFont(&lcd, u8g2_font_squeezed_b7_tr);
		u8g2_DrawStr(&lcd, 2, 9, "EDIT");
	}

	u8g2_SendBuffer(&lcd);

	HAL_GPIO_TogglePin(LCD_REFRESH_GPIO_Port, LCD_REFRESH_Pin);
}

#endif /* INC_LCD_H_ */
