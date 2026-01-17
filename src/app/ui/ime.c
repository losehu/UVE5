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

#include <stdio.h>
#include <string.h>

#include "../bitmaps.h"
#include "../driver/st7565.h"
#include "../font.h"
#include "../misc.h"
#include "../pinyin_blob.h"
#include "helper.h"
#include "menu.h"
#include "ui.h"
#include "ime.h"

#ifdef ENABLE_PINYIN
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
#endif

void UI_DisplayIme(void) {
    const unsigned int menu_item_x1 = 12;
    const unsigned int menu_item_x2 = LCD_WIDTH - 1;
    char String[64];

    UI_DisplayClear();
    UI_PrintStringSmall(edit, menu_item_x1 - 12, menu_item_x2, 3);

    if (edit_index < MAX_EDIT_INDEX) {
#ifdef ENABLE_PINYIN
        uint8_t cnt_chn = 0;
        uint8_t sum_pxl = 0;

        for (int j = 0; j < MAX_EDIT_INDEX; ++j) {
            if ((uint8_t)edit[j] >= 0xb0 && j != MAX_EDIT_INDEX - 1) {
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

#ifdef ENABLE_PINYIN
        if (INPUT_MODE == 0)memcpy(&gFrameBuffer[3][0], BITMAP_CN, 7);
        else if (INPUT_MODE == 1) UI_PrintStringSmall("A", 0, 0, 3);
        else if (INPUT_MODE == 2) UI_PrintStringSmall("1", 0, 0, 3);
        else if (INPUT_MODE == 3) UI_PrintStringSmall("*", 0, 0, 3);
        if (INPUT_MODE == 0) {
            sprintf(String, "%06d", PINYIN_CODE);
            GUI_DisplaySmallest(String, 0, 18, 0, 1);
            uint8_t tmp[12];

            if (INPUT_STAGE >= 1) {
                if (PINYIN_SEARCH_MODE == 1) {
                    uint8_t num = PINYIN_NUM_SELECT / 6;
                    if ((PINYIN_NOW_NUM + 5) / 6 > 1 + num)memcpy(&gFrameBuffer[1][123], BITMAP_ARRAY_DOWN, 5);
                    if (num)memcpy(&gFrameBuffer[0][123], BITMAP_ARRAY_UP, 5);

                    uint8_t HAVE_PINYIN = PINYIN_NOW_NUM - num * 6 > 6 ? 6 : PINYIN_NOW_NUM - num * 6;
                    char line0[19] = {0};
                    char line1[19] = {0};
                    uint8_t have_row0 = HAVE_PINYIN > 3 ? 3 : HAVE_PINYIN;
                    uint8_t have_row1 = HAVE_PINYIN > 3 ? HAVE_PINYIN - 3 : 0;
                    for (int j = 0; j < HAVE_PINYIN; ++j) {
                        PINYIN_Read(
                                PINYIN_NOW_INDEX * 128 + 0X20000 + 16 + num * 6 * 16 +
                                j * 16, tmp, 6);
                        if (j < 3) {
                            memcpy(&line0[6 * j], tmp, 6);
                        } else {
                            memcpy(&line1[6 * (j - 3)], tmp, 6);
                        }
                    }
                    if (have_row0 > 0) {
                        line0[6 * have_row0] = 0;
                        UI_PrintStringSmall(line0, 0, 0, 0);
                    }
                    if (have_row1 > 0) {
                        line1[6 * have_row1] = 0;
                        UI_PrintStringSmall(line1, 0, 0, 1);
                    }
                } else if (PINYIN_SEARCH_MODE == 2) {
                    uint16_t total = PINYIN_CountRange(PINYIN_START_INDEX, PINYIN_END_INDEX);
                    if (total) {
                        uint16_t flat_index = PINYIN_FlatIndex(PINYIN_START_INDEX, PINYIN_NOW_INDEX, PINYIN_NUM_SELECT);
                        uint16_t page = flat_index / 6;
                        uint16_t page_start = page * 6;
                        uint16_t total_pages = (total + 5) / 6;
                        uint8_t HAVE_PINYIN = total - page_start > 6 ? 6 : (uint8_t)(total - page_start);
                        if (page + 1 < total_pages)memcpy(&gFrameBuffer[1][123], BITMAP_ARRAY_DOWN, 5);
                        if (page)memcpy(&gFrameBuffer[0][123], BITMAP_ARRAY_UP, 5);

                        char line0[19] = {0};
                        char line1[19] = {0};
                        uint8_t have_row0 = HAVE_PINYIN > 3 ? 3 : HAVE_PINYIN;
                        uint8_t have_row1 = HAVE_PINYIN > 3 ? HAVE_PINYIN - 3 : 0;
                        for (uint8_t j = 0; j < HAVE_PINYIN; ++j) {
                            uint8_t entry_index = 0;
                            uint8_t entry_offset = 0;
                            if (PINYIN_GetFlatEntry(PINYIN_START_INDEX, PINYIN_END_INDEX,
                                                    page_start + j, &entry_index, &entry_offset)) {
                                PINYIN_Read(
                                        entry_index * 128 + 0X20000 + 16 + entry_offset * 16,
                                        tmp, 6);
                                if (j < 3) {
                                    memcpy(&line0[6 * j], tmp, 6);
                                } else {
                                    memcpy(&line1[6 * (j - 3)], tmp, 6);
                                }
                            }
                        }
                        if (have_row0 > 0) {
                            line0[6 * have_row0] = 0;
                            UI_PrintStringSmall(line0, 0, 0, 0);
                        }
                        if (have_row1 > 0) {
                            line1[6 * have_row1] = 0;
                            UI_PrintStringSmall(line1, 0, 0, 1);
                        }
                    }
                }
            }
            if (INPUT_STAGE == 2) {
                if (PINYIN_SEARCH_MODE == 1 || PINYIN_SEARCH_MODE == 2) {
                    if (PINYIN_SEARCH_MODE == 2) {
                        uint16_t flat_index = PINYIN_FlatIndex(PINYIN_START_INDEX, PINYIN_NOW_INDEX, PINYIN_NUM_SELECT);
                        uint8_t pos = flat_index % 6;
                        uint8_t row = pos / 3;
                        uint8_t col = pos % 3;
                        UI_InvertBlock(row, col * 7 * 6, 7 * 6);
                    } else {
                        uint8_t pos = PINYIN_NUM_SELECT % 6;
                        uint8_t row = pos / 3;
                        uint8_t col = pos % 3;
                        UI_InvertBlock(row, col * 7 * 6, 7 * 6);
                    }

                    uint8_t SHOW_NUM =
                            CHN_NOW_NUM - CHN_NOW_PAGE * 6 > 6 ? 6 : CHN_NOW_NUM - CHN_NOW_PAGE * 6;
                    PINYIN_Read(CHN_NOW_ADD + CHN_NOW_PAGE * 6 * 2, tmp, SHOW_NUM * 2);
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
        } else if (INPUT_MODE == 1) {
            if (INPUT_STAGE == 1) {
                char tmp[22] = {0};
                if (num_size[INPUT_SELECT - 2] == 3) {
                    sprintf(tmp, "1.%c 2.%c 3.%c", num_excel[INPUT_SELECT - 2][0],
                            num_excel[INPUT_SELECT - 2][1], num_excel[INPUT_SELECT - 2][2]);
                    UI_PrintStringSmall(tmp, 0, 127, 0);
                    tmp[0] = '4', tmp[4] = '5', tmp[8] = '6';
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

    ST7565_BlitFullScreen();
}
