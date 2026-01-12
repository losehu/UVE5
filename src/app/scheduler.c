/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#ifndef ENABLE_OPENCV
#include <Arduino.h>
#else
#include "opencv/Arduino.hpp"
#endif
#include "app/chFrScanner.h"
#ifdef ENABLE_FMRADIO
	#include "app/fm.h"
#endif
#include "app/scanner.h"
#include "audio.h"
#include "functions.h"
#include "helper/battery.h"
#include "misc.h"
#include "settings.h"

#include "driver/backlight.h"

#define DECREMENT(cnt) \
	do {               \
		if (cnt > 0)   \
			cnt--;     \
	} while (0)

#define DECREMENT_AND_TRIGGER(cnt, flag) \
	do {                                 \
		if (cnt > 0)                     \
			if (--cnt == 0)              \
				flag = true;             \
	} while (0)

static volatile uint32_t gGlobalSysTickCounter;
void SystickHandler(void);
void SCHEDULER_Init(void);

#ifndef ENABLE_OPENCV
// ESP32-S3 硬件定时器
static hw_timer_t *timer = NULL;
static portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;


// 定时器中断回调函数 (在 ISR 上下文中执行)
void IRAM_ATTR onTimer(void)
{
	portENTER_CRITICAL_ISR(&timerMux);
	SystickHandler();
	portEXIT_CRITICAL_ISR(&timerMux);
}

// 初始化调度器定时器
void SCHEDULER_Init(void)
{
	// 配置硬件定时器
	// ESP32-S3 主频 240MHz
	// 定时器 0, 分频器 240 (240MHz / 240 = 1MHz, 每个计数 = 1us)
	// 参数: timer_num, divider, count_up
	timer = timerBegin(0, 240, true);
	
	// 绑定中断回调函数
	timerAttachInterrupt(timer, &onTimer, true);
	
	// 设置定时器报警值: 10000us = 10ms
	// 参数: timer, alarm_value, auto_reload
	timerAlarmWrite(timer, 10000, true);
	
	// 启动定时器
	timerAlarmEnable(timer);
}
#endif

// we come here every 10ms
void SystickHandler(void)
{
	gGlobalSysTickCounter++;
	
	gNextTimeslice = true;

	if ((gGlobalSysTickCounter % 50) == 0)
	{
		gNextTimeslice_500ms = true;
		
		DECREMENT_AND_TRIGGER(gTxTimerCountdown_500ms, gTxTimeoutReached);
		DECREMENT(gSerialConfigCountDown_500ms);
	}

	if ((gGlobalSysTickCounter & 3) == 0)
		gNextTimeslice40ms = true;

	#ifdef ENABLE_NOAA
		DECREMENT(gNOAACountdown_10ms);
	#endif

	DECREMENT(gFoundCDCSSCountdown_10ms);

	DECREMENT(gFoundCTCSSCountdown_10ms);

	if (gCurrentFunction == FUNCTION_FOREGROUND)
		DECREMENT_AND_TRIGGER(gBatterySaveCountdown_10ms, gSchedulePowerSave);

	if (gCurrentFunction == FUNCTION_POWER_SAVE)
		DECREMENT_AND_TRIGGER(gPowerSave_10ms, gPowerSaveCountdownExpired);

	if (gScanStateDir == SCAN_OFF && !gCssBackgroundScan && gEeprom.DUAL_WATCH != DUAL_WATCH_OFF)
		if (gCurrentFunction != FUNCTION_MONITOR && gCurrentFunction != FUNCTION_TRANSMIT && gCurrentFunction != FUNCTION_RECEIVE)
			DECREMENT_AND_TRIGGER(gDualWatchCountdown_10ms, gScheduleDualWatch);

	#ifdef ENABLE_NOAA
		if (gScanStateDir == SCAN_OFF && !gCssBackgroundScan && gEeprom.DUAL_WATCH == DUAL_WATCH_OFF)
			if (gIsNoaaMode && gCurrentFunction != FUNCTION_MONITOR && gCurrentFunction != FUNCTION_TRANSMIT)
				if (gCurrentFunction != FUNCTION_RECEIVE)
					DECREMENT_AND_TRIGGER(gNOAA_Countdown_10ms, gScheduleNOAA);
	#endif

	if (gScanStateDir != SCAN_OFF)
		if (gCurrentFunction != FUNCTION_MONITOR && gCurrentFunction != FUNCTION_TRANSMIT)
			DECREMENT_AND_TRIGGER(gScanPauseDelayIn_10ms, gScheduleScanListen);

	DECREMENT_AND_TRIGGER(gTailToneEliminationCountdown_10ms, gFlagTailToneEliminationComplete);

	#ifdef ENABLE_VOICE
		DECREMENT_AND_TRIGGER(gCountdownToPlayNextVoice_10ms, gFlagPlayQueuedVoice);
	#endif
	
	#ifdef ENABLE_FMRADIO
		if (gFM_ScanState != FM_SCAN_OFF && gCurrentFunction != FUNCTION_MONITOR)
			if (gCurrentFunction != FUNCTION_TRANSMIT && gCurrentFunction != FUNCTION_RECEIVE)
				DECREMENT_AND_TRIGGER(gFmPlayCountdown_10ms, gScheduleFM);
	#endif

	#ifdef ENABLE_VOX
		DECREMENT(gVoxStopCountdown_10ms);
	#endif

	DECREMENT(boot_counter_10ms);
}
