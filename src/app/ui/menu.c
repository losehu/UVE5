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
#include "../driver/eeprom.h"
#include <string.h>
#include <stdlib.h>  // abs()
#include "../bitmaps.h"
#include "../driver/uart1.h"
#include "../app/dtmf.h"
#include "../app/menu.h"
#include "../board.h"
#include "../dcs.h"
#include "../driver/backlight.h"
#include "../driver/bk4819.h"
#include "../driver/st7565.h"
#include "../frequencies.h"
#include "../helper/battery.h"
#include "../misc.h"
#include "../pinyin_blob.h"
#include "../settings.h"
#include "helper.h"
#include "inputbox.h"
#include "menu.h"
#include "ui.h"
#include "../chinese.h"

#ifdef ENABLE_PINYIN
static uint8_t PINYIN_ReadCount(uint8_t index);
static uint16_t PINYIN_CountRange(uint8_t start, uint8_t end);
static uint16_t PINYIN_FlatIndex(uint8_t start, uint8_t index, uint8_t offset);
static bool PINYIN_GetFlatEntry(uint8_t start, uint8_t end, uint16_t flat_index, uint8_t *entry_index, uint8_t *entry_offset);
#endif
void insertNewline(char a[], int index, int len) {

    if (index < 0 || index >= len || len >= 63) {
        return;
    }
    for (int i = len; i >= index; i--) {
        a[i + 1] = a[i];
    }
    a[index] = '\n';
    a[len + 1] = '\0'; // Null-terminate the string
}

const t_menu_item MenuList[] =
        {
//   text,     voice ID,                               menu ID
                {/*"Step",*/   VOICE_ID_FREQUENCY_STEP, MENU_STEP, STR_STEP},
                {/*"RxDCS",*/  VOICE_ID_DCS, MENU_R_DCS, STR_RX_DCS}, // was "R_DCS"
                {/*"RxCTCS",*/ VOICE_ID_CTCSS, MENU_R_CTCS, STR_RX_CTCS}, // was "R_CTCS"
                {/*"TxDCS",*/  VOICE_ID_DCS, MENU_T_DCS, STR_TX_DCS}, // was "T_DCS"
                {/*"TxCTCS",*/ VOICE_ID_CTCSS, MENU_T_CTCS, STR_TX_CTCS}, // was "T_CTCS"
                {/*"TxODir",*/ VOICE_ID_TX_OFFSET_FREQUENCY_DIRECTION, MENU_SFT_D, STR_TX_OFFSET_DIR}, // was "SFT_D"
                {/*"TxOffs",*/ VOICE_ID_TX_OFFSET_FREQUENCY, MENU_OFFSET, STR_TX_OFFSET}, // was "OFFSET"
#ifdef ENABLE_CUSTOM_SIDEFUNCTIONS

                {/*"W/N",*/    VOICE_ID_CHANNEL_BANDWIDTH,             MENU_W_N           ,STR_BANDWIDTH},
#endif

                {/*"Scramb",*/ VOICE_ID_SCRAMBLER_ON, MENU_SCR, STR_SCRAMBLER}, // was "SCR"
                {/*"BusyCL",*/ VOICE_ID_BUSY_LOCKOUT, MENU_BCL, STR_BUSY_LOCK}, // was "BCL"
                {/*"Compnd",*/ VOICE_ID_INVALID, MENU_COMPAND, STR_COMPAND},
                {/*"ChSave",*/ VOICE_ID_MEMORY_CHANNEL, MENU_MEM_CH, STR_CH_SAVE}, // was "MEM-CH"
                {/*"ChDele",*/ VOICE_ID_DELETE_CHANNEL, MENU_DEL_CH, STR_CH_DELETE}, // was "DEL-CH"
                {/*"ChName",*/ VOICE_ID_INVALID, MENU_MEM_NAME, STR_CH_NAME},
                {/*"SList",*/  VOICE_ID_INVALID, MENU_S_LIST, STR_SCAN_LIST},
                {/*"SList1",*/ VOICE_ID_INVALID, MENU_SLIST1, STR_SCAN_LIST1},
                {/*"SList2",*/ VOICE_ID_INVALID, MENU_SLIST2, STR_SCAN_LIST2},
                {/*"ScnRev",*/ VOICE_ID_INVALID, MENU_SC_REV, STR_SCAN_RESUME},
                {/*"TxTOut",*/ VOICE_ID_TRANSMIT_OVER_TIME, MENU_TOT, STR_TX_TIMEOUT}, // was "TOT"
                {/*"BatSav",*/ VOICE_ID_SAVE_MODE, MENU_SAVE, STR_BAT_SAVE}, // was "SAVE"
                {/*"Mic",*/    VOICE_ID_INVALID, MENU_MIC, STR_MIC_GAIN},
                {/*"ChDisp",*/ VOICE_ID_INVALID, MENU_MDF, STR_CH_DISPLAY}, // was "MDF"
#if ENABLE_CHINESE_FULL == 4
                {/*"POnMsg",*/ VOICE_ID_INVALID,                       MENU_PONMSG        ,STR_POWER_ON_MSG},
#endif
                {/*"BackLt",*/ VOICE_ID_INVALID, MENU_ABR, STR_AUTO_BACKLIGHT}, // was "ABR"
                {/*"BLMax",*/  VOICE_ID_INVALID, MENU_ABR_MAX, STR_BACKLIGHT_LEVEL},
                {/*"MDCID",*/  VOICE_ID_INVALID, MENU_MDC_ID, STR_MDC_ID},

                {/*"Roger",*/  VOICE_ID_INVALID, MENU_ROGER, STR_ROGER},

                {/*"STE",*/    VOICE_ID_INVALID, MENU_STE, STR_STE},
                {/*"RP STE",*/ VOICE_ID_INVALID, MENU_RP_STE, STR_RP_STE},
                {/*"1 Call",*/ VOICE_ID_INVALID, MENU_1_CALL, STR_1_CALL},

#ifdef ENABLE_CUSTOM_SIDEFUNCTIONS
                {/*"F1Shrt",*/ VOICE_ID_INVALID,                       MENU_F1SHRT        ,STR_F1_SHORT},
                {/*"F1Long",*/ VOICE_ID_INVALID,                       MENU_F1LONG        ,STR_F1_LONG},
                {/*"F2Shrt",*/ VOICE_ID_INVALID,                       MENU_F2SHRT        ,STR_F2_SHORT},
                {/*"F2Long",*/ VOICE_ID_INVALID,                       MENU_F2LONG        ,STR_F2_LONG},
                {/*"M Long",*/ VOICE_ID_INVALID,                       MENU_MLONG         ,STR_M_LONG},
#endif

#ifdef ENABLE_DTMF_CALLING

                {/*"ANI ID",*/ VOICE_ID_ANI_CODE,                      MENU_ANI_ID        ,STR_DTMF_ID},
#endif
                {/*"UPCode",*/ VOICE_ID_INVALID, MENU_UPCODE, STR_DTMF_UP_CODE},
                {/*"DWCode",*/ VOICE_ID_INVALID, MENU_DWCODE, STR_DTMF_DOWN_CODE},
                {/*"PTT ID",*/ VOICE_ID_INVALID, MENU_PTT_ID, STR_DTMF_PTT_ID},
                {/*"D ST",*/   VOICE_ID_INVALID, MENU_D_ST, STR_DTMF_SIDE_TONE},
#ifdef ENABLE_DTMF_CALLING

                {/*"D Resp",*/ VOICE_ID_INVALID,                       MENU_D_RSP         ,STR_DTMF_RESPONSE},
                {/*"D Hold",*/ VOICE_ID_INVALID,                       MENU_D_HOLD        ,STR_DTMF_HOLD},
#endif
                {/*"D Prel",*/ VOICE_ID_INVALID, MENU_D_PRE, STR_DTMF_PREAMBLE},
#ifdef ENABLE_DTMF_CALLING
#ifdef ENABLE_CUSTOM_SIDEFUNCTIONS

                {/*"D Decd",*/ VOICE_ID_INVALID,                       MENU_D_DCD         ,STR_DTMF_DECODE},
#endif
                {/*"D List",*/ VOICE_ID_INVALID,                       MENU_D_LIST        ,STR_DTMF_CONTACTS},
#endif
                {/*"D Live",*/ VOICE_ID_INVALID, MENU_D_LIVE_DEC, STR_DTMF_DISPLAY}, // live DTMF decoder
#ifdef ENABLE_AM_FIX//1
                {/*"AM Fix",*/ VOICE_ID_INVALID,                       MENU_AM_FIX        ,STR_AM_FIX},
#endif
#ifdef ENABLE_AM_FIX_TEST1//0
                {/*"AM FT1",*/ VOICE_ID_INVALID,                       MENU_AM_FIX_TEST1  ,""},
#endif

                {/*"RxMode",*/ VOICE_ID_DUAL_STANDBY, MENU_TDR, STR_RX_MODE},
                {/*"Sql",*/    VOICE_ID_SQUELCH, MENU_SQL, STR_SQUELCH},

                // hidden menu items from here on
                // enabled if pressing both the PTT and upper side button at power-on
                {/*"F Lock",*/ VOICE_ID_INVALID, MENU_F_LOCK, STR_FREQ_LOCK},
//                {/*"Tx 200",*/ VOICE_ID_INVALID,                       MENU_200TX         ,两百M发射}, // was "200TX"
//                {/*"Tx 350",*/ VOICE_ID_INVALID,                       MENU_350TX         ,三百五十M发射}, // was "350TX"
//                {/*"Tx 500",*/ VOICE_ID_INVALID,                       MENU_500TX         ,五百M发射}, // was "500TX"
//                {/*"350 En",*/ VOICE_ID_INVALID,                       MENU_350EN         ,三百五十M接收}, // was "350EN"
#ifdef ENABLE_F_CAL_MENU//0
                {/*"FrCali",*/ VOICE_ID_INVALID,                       MENU_F_CALI        ,""}, // reference xtal calibration
#endif
                {/*"BatCal",*/ VOICE_ID_INVALID, MENU_BATCAL, STR_BAT_CAL}, // battery voltage calibration
                {/*"BatTyp",*/ VOICE_ID_INVALID, MENU_BATTYP, STR_BAT_VOL}, // battery type 1600/2200mAh
                {/*"Reset",*/  VOICE_ID_INITIALISATION, MENU_RESET,
                               STR_RESET}, // might be better to move this to the hidden menu items ?

                {/*"",*/       VOICE_ID_INVALID, 0xff, "\x00"}  // end of list - DO NOT delete or move this this
        };

#ifdef ENABLE_CUSTOM_SIDEFUNCTIONS
#if ENABLE_CHINESE_FULL==0 || defined(ENABLE_ENGLISH)
#ifdef ENABLE_ENGLISH
const char gSubMenu_W_N[][7] =//7

#else
const char gSubMenu_W_N[][3] =//7

#endif
#else
        const char gSubMenu_W_N[][5] =//7
#endif
        {
//                "WIDE",
//                "NARROW"
                STR_WIDE,
               STR_NARROW
        };
#endif
#if ENABLE_CHINESE_FULL == 4
const char gSubMenu_PONMSG[][5]={
        STR_CLOSE,
        STR_PICTURE,
        STR_MESSAGE
};
#endif
#if ENABLE_CHINESE_FULL != 4 || defined(ENABLE_ENGLISH)
#ifdef ENABLE_ENGLISH
const char gSubMenu_SFT_D[][4] =

#else
const char gSubMenu_SFT_D[][10] =//4
#endif
#else
        const char gSubMenu_SFT_D[][16] =//4
#endif
        {
//                "OFF",
//                "+",
//                "-"
                STR_TX_OFFSET_OFF,
                STR_TX_OFFSET_PLUS,
                STR_TX_OFFSET_MINUS

        };


#if ENABLE_CHINESE_FULL != 4 || defined(ENABLE_ENGLISH)
#ifdef ENABLE_ENGLISH
const char gSubMenu_OFF_ON[][4] =

#else
const char gSubMenu_OFF_ON[][3] =//4
#endif

#else
        const char gSubMenu_OFF_ON[][5] =//4
#endif
        {
//                "OFF",
//                "ON"
                STR_OFF,
                STR_ON
        };
#if ENABLE_CHINESE_FULL != 4 || defined(ENABLE_ENGLISH)

const char gSubMenu_SAVE[][4] =//4
#else
        const char gSubMenu_SAVE[][6] =//4
#endif
        {
//                "OFF",
//                "1:1",
//                "1:2",
//                "1:3",
//                "1:4"

                STR_OFF,
                STR_LEVEL_1,
                STR_LEVEL_2,
                STR_LEVEL_3,
                STR_LEVEL_4

        };
#if ENABLE_CHINESE_FULL != 4 || defined(ENABLE_ENGLISH)
const char gSubMenu_TOT[][7] = //7
#else
        const char gSubMenu_TOT[][6] = //7
#endif
        {
//                "30 sec",
//                "1 min",
//                "2 min",
//                "3 min",
//                "4 min",
//                "5 min",
//                "6 min",
//                "7 min",
//                "8 min",
//                "9 min",
//                "15 min"

                STR_30_SEC,
                STR_1_MIN,
                STR_2_MIN,
                STR_3_MIN,
                STR_4_MIN,
                STR_5_MIN,
                STR_6_MIN,
                STR_7_MIN,
                STR_8_MIN,
                STR_9_MIN,
                STR_15_MIN

        };

const char *const gSubMenu_RXMode[] =
        {

//                "MAIN\nONLY",        // TX and RX on main only
//                "DUAL RX\nRESPOND", // Watch both and respond
//                "CROSS\nBAND",        // TX on main, RX on secondary
//                "MAIN TX\nDUAL RX"    // always TX on main, but RX on both
                STR_MAIN_ONLY,        // TX and RX on main only
                STR_DUAL_RX, // Watch both and respond
                STR_CROSS_BAND,        // TX on main, RX on secondary
                STR_MAIN_TX_DUAL_RX    // always TX on main, but RX on both

        };

#ifdef ENABLE_VOICE
const char gSubMenu_VOICE[][4] =
{
    "OFF",
    "CHI",
    "ENG"
};
#endif
#if ENABLE_CHINESE_FULL != 4 || defined(ENABLE_ENGLISH)
#ifdef ENABLE_ENGLISH
const char gSubMenu_SC_REV[][8] =//8

#else
const char gSubMenu_SC_REV[][10] =//8
#endif

#else
        const char gSubMenu_SC_REV[][18] =//8
#endif
        {
//                "TIMEOUT",
//                "CARRIER",
//                "STOP"
                STR_SCAN_RESUME_TIME,
                STR_SCAN_RESUME_CARRIER,
                STR_SCAN_RESUME_STOP

        };

const char *const gSubMenu_MDF[] =
        {
//                "FREQ",
//                "CHANNEL\nNUMBER",
//                "NAME",
//                "NAME\n+\nFREQ"
                STR_FREQ,
                STR_CHANNEL_NUMBER,
                STR_NAME,
                STR_NAME_FREQ
        };

#ifdef ENABLE_ALARM
const char gSubMenu_AL_MOD[][5] =
{
    "SITE",
    "TONE"
};
#endif
#ifdef ENABLE_DTMF_CALLING
#if ENABLE_CHINESE_FULL!=4 || defined(ENABLE_ENGLISH)

#ifdef ENABLE_ENGLISH
const char gSubMenu_D_RSP[][11] =//11

#else
const char gSubMenu_D_RSP[][10] =//11
#endif
#else
const char gSubMenu_D_RSP[][18] =//11
#endif
        {
//                "DO\nNOTHING",
//                "RING",
//                "REPLY",
//                "BOTH"
                STR_NO_RESPONSE,
                STR_LOCAL_RING,
                STR_REPLY,
               STR_RING_REPLY
        };
#endif

const char *const gSubMenu_PTT_ID[] =
        {
//                "OFF",
//                "UP CODE",
//                "DOWN CODE",
//                "UP+DOWN\nCODE",
//                "APOLLO\nQUINDAR"
                STR_NO_TX,
                STR_UP_CODE,
                STR_DOWN_CODE,
                STR_UP_DOWN_CODE,
                STR_QUINDAR_TONE
        };


#if ENABLE_CHINESE_FULL != 4 || defined(ENABLE_ENGLISH)


#ifdef ENABLE_ENGLISH
const char gSubMenu_ROGER[][15] =

#else
const char gSubMenu_ROGER[][13] =
#endif
#else
        const char gSubMenu_ROGER[][15] =
#endif
        {
//                "OFF",
//                "ROGER",
//                "MDC"

                STR_CLOSE,
                STR_ROGER_TONE,
                STR_MDC_END,
                STR_MDC_BEGIN,
                STR_MDC_BOTH,
                STR_MDC_BEGIN_ROGER
        };
#if ENABLE_CHINESE_FULL != 4 || defined(ENABLE_ENGLISH)

#ifdef ENABLE_ENGLISH
const char gSubMenu_RESET[][4] =//4

#else
const char gSubMenu_RESET[][6] =//4
#endif

#else
        const char gSubMenu_RESET[][11] =//4
#endif
        {
//                "VFO",
//                "ALL"
                STR_VFO_PARAM,
                STR_ALL_PARAM
        };

const char *const gSubMenu_F_LOCK[] =
        {
                "DEFAULT+\n137-174\n400-470",
                "FCC HAM\n144-148\n420-450",
                "CE HAM\n144-146\n430-440",
                "GB HAM\n144-148\n430-440",

//                "DISABLE\nALL",
//                "UNLOCK\nALL",
                STR_DISABLE_ALL,
                STR_UNLOCK_ALL,
        };
#if ENABLE_CHINESE_FULL != 4 || defined(ENABLE_ENGLISH)

#ifdef ENABLE_ENGLISH
const char gSubMenu_BACKLIGHT[][7] =//7

#else
const char gSubMenu_BACKLIGHT[][5] =//7
#endif
#else
        const char gSubMenu_BACKLIGHT[][6] =//7
#endif
        {
//                "OFF",
//                "5 sec",
//                "10 sec",
//                "20 sec",
//                "1 min",
//                "2 min",
//                "4 min",
//                "ON"
                STR_OFF,
                STR_5_SEC,
                STR_10_SEC,
                STR_20_SEC,
                STR_1_MIN,
                STR_2_MIN,
                "4 min",
                STR_ON

        };
#if ENABLE_CHINESE_FULL != 4 || defined(ENABLE_ENGLISH)


#ifdef ENABLE_ENGLISH
const char gSubMenu_RX_TX[][6] =//6

#else
const char gSubMenu_RX_TX[][7] =//6
#endif
#else
        const char gSubMenu_RX_TX[][12] =//6
#endif
        {
//                "OFF",
//                "TX",
//                "RX",
//                "TX/RX"
                STR_OFF,
                STR_TX_TIME,
                STR_RX_TIME,
                STR_TX_RX_TIME
        };

#ifdef ENABLE_AM_FIX_TEST1
const char gSubMenu_AM_fix_test1[][8] =
{
    "LNA-S 0",
    "LNA-S 1",
    "LNA-S 2",
    "LNA-S 3"
};
#endif


const char gSubMenu_BATTYP[][8] =
        {
                "1600mAh",
                "2200mAh"
        };

const char gSubMenu_SCRAMBLER[][7] =
        {
//                "OFF",
                STR_OFF,

                "2600Hz",
                "2700Hz",
                "2800Hz",
                "2900Hz",
                "3000Hz",
                "3100Hz",
                "3200Hz",
                "3300Hz",
                "3400Hz",
                "3500Hz"
        };

#ifdef ENABLE_CUSTOM_SIDEFUNCTIONS
const t_sidefunction SIDEFUNCTIONS[] =
        {
               {STR_OFF, ACTION_OPT_NONE},
#ifdef ENABLE_FLASHLIGHT
               {STR_FLASHLIGHT, ACTION_OPT_FLASHLIGHT},
#endif
               {STR_POWER, ACTION_OPT_POWER},
               {STR_MONITOR, ACTION_OPT_MONITOR},
               {STR_SCAN, ACTION_OPT_SCAN},
#ifdef ENABLE_VOX
               {STR_VOX,				ACTION_OPT_VOX},
#endif
#ifdef ENABLE_ALARM
                {"ALARM",			ACTION_OPT_ALARM},
#endif
#ifdef ENABLE_FMRADIO
               {STR_FM_RADIO,		ACTION_OPT_FM},
#endif
#ifdef ENABLE_TX1750
                {"1750HZ",			ACTION_OPT_1750},
#endif
               {STR_KEY_LOCK, ACTION_OPT_KEYLOCK},
               {STR_SWITCH_VFO, ACTION_OPT_A_B},
               {STR_VFO_MR, ACTION_OPT_VFO_MR},
               {STR_SWITCH_DEMODULATION, ACTION_OPT_SWITCH_DEMODUL},
               {STR_DTMF_DECODE, ACTION_OPT_D_DCD},
               {STR_SWITCH_BANDWIDTH, ACTION_OPT_WIDTH},
#ifdef ENABLE_SIDEFUNCTIONS_SEND
               {STR_MAIN_TX, ACTION_OPT_SEND_CURRENT},
               {STR_DUAL_TX, ACTION_OPT_SEND_OTHER},
#endif
#ifdef ENABLE_BLMIN_TMP_OFF
                {"BLMIN\nTMP OFF",  ACTION_OPT_BLMIN_TMP_OFF}, 		//BackLight Minimum Temporay OFF
#endif
        };
const t_sidefunction *gSubMenu_SIDEFUNCTIONS = SIDEFUNCTIONS;
const uint8_t gSubMenu_SIDEFUNCTIONS_size = ARRAY_SIZE(SIDEFUNCTIONS);
#endif

bool gIsInSubMenu;
uint8_t gMenuCursor;

int UI_MENU_GetCurrentMenuId() {

    if (gMenuCursor < ARRAY_SIZE(MenuList))
        return MenuList[gMenuCursor].menu_id;
    else
        return MenuList[ARRAY_SIZE(MenuList) - 1].menu_id;
}

uint8_t UI_MENU_GetMenuIdx(uint8_t id) {
    for (uint8_t i = 0; i < ARRAY_SIZE(MenuList); i++)
        if (MenuList[i].menu_id == id)
            return i;
    return 0;
}

int32_t gSubMenuSelection;

// edit box
char edit_original[17]; // a copy of the text before editing so that we can easily test for changes/difference
char edit[17];
int edit_index;


void UI_DisplayMenu(void) {
    const unsigned int menu_list_width = 6; // max no. of characters on the menu list (left side)
    const unsigned int menu_item_x1 = (8 * menu_list_width);//+ 2;
    const unsigned int menu_item_x2 = LCD_WIDTH - 1;
    unsigned int i;
    char String[64];  // bigger cuz we can now do multi-line in one string (use '\n' char)
#ifdef ENABLE_DTMF_CALLING
    char               Contact[16];
#endif

    // clear the screen buffer
    UI_DisplayClear();

#if 1
    // original menu layout



    // invert the current menu list item pixels反转当前菜单项的像素值 ：


    // draw vertical separating dotted line绘制垂直分隔的点线 ：
//    for (i = 0; i < 7; i++)
//        gFrameBuffer[i][(8 * menu_list_width) + 1] = 0xAA;


    // draw the little sub-menu triangle marker绘制子菜单三角标志：
    //const void *BITMAP_CurrentIndicator = BITMAP_MARKER;

    if (gIsInSubMenu)
        memmove(gFrameBuffer[2] + 41, BITMAP_VFO_Default, sizeof(BITMAP_VFO_Default));
#ifndef ENABLE_MDC1200
    uint8_t add = 1;

    if (gMenuCursor + 1 >= 26)
        add = 0;

    sprintf(String, "%2u/%u", add + gMenuCursor, gMenuListCount - 1);

#else
    sprintf(String, "%2u/%u", 1 + gMenuCursor, gMenuListCount);
#endif

#ifdef ENABLE_PINYIN //拼音取消显示
    const bool isInPinyin = UI_MENU_GetCurrentMenuId() == MENU_MEM_NAME && gIsInSubMenu && edit_index >= 0;
    if (!isInPinyin)
#endif
    UI_PrintStringSmall(String, 2, 0, 6);
#ifdef ENABLE_PINYIN//拼音取消显示
    if (!isInPinyin)
#endif


#ifdef ENABLE_ENGLISH
    {
    uint8_t size_menu = strlen(MenuList[gMenuCursor].name)*7;
    UI_PrintStringSmall(MenuList[gMenuCursor].name, size_menu < 48 ? (48 - size_menu) / 2 : 0, 0, 0);
    }
#else
    {
        UI_ShowChineseMenu();
    }
#endif

#else
    {	// new menu layout .. experimental & unfinished

        const int menu_index = gMenuCursor;  // current selected menu item
        i = 1;

        if (!gIsInSubMenu)
        {
            while (i < 2)
            {	// leading menu items - small text
                const int k = menu_index + i - 2;
                if (k < 0)
                    UI_PrintStringSmall(MenuList[gMenuListCount + k].name, 0, 0, i);  // wrap-a-round
                else
                if (k >= 0 && k < (int)gMenuListCount)
                    UI_PrintStringSmall(MenuList[k].name, 0, 0, i);
                i++;
            }

            // current menu item - keep big n fat
            if (menu_index >= 0 && menu_index < (int)gMenuListCount)
                UI_PrintStringSmall(MenuList[menu_index].name, 0, 0, 2);
            i++;

            while (i < 4)
            {	// trailing menu item - small text
                const int k = menu_index + i - 2;
                if (k >= 0 && k < (int)gMenuListCount)
                    UI_PrintStringSmall(MenuList[k].name, 0, 0, 1 + i);
                else
                if (k >= (int)gMenuListCount)
                    UI_PrintStringSmall(MenuList[gMenuListCount - k].name, 0, 0, 1 + i);  // wrap-a-round
                i++;
            }

            // draw the menu index number/count
            sprintf(String, "%2u.%u", 1 + gMenuCursor, gMenuListCount);
            UI_PrintStringSmall(String, 2, 0, 6);
        }
        else
        if (menu_index >= 0 && menu_index < (int)gMenuListCount)
        {	// current menu item

            UI_PrintStringSmall(MenuList[menu_index].name, 0, 0, 0);
//			UI_PrintStringSmall(String, 0, 0, 0);
        }
    }
#endif

    // **************

    memset(String, 0, sizeof(String));

    bool already_printed = false;
/* Brightness is set to max in some entries of this menu. Return it to the configured brightness
	   level the "next" time we enter here.I.e., when we move from one menu to another.
	   It also has to be set back to max when pressing the Exit key. */

    BACKLIGHT_TurnOn();
    switch (UI_MENU_GetCurrentMenuId()) {
        case MENU_SQL:
            sprintf(String, "%d", gSubMenuSelection);
            break;

        case MENU_MIC: {    // display the mic gain in actual dB rather than just an index number
            const uint8_t mic = gMicGain_dB2[gSubMenuSelection];
            sprintf(String, "+%u.%01udB", mic / 2, mic % 2);
        }
            break;

//#ifdef ENABLE_AUDIO_BAR
//            case MENU_MIC_BAR:
//                strcpy(String, gSubMenu_OFF_ON[gSubMenuSelection]);
//                break;
//#endif

        case MENU_STEP: {
            uint16_t step = gStepFrequencyTable[FREQUENCY_GetStepIdxFromSortedIdx(gSubMenuSelection)];
            sprintf(String, "%d.%02ukHz", step / 100, step % 100);
            break;
        }

//        case MENU_TXP:
//            strncpy(String, gSubMenu_TXP[gSubMenuSelection], sizeof(gSubMenu_TXP[gSubMenuSelection]));
//            String[sizeof(gSubMenu_TXP[gSubMenuSelection])] = '\0';
//
//
//            break;

        case MENU_R_DCS:
        case MENU_T_DCS:
            if (gSubMenuSelection == 0)
                //translate
#ifdef test
                strcpy(String, "OFF");

#else
                strcpy(String, STR_OFF);

#endif


            else if (gSubMenuSelection < 105)
                sprintf(String, "D%03oN", DCS_Options[gSubMenuSelection - 1]);
            else
                sprintf(String, "D%03oI", DCS_Options[gSubMenuSelection - 105]);
            break;

        case MENU_R_CTCS:
        case MENU_T_CTCS: {
            if (gSubMenuSelection == 0)
                // translate
#ifdef test
                strcpy(String, "OFF");

#else
                //关闭
                strcpy(String, STR_OFF);

#endif

            else {
                sprintf(String, "%u.%uHz", CTCSS_Options[gSubMenuSelection - 1] / 10,
                        CTCSS_Options[gSubMenuSelection - 1] % 10);

            }

            break;
        }

        case MENU_SFT_D:
            strncpy(String, gSubMenu_SFT_D[gSubMenuSelection], sizeof(gSubMenu_SFT_D[gSubMenuSelection]));
            String[sizeof(gSubMenu_SFT_D[gSubMenuSelection])] = '\0';
            break;

        case MENU_OFFSET:
            if (!gIsInSubMenu || gInputBoxIndex == 0) {
                sprintf(String, "%3d.%05u", gSubMenuSelection / 100000, abs(gSubMenuSelection) % 100000);
                UI_PrintStringSmall(String, menu_item_x1, menu_item_x2, 2);
            } else {
                const char *ascii = INPUTBOX_GetAscii();
                sprintf(String, "%.3s.%.3s  ", ascii, ascii + 3);
                UI_PrintStringSmall(String, menu_item_x1, menu_item_x2, 2);
            }

            UI_PrintStringSmall("MHz", menu_item_x1, menu_item_x2, 4);

            already_printed = true;
            break;
#ifdef ENABLE_CUSTOM_SIDEFUNCTIONS

            case MENU_W_N:

                strcpy(String, gSubMenu_W_N[gSubMenuSelection]);
                break;
#endif

        case MENU_SCR:
            strcpy(String, gSubMenu_SCRAMBLER[gSubMenuSelection]);

#if 1
            //  if (gSubMenuSelection > 0 && gSetting_ScrambleEnable)
            if (gSubMenuSelection > 0)
                BK4819_EnableScramble(gSubMenuSelection - 1);
            else
                BK4819_DisableScramble();
#endif
            break;


        case MENU_ABR:
            strcpy(String, gSubMenu_BACKLIGHT[gSubMenuSelection]);


//            BACKLIGHT_SetBrightness(-1);
            break;

            // case MENU_ABR_MIN:
        case MENU_ABR_MAX:
            sprintf(String, "%d", gSubMenuSelection);
            if (gIsInSubMenu)
                BACKLIGHT_SetBrightness(gSubMenuSelection);
//            else
//                BACKLIGHT_SetBrightness(-1);
            break;

//        case MENU_AM:
//            strcpy(String, gModulationStr[gSubMenuSelection]);
//
//            break;

#ifdef ENABLE_AM_FIX_TEST1
            case MENU_AM_FIX_TEST1:
                strcpy(String, gSubMenu_AM_fix_test1[gSubMenuSelection]);
//				gSetting_AM_fix = gSubMenuSelection;
                break;
#endif


        case MENU_COMPAND:
            strcpy(String, gSubMenu_RX_TX[gSubMenuSelection]);


            break;

#ifdef ENABLE_AM_FIX
            case MENU_AM_FIX:
#endif
        case MENU_BCL:
            //     case MENU_BEEP:
//        case MENU_S_ADD1:
//        case MENU_S_ADD2:
        case MENU_STE:
        case MENU_D_ST:
#ifdef ENABLE_DTMF_CALLING
#ifdef ENABLE_CUSTOM_SIDEFUNCTIONS

            case MENU_D_DCD:
#endif
#endif
        case MENU_D_LIVE_DEC:
#ifdef ENABLE_NOAA
            case MENU_NOAA_S:
#endif
//        case MENU_350TX:
//        case MENU_200TX:
//        case MENU_500TX:
//        case MENU_350EN:
            strcpy(String, gSubMenu_OFF_ON[gSubMenuSelection]);

            break;
//        case MENU_SCREN:
//            strcpy(String, gSubMenu_OFF_ON[gSubMenuSelection]);
//
//
//            break;

        case MENU_MEM_CH:
        case MENU_1_CALL:
        case MENU_DEL_CH: {
            const bool valid = RADIO_CheckValidChannel(gSubMenuSelection, false, 1);

            UI_GenerateChannelStringEx(String, valid, gSubMenuSelection);
            UI_PrintStringSmall(String, menu_item_x1 - 12, menu_item_x2, 2);

            if (valid && !gAskForConfirmation) {    // show the frequency so that the user knows the channels frequency
                const uint32_t frequency = SETTINGS_FetchChannelFrequency(gSubMenuSelection);
                sprintf(String, "%u.%05u", frequency / 100000, frequency % 100000);
                UI_PrintStringSmall(String, menu_item_x1 - 12, menu_item_x2, 5);
            }
            SETTINGS_FetchChannelName(String, gSubMenuSelection);
#if ENABLE_CHINESE_FULL == 4 && !defined(ENABLE_ENGLISH)
            show_move_flag=1;
#endif
            UI_PrintStringSmall(String[0] ? String : "--", menu_item_x1 - 12, menu_item_x2, 3);
            already_printed = true;
            break;
        }
#ifdef ENABLE_MDC1200
        case MENU_MDC_ID: {
#ifdef ENABLE_MDC1200_EDIT
            if (gIsInSubMenu) {
                // show the channel name being edited
                UI_PrintStringSmall(edit, menu_item_x1, menu_item_x2, 3);
                if (edit_index < 4)
                    UI_PrintStringSmall("^", menu_item_x1 + (((menu_item_x2 - menu_item_x1) - (28)) + 1) / 2 + (7 * edit_index), 0, 4); // show the cursor
            } else {
#endif
                sprintf(String, "%04X", gEeprom.MDC1200_ID); // %04X确保输出是4个字符长度的十六进制数
                UI_PrintStringSmall(String, menu_item_x1, menu_item_x2, 3); //4

#ifdef ENABLE_MDC1200_EDIT

                edit_index = -1;
                edit[0] = String[0];
                edit[1] = String[1];
                edit[2] = String[2];
                edit[3] = String[3];
                edit[4] = '\0';
#endif
#ifdef ENABLE_MDC1200_EDIT
            }
#endif
            already_printed = true;
            break;
        }
#endif
        case MENU_MEM_NAME: { //输入法显示
//ok


            const bool valid = RADIO_CheckValidChannel(gSubMenuSelection, false, 1);
            UI_GenerateChannelStringEx(String, valid, gSubMenuSelection);
            UI_PrintStringSmall(String, menu_item_x1 - 12, menu_item_x2, 2);

            if (valid) {
                const uint32_t frequency = SETTINGS_FetchChannelFrequency(gSubMenuSelection);
                //bug way
                char tmp_name[17] = {0};
                SETTINGS_FetchChannelName(tmp_name, gSubMenuSelection);

                if (!gIsInSubMenu)
                    edit_index = -1;
                if (edit_index < 0) {    // show the channel name
                    SETTINGS_FetchChannelName(String, gSubMenuSelection);
                    const char *pPrintStr = String[0] ? String : "--";
#if ENABLE_CHINESE_FULL == 4 && !defined(ENABLE_ENGLISH)
                    show_move_flag=1;
#endif
                    UI_PrintStringSmall(pPrintStr, menu_item_x1 - 12, menu_item_x2, 3);

                }

//
#if ENABLE_CHINESE_FULL == 4 && !defined(ENABLE_PINYIN) && !defined(ENABLE_ENGLISH)

                else if (CHINESE_JUDGE(tmp_name, strlen(tmp_name))) {
                    edit_index = -1;
                }else if (!CHINESE_JUDGE(tmp_name, strlen(tmp_name))) {    // show the channel name being edited
#else
                else {
#endif


#if ENABLE_CHINESE_FULL == 4 && !defined(ENABLE_ENGLISH)
                    show_move_flag=1;
#endif
                    UI_PrintStringSmall(edit, menu_item_x1 - 12, menu_item_x2, 3);

                    if (edit_index < MAX_EDIT_INDEX) {
//#if ENABLE_CHINESE_FULL == 4
//                        show_move_flag=1;
//#endif

#ifdef ENABLE_PINYIN
                        uint8_t cnt_chn = 0;
                        uint8_t sum_pxl = 0;

                        for (int j = 0; j < MAX_EDIT_INDEX; ++j) {
                            if (edit[j] >= 0xb0 && j != MAX_EDIT_INDEX - 1) {
                                if (j < edit_index) {
                                    sum_pxl += 13;
                                }
                                edit_chn[j] = 1;
                                j++;
                                edit_chn[j] = 2;
                                cnt_chn++;
                            } else {
                                if (j < edit_index) {
                                    sum_pxl += 7;
                                }
                                edit_chn[j] = 0;
                            }
                        }

                        uint8_t add_point = 3 << edit_chn[edit_index];

                        uint8_t pointY = menu_item_x1 - 12 + sum_pxl +
                                        (((menu_item_x2 - menu_item_x1 + 12) -
                                          (7 * (MAX_EDIT_INDEX - 2 * cnt_chn) + 13 * cnt_chn)) + 1) / 2 + add_point;

                        gFrameBuffer[4][pointY] |= 3 << 5;
                        gFrameBuffer[4][pointY + 1] |= 3 << 5;
#else

                        gFrameBuffer[4][menu_item_x1 - 12 + 7 * edit_index +
                                        (((menu_item_x2 - menu_item_x1 + 12) - (7 * MAX_EDIT_INDEX)) + 1) / 2 + 3] |=
                                3 << 6;
                        gFrameBuffer[4][menu_item_x1 - 12 + 7 * edit_index +
                                        (((menu_item_x2 - menu_item_x1 + 12) - (7 * MAX_EDIT_INDEX)) + 1) / 2 + 4] |=
                                3 << 6;

#endif

#ifdef ENABLE_PINYIN //拼音显示
                        //OK
                        if (INPUT_MODE == 0)memcpy(&gFrameBuffer[3][0], BITMAP_CN, 7);
                        else if (INPUT_MODE == 1) UI_PrintStringSmall("A", 0, 0, 3);
                        else if (INPUT_MODE == 2) UI_PrintStringSmall("1", 0, 0, 3);
                        else if (INPUT_MODE == 3) UI_PrintStringSmall("*", 0, 0, 3);
                        if (INPUT_MODE == 0) {
                            sprintf(String, "%06d", PINYIN_CODE);
                            GUI_DisplaySmallest(String, 0, 18, 0, 1);
                            uint8_t tmp[12];


                            if (INPUT_STAGE >= 1)//显示拼音
                            {
                                if (PINYIN_SEARCH_MODE == 1)//准确的组合
                                {
                                    uint8_t num = (PINYIN_NUM_SELECT) / 3;
                                    if ((PINYIN_NOW_NUM + 2) / 3 > 1 + num)memcpy(&gFrameBuffer[1][123], BITMAP_ARRAY_DOWN, 5);
                                    if (num)memcpy(&gFrameBuffer[0][123], BITMAP_ARRAY_UP, 5);

                                    uint8_t HAVE_PINYIN = PINYIN_NOW_NUM - num * 3 > 3 ? 3 : PINYIN_NOW_NUM - num * 3;
                                    for (int j = 0; j < HAVE_PINYIN; ++j) {
                                        PINYIN_Read(
                                                PINYIN_NOW_INDEX * 128 + 0X20000 + 16 + num * 3 * 16 +
                                                j * 16, tmp, 6);
                                        memcpy(&String[6 * j], tmp, 6);
                                    }
                                    String[6 * HAVE_PINYIN] = 0;
                                    UI_PrintStringSmall(String, 0, 0, 0);
                                } else if (PINYIN_SEARCH_MODE == 2) {
                                    uint16_t total = PINYIN_CountRange(PINYIN_START_INDEX, PINYIN_END_INDEX);
                                    if (total) {
                                        uint16_t flat_index = PINYIN_FlatIndex(PINYIN_START_INDEX, PINYIN_NOW_INDEX, PINYIN_NUM_SELECT);
                                        uint16_t page = flat_index / 3;
                                        uint16_t page_start = page * 3;
                                        uint16_t total_pages = (total + 2) / 3;
                                        uint8_t HAVE_PINYIN = total - page_start > 3 ? 3 : (uint8_t)(total - page_start);
                                        if (page + 1 < total_pages)memcpy(&gFrameBuffer[1][123], BITMAP_ARRAY_DOWN, 5);
                                        if (page)memcpy(&gFrameBuffer[0][123], BITMAP_ARRAY_UP, 5);

                                        for (uint8_t j = 0; j < HAVE_PINYIN; ++j) {
                                            uint8_t entry_index = 0;
                                            uint8_t entry_offset = 0;
                                            if (PINYIN_GetFlatEntry(PINYIN_START_INDEX, PINYIN_END_INDEX,
                                                                    page_start + j, &entry_index, &entry_offset)) {
                                                PINYIN_Read(
                                                        entry_index * 128 + 0X20000 + 16 + entry_offset * 16,
                                                        tmp, 6);
                                                memcpy(&String[6 * j], tmp, 6);
                                            }
                                        }
                                        String[6 * HAVE_PINYIN] = 0;
                                        UI_PrintStringSmall(String, 0, 0, 0);
                                    }
                                }
                            }
                            if (INPUT_STAGE == 2) {

                                if (PINYIN_SEARCH_MODE == 1 || PINYIN_SEARCH_MODE == 2)//准确的组合
                                {
                                    uint8_t pos = PINYIN_NUM_SELECT % 3;
                                    if (PINYIN_SEARCH_MODE == 2) {
                                        uint16_t flat_index = PINYIN_FlatIndex(PINYIN_START_INDEX, PINYIN_NOW_INDEX, PINYIN_NUM_SELECT);
                                        pos = flat_index % 3;
                                    }
                                    memcpy(&gFrameBuffer[1][pos * 7 * 6], BITMAP_ARRAY_UP, 5);

                                    uint8_t SHOW_NUM =
                                            CHN_NOW_NUM - CHN_NOW_PAGE * 6 > 6 ? 6 : CHN_NOW_NUM - CHN_NOW_PAGE * 6;
                                    PINYIN_Read(CHN_NOW_ADD + CHN_NOW_PAGE * 6 * 2, tmp, SHOW_NUM * 2);
//                                    show_uint32(PINYIN_NOW_INDEX * 128 + 0X20000 + 16 + PINYIN_NUM_SELECT * 16 + 6, 5);
                                    for (int j = 0; j < SHOW_NUM; ++j) {
                                        String[j * 3] = '0' + j + 1;
                                        String[j * 3 + 1] = tmp[j * 2];
                                        String[j * 3 + 2] = tmp[j * 2 + 1];
                                    }
                                    String[SHOW_NUM * 3] = 0;
                                    show_move_flag = 1;

                                    UI_PrintStringSmall(String, 0, 0, 5);
                                    if (CHN_NOW_PAGE) memcpy(&gFrameBuffer[5][123], BITMAP_ARRAY_UP, 5);
                                    if ((CHN_NOW_PAGE + 1) * 6 < CHN_NOW_NUM)
                                        memcpy(&gFrameBuffer[6][123], BITMAP_ARRAY_DOWN, 5);
                                }
                            }
//NOT OK


                        } else if (INPUT_MODE == 1) {
                            if (INPUT_STAGE == 1) {
                                char tmp[22] = {0};
                                if (num_size[INPUT_SELECT - 2] == 3) {
                                    sprintf(tmp, "1.%c 2.%c 3.%c", num_excel[INPUT_SELECT - 2][0],
                                            num_excel[INPUT_SELECT - 2][1], num_excel[INPUT_SELECT - 2][2]);
                                    UI_PrintStringSmall(tmp, 0, 127, 0);
                                    tmp[0] = '4', tmp[4] = '5', tmp[8] = '6';//flash?
                                    tmp[2] -= 32, tmp[6] -= 32, tmp[10] -= 32;

                                } else {
                                    sprintf(tmp, "1.%c 2.%c 3.%c 4.%c", num_excel[INPUT_SELECT - 2][0],
                                            num_excel[INPUT_SELECT - 2][1], num_excel[INPUT_SELECT - 2][2],
                                            num_excel[INPUT_SELECT - 2][3]);
                                    UI_PrintStringSmall(tmp, 0, 127, 0);

                                    tmp[0] ='5', tmp[4] = '6', tmp[8] = '7', tmp[12] = '8';
                                    tmp[2] -= 32, tmp[6] -= 32, tmp[10] -= 32, tmp[14] -= 32;
                                }
                                UI_PrintStringSmall(tmp, 0, 127, 1);

                            }
                        }
#endif

                    }
                }

//NOT OK
//                sprintf(String, "%d", edit_index);
//                UI_PrintStringSmall(String, 0, 0, 1);
//                sprintf(String, "%d", gIsInSubMenu);
//                UI_PrintStringSmall(String, 20, 0, 1);
//
//                sprintf(String,"%d",edit[2]);
//                UI_PrintStringSmall(String, 0, 0, 3);


                if (!gAskForConfirmation) {    // show the frequency so that the user knows the channels frequency
                    sprintf(String, "%u.%05u", frequency / 100000, frequency % 100000);
#ifdef ENABLE_PINYIN

                    if (edit_index == MAX_EDIT_INDEX - 1 && INPUT_MODE == 0)
                        INPUT_MODE = 1;
                    if (!(gIsInSubMenu && edit_index >= 0))

#endif
                    {
//                        show_move_flag = 1;
                        UI_PrintStringSmall(String, menu_item_x1 - 12, menu_item_x2, 5);
                    }
                }
            }

            already_printed = true;

            break;
        }

        case MENU_SAVE:
            strcpy(String, gSubMenu_SAVE[gSubMenuSelection]);


            break;

        case MENU_TDR:
            strcpy(String, gSubMenu_RXMode[gSubMenuSelection]);


            break;

        case MENU_TOT:
            strcpy(String, gSubMenu_TOT[gSubMenuSelection]);


            break;

#ifdef ENABLE_VOICE
            case MENU_VOICE:
                strcpy(String, gSubMenu_VOICE[gSubMenuSelection]);
                break;
#endif

        case MENU_SC_REV:


            strcpy(String, gSubMenu_SC_REV[gSubMenuSelection]);


            break;

        case MENU_MDF:


            strcpy(String, gSubMenu_MDF[gSubMenuSelection]);


            break;

        case MENU_RP_STE:
            if (gSubMenuSelection == 0)
//translate
#ifdef test
                strcpy(String, "OFF");

#else
                //关闭
                strcpy(String, STR_OFF);

#endif


            else
                sprintf(String, "%d*100ms", gSubMenuSelection);
            break;

        case MENU_S_LIST:
            if (gSubMenuSelection < 2)

                //translate

#ifdef test
                sprintf(String, "list %u", 1 + gSubMenuSelection);

#else  //！！列表
                sprintf(String, STR_LIST" %u", 1 + gSubMenuSelection);

#endif

            else

#ifdef test
                strcpy(String, "ALL");

#else
                //全部
                strcpy(String, STR_ALL);

#endif
            break;

#ifdef ENABLE_ALARM
            case MENU_AL_MOD:
                sprintf(String, gSubMenu_AL_MOD[gSubMenuSelection]);
                break;
#endif
#ifdef ENABLE_DTMF_CALLING
            case MENU_ANI_ID:

                strcpy(String, gEeprom.ANI_DTMF_ID);
                break;
#endif

        case MENU_UPCODE:

            sprintf(String, "%.8s\n%.8s", gEeprom.DTMF_UP_CODE, gEeprom.DTMF_UP_CODE + 8);
            break;

        case MENU_DWCODE:
            sprintf(String, "%.8s\n%.8s", gEeprom.DTMF_DOWN_CODE, gEeprom.DTMF_DOWN_CODE + 8);
            break;
#ifdef ENABLE_DTMF_CALLING
            case MENU_D_RSP:
                strcpy(String, gSubMenu_D_RSP[gSubMenuSelection]);


                break;

            case MENU_D_HOLD:
                sprintf(String, "%ds", gSubMenuSelection);
                break;
#endif
        case MENU_D_PRE:
            sprintf(String, "%d*10ms", gSubMenuSelection);
            break;

        case MENU_PTT_ID:
            strcpy(String, gSubMenu_PTT_ID[gSubMenuSelection]);


            break;

//        case MENU_BAT_TXT:
//            strcpy(String, gSubMenu_BAT_TXT[gSubMenuSelection]);
//
//
//            break;
#ifdef ENABLE_DTMF_CALLING
            case MENU_D_LIST:
                gIsDtmfContactValid = DTMF_GetContact((int) gSubMenuSelection - 1, Contact);
                if (!gIsDtmfContactValid)
                    strcpy(String, "NULL");
                else
                    memcpy(String, Contact, 8);
                break;
#endif
#if ENABLE_CHINESE_FULL == 4

            case MENU_PONMSG:
                strcpy(String, gSubMenu_PONMSG[gSubMenuSelection]);
                break;
#endif
        case MENU_ROGER:
            strcpy(String, gSubMenu_ROGER[gSubMenuSelection]);


            break;

//        case MENU_VOL:
//            sprintf(String, "%u.%02uV\n%u%%",
//                    gBatteryVoltageAverage / 100, gBatteryVoltageAverage % 100,
//                    BATTERY_VoltsToPercent(gBatteryVoltageAverage));
//            break;

        case MENU_RESET:
            strcpy(String, gSubMenu_RESET[gSubMenuSelection]);


            break;

        case MENU_F_LOCK:
//            if (!gIsInSubMenu && gUnlockAllTxConfCnt > 0&& gUnlockAllTxConfCnt < 10)
//                strcpy(String, "READ\nMANUAL");
//
//            else
            strcpy(String, gSubMenu_F_LOCK[gSubMenuSelection]);


            break;

#ifdef ENABLE_F_CAL_MENU
            case MENU_F_CALI:
                {
                    const uint32_t value   = 22656 + gSubMenuSelection;
                    const uint32_t xtal_Hz = (0x4f0000u + value) * 5;

                    writeXtalFreqCal(gSubMenuSelection, false);

                    sprintf(String, "%d\n%u.%06u\nMHz",
                        gSubMenuSelection,
                        xtal_Hz / 1000000, xtal_Hz % 1000000);
                }
                break;
#endif

        case MENU_BATCAL: {
            const uint16_t vol = (uint32_t) gBatteryVoltageAverage * gBatteryCalibration[3] / gSubMenuSelection;
            sprintf(String, "%u.%02uV\n%u", vol / 100, vol % 100, gSubMenuSelection);
            break;
        }

        case MENU_BATTYP:
            strcpy(String, gSubMenu_BATTYP[gSubMenuSelection]);

            break;

#ifdef ENABLE_CUSTOM_SIDEFUNCTIONS
            case MENU_F1SHRT:
            case MENU_F1LONG:
            case MENU_F2SHRT:
            case MENU_F2LONG:
            case MENU_MLONG:
                strcpy(String, gSubMenu_SIDEFUNCTIONS[gSubMenuSelection].name);
                break;
#endif

    }

    if (!already_printed) {    // we now do multi-line text in a single string

        unsigned int y;
        unsigned int lines = 1;
        unsigned int len = strlen(String);
        bool small = false;

        if (len > 0) {
            // count number of lines
            for (i = 0; i < len; i++) {
                if (String[i] == '\n' && i < (len - 1)) {    // found new line char
                    lines += 1;
                    String[i] = 0;  // null terminate the line
                }
            }

            if (lines > 3) {    // use small text
                small = true;
                if (lines > 7)
                    lines = 7;
            }

            // center vertically'ish
            if (small)
                y = 3 - ((lines + 0) / 2);  // untested
            else
                y = 2 - ((lines + 0) / 2);

            // draw the text lines
            for (i = 0; i < len && lines > 0; lines--) {
                if (small)
                    UI_PrintStringSmall(String + i, menu_item_x1, menu_item_x2, y + 1);
                else
                    UI_PrintStringSmall(String + i, menu_item_x1, menu_item_x2, y + 1);

                // look for start of next line
                while (i < len && String[i] != 0 && String[i] != '\n')
                    i++;

                // hop over the null term char(s)
                while (i < len && (String[i] == 0 || String[i] == '\n'))
                    i++;

                y += small ? 1 : 2;
            }
        }
    }

    if (UI_MENU_GetCurrentMenuId() == MENU_SLIST1 || UI_MENU_GetCurrentMenuId() == MENU_SLIST2) {
        i = (UI_MENU_GetCurrentMenuId() == MENU_SLIST1) ? 0 : 1;

        const char *pPrintStr = String;

        if (gSubMenuSelection < 0) {
            pPrintStr = "NULL";
        } else {
            UI_GenerateChannelStringEx(String, true, gSubMenuSelection);
            pPrintStr = String;
        }

        // channel number
        UI_PrintStringSmall(pPrintStr, menu_item_x1 - 12, menu_item_x2, 2);

#if ENABLE_CHINESE_FULL == 4 && !defined(ENABLE_ENGLISH)
        show_move_flag=1;
#endif
        SETTINGS_FetchChannelName(String, gSubMenuSelection);
        pPrintStr = String[0] ? String : "--";


// channel name and scan-list
        if (gSubMenuSelection < 0 || !gEeprom.SCAN_LIST_ENABLED[i]) {
            UI_PrintStringSmall(pPrintStr, menu_item_x1 - 12, menu_item_x2, 4);
        } else {
            UI_PrintStringSmall(pPrintStr, menu_item_x1 - 12, menu_item_x2, 4);

//
//            if (IS_MR_CHANNEL(gEeprom.SCANLIST_PRIORITY_CH1[i])) {
//                sprintf(String, "PRI%d:%u", 1, gEeprom.SCANLIST_PRIORITY_CH1[i] + 1);
//                UI_PrintStringSmall(String, menu_item_x1 - 12, menu_item_x2, 3);
//            }
//
//            if (IS_MR_CHANNEL(gEeprom.SCANLIST_PRIORITY_CH2[i])) {
//                sprintf(String, "PRI%d:%u", 2, gEeprom.SCANLIST_PRIORITY_CH2[i] + 1);
//                UI_PrintStringSmall(String, menu_item_x1 - 12, menu_item_x2, 6);
//            }

        }
    }


    if ((UI_MENU_GetCurrentMenuId() == MENU_R_CTCS || UI_MENU_GetCurrentMenuId() == MENU_R_DCS) && gCssBackgroundScan)
        //扫描
        UI_PrintStringSmall(STR_SCAN, menu_item_x1, menu_item_x2, 5);

//
//    if (UI_MENU_GetCurrentMenuId() == MENU_UPCODE)
//        if (strlen(gEeprom.DTMF_UP_CODE) > 12)
//            UI_PrintStringSmall(gEeprom.DTMF_UP_CODE + 12, menu_item_x1, menu_item_x2, 5);
//
//    if (UI_MENU_GetCurrentMenuId() == MENU_DWCODE)
//        if (strlen(gEeprom.DTMF_DOWN_CODE) > 12)
//            UI_PrintStringSmall(gEeprom.DTMF_DOWN_CODE + 12, menu_item_x1, menu_item_x2, 5);
#ifdef ENABLE_DTMF_CALLING
    if (UI_MENU_GetCurrentMenuId() == MENU_D_LIST && gIsDtmfContactValid) {

        Contact[11] = 0;
        memcpy(&gDTMF_ID, Contact + 8, 4);
        sprintf(String, "ID:%4s", gDTMF_ID);
        UI_PrintStringSmall(String, menu_item_x1, menu_item_x2, 5);
    }
#endif
    if (UI_MENU_GetCurrentMenuId() == MENU_R_CTCS ||
        UI_MENU_GetCurrentMenuId() == MENU_T_CTCS ||
        UI_MENU_GetCurrentMenuId() == MENU_R_DCS ||
        UI_MENU_GetCurrentMenuId() == MENU_T_DCS
#ifdef ENABLE_DTMF_CALLING
        || UI_MENU_GetCurrentMenuId() == MENU_D_LIST
#endif
            ) {

        sprintf(String, "%2d", gSubMenuSelection);
        UI_PrintStringSmall(String, 105, 0, 1);//small
    }

    if ((UI_MENU_GetCurrentMenuId() == MENU_RESET ||
         UI_MENU_GetCurrentMenuId() == MENU_MEM_CH ||
         UI_MENU_GetCurrentMenuId() == MENU_MEM_NAME ||
         #ifdef ENABLE_MDC1200_EDIT

         UI_MENU_GetCurrentMenuId() == MENU_MDC_ID ||
         #endif
         UI_MENU_GetCurrentMenuId() == MENU_DEL_CH) && gAskForConfirmation) {    // display confirmation
        const char *pPrintStr = (gAskForConfirmation == 1) ? "SURE?" : "WAIT!";
        if (UI_MENU_GetCurrentMenuId() == MENU_MEM_CH || UI_MENU_GetCurrentMenuId() == MENU_MEM_NAME ||
             UI_MENU_GetCurrentMenuId() == MENU_DEL_CH)
            UI_PrintStringSmall(pPrintStr, menu_item_x1 - 12, menu_item_x2, 5);
        else UI_PrintStringSmall(pPrintStr, menu_item_x1, menu_item_x2, 5);

        gRequestSaveSettings = 1;

    }

    ST7565_BlitFullScreen();
}

//

#ifndef ENABLE_ENGLISH
void UI_ShowChineseMenu() {


    uint8_t size_menu = 0;
    uint8_t cnt_menu = 0;


    for (cnt_menu = 0; cnt_menu < 7 && MenuList[gMenuCursor].name[cnt_menu] != 0; cnt_menu++) {
        if (is_chn(MenuList[gMenuCursor].name[cnt_menu]) != 255)//中文
        {
            size_menu += 12;
#if ENABLE_CHINESE_FULL != 0
            cnt_menu++;
#endif
        } else//英文
        {
            size_menu += 7;
        }
    }

    show_move_flag = 1;

    UI_PrintStringSmall(MenuList[gMenuCursor].name, size_menu < 48 ? (48 - size_menu) / 2 : 0, 0, 0);

}
#endif
#ifdef ENABLE_PINYIN
uint8_t INPUT_SELECT = 0;//选择的按键
uint8_t INPUT_MODE_LAST = 0;
uint8_t INPUT_MODE = 0;//0中文 1英文 2数字、符号
uint8_t INPUT_STAGE = 0;//中文：0 还没输入，不显示拼音和汉字 1输入了
uint32_t PINYIN_CODE = 0;
uint32_t PINYIN_CODE_INDEX = 100000;
uint8_t PINYIN_SEARCH_INDEX = 0;
uint8_t PINYIN_SEARCH_FOUND = 0;
uint8_t PINYIN_SEARCH_NUM = 0;
uint8_t PINYIN_NOW_INDEX = 0;//当前拼音组合地址
uint8_t PINYIN_NOW_NUM = 0;
uint8_t PINYIN_SEARCH_MODE = 0;
uint8_t PINYIN_START_INDEX = 0;
uint8_t PINYIN_END_INDEX = 0;
uint8_t PINYIN_NOW_PAGE = 0;
uint8_t PINYIN_NUM_SELECT = 0;
uint32_t CHN_NOW_ADD = 0;
uint8_t CHN_NOW_NUM = 0;
uint8_t CHN_NOW_PAGE = 0;
uint8_t edit_chn[MAX_EDIT_INDEX];
//英语：0 未选字 1选字
//数字：0正常模式 1按了上下的轮询模式，需要按MENU确定
char input1[22];
char input2[22];
char num_excel[8][4] = {
        {'a','b','c','\0'},
        {'d','e','f','\0'},
        {'g','h','i','\0'},
        {'j','k','l','\0'},
        {'m','n','o','\0'},
        {'p','q','r','s'},
        {'t','u','v','\0'},
        {'w','x','y','z'},

};
uint8_t num_size[8]={3,3,3,3,3,4,3,4};

static uint8_t PINYIN_ReadCount(uint8_t index)
{
    uint8_t tmp[5];

    PINYIN_Read(index * 128 + 0x20000, tmp, 5);
    return tmp[4];
}

static uint16_t PINYIN_CountRange(uint8_t start, uint8_t end)
{
    uint16_t total = 0;

    if (end < start) {
        return 0;
    }
    for (uint8_t i = start; i <= end; ++i) {
        total += PINYIN_ReadCount(i);
    }
    return total;
}

static uint16_t PINYIN_FlatIndex(uint8_t start, uint8_t index, uint8_t offset)
{
    uint16_t total = 0;

    if (index < start) {
        return 0;
    }
    for (uint8_t i = start; i < index; ++i) {
        total += PINYIN_ReadCount(i);
    }
    total += offset;
    return total;
}

static bool PINYIN_GetFlatEntry(uint8_t start, uint8_t end, uint16_t flat_index, uint8_t *entry_index, uint8_t *entry_offset)
{
    if (end < start) {
        return false;
    }
    for (uint8_t i = start; i <= end; ++i) {
        uint8_t count = PINYIN_ReadCount(i);
        if (flat_index < count) {
            if (entry_index != NULL) {
                *entry_index = i;
            }
            if (entry_offset != NULL) {
                *entry_offset = (uint8_t)flat_index;
            }
            return true;
        }
        flat_index -= count;
    }
    return false;
}

uint32_t formatInt(uint32_t number) {//数字转拼音编码
    uint32_t formatted = number;
    uint32_t length = 0;
    // 计算整数的位数
    while (number != 0) {
        number /= 10;
        length++;
    }
    // 如果位数不足6位，则在后面补0
    if (length < 6) {
        for (uint8_t i = 0; i < 6 - length; ++i) {
            formatted *= 10;
        }
    }
    return formatted;
}


uint32_t get_num(const char *a) {//拼音转数字
    uint32_t num = 0;
    uint32_t bin = 100000;
    for (unsigned int j = 0; j < strlen(a); j++) {
        uint32_t now_num = 0;
        for (int i = 0; i < 8; ++i) {
            for (int k = 0; k < num_size[i]; ++k) {
                if (num_excel[i][k] == a[j]) {
                    now_num = i + 2;
                    goto end_loop;
                }
            }
        }
        end_loop:
        num += bin * now_num;
        bin /= 10;
    }
    return num;
}

bool judge_belong(uint32_t a, uint32_t b)//拼音归属判断
{
    for (uint32_t i = 100000; i >= 1; i /= 10) {
        if (a / i == 0)break;
        if (a / i != b / i)return false;
        a = a - a / i * i;
        b = b - b / i * i;

    }
    return true;
}

uint8_t sear_pinyin_code(uint32_t target, uint8_t *pinyin_num, uint8_t *found)//返回拼音索引0~213，以及是否找到
{
    int left = 0;
    int right = 213;
    *found = 0; // 初始设定未找到

    while (left <= right) {
        int mid = left + (right - left) / 2;
        uint8_t tmp[5];
        PINYIN_Read(mid * 128 + 0x20000, tmp, 5);
        uint32_t mid_num = tmp[0] | tmp[1] << 8 | tmp[2] << 16 | tmp[3] << 24;
        *pinyin_num = tmp[4];
        if (mid_num == target) {
            *found = 1; // 找到了
            return mid;
        } else if (target < mid_num) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }

    // 找不到目标值，返回比目标值大一个的值
    if (left <= 213) {
        uint8_t tmp[5];
        PINYIN_Read(left * 128 + 0x20000, tmp, 5);
        uint32_t left_num = tmp[0] | tmp[1] << 8 | tmp[2] << 16 | tmp[3] << 24;


        if (judge_belong(target, left_num)) {
            *pinyin_num = tmp[4];
            return left;
        }
    }
    return 255;

}

#endif
