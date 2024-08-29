/*
 * sleep.h
 *
 *  Created on: Aug 25, 2024
 *      Author: Ed
 */

#ifndef INC_SLEEP_H_
#define INC_SLEEP_H_

void SystemClock_Config();

void sleep_init() {
	LL_C2_PWR_SetPowerMode(LL_PWR_MODE_SHUTDOWN);
}

void enter_stop(uint16_t milliseconds) {
	/* In case of debugger probe attached, work-around of issue specified in "ES0394 - STM32WB55Cx/Rx/Vx device errata":
		2.2.9 Incomplete Stop 2 mode entry after a wakeup from debug upon EXTI line 48 event
		  "With the JTAG debugger enabled on GPIO pins and after a wakeup from debug triggered by an event on EXTI
		  line 48 (CDBGPWRUPREQ), the device may enter in a state in which attempts to enter Stop 2 mode are not fully
		  effective ..."
	*/
	LL_EXTI_DisableIT_32_63(LL_EXTI_LINE_48);
	LL_C2_EXTI_DisableIT_32_63(LL_EXTI_LINE_48);

	// Disable all used wakeup source
	HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);

	/* Re-enable wakeup source */
	/* ## Setting the Wake up time ############################################*/
	/* RTC Wakeup Interrupt Generation:
	the wake-up counter is set to its maximum value to yield the longest
	stop time to let the current reach its lowest operating point.
	The maximum value is 0xFFFF, corresponding to about 33 sec. when
	RTC_WAKEUPCLOCK_RTCCLK_DIV = RTCCLK_Div16 = 16

	Wakeup Time Base = (RTC_WAKEUPCLOCK_RTCCLK_DIV /(LSI))
	Wakeup Time = Wakeup Time Base * WakeUpCounter
	  = (RTC_WAKEUPCLOCK_RTCCLK_DIV /(LSI)) * WakeUpCounter
	  ==> WakeUpCounter = Wakeup Time / Wakeup Time Base

	To configure the wake up timer to maximum value, the WakeUpCounter is set to 0xFFFF:
	Wakeup Time Base = 16 /(~32.000KHz) = ~0.5 ms
	Wakeup Time = 0.5 ms  * WakeUpCounter
	   Wakeup Time =  0,5 ms *  1000 = 0.5 second */
	if (milliseconds > 0) {
		HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, milliseconds*2, RTC_WAKEUPCLOCK_RTCCLK_DIV16);
	}

	/* Suspend SysTick */
	/**
	 * When HAL_DBGMCU_EnableDBGStopMode() is called to keep the debugger active in Stop Mode,
	 * the systick shall be disabled otherwise the cpu may crash when moving out from stop mode
	 *
	 * When in production, the HAL_DBGMCU_EnableDBGStopMode() is not called so that the device can reach best power consumption
	 * However, the systick should be disabled anyway to avoid the case when it is about to expire at the same time the device enters
	 * stop mode (this will abort the Stop Mode entry).
	 */
	HAL_SuspendTick();

	/* Enter STOP 2 mode */
	HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);

	/* ... Stop 2 mode ... */

	/* Resume SysTick */
	uwTick += milliseconds;
	HAL_ResumeTick();

	/* Configure system clock after wake-up from STOP */
	SystemClock_Config();
}

#endif /* INC_SLEEP_H_ */
