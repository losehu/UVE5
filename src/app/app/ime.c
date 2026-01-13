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

#include <string.h>

#include "ime.h"
#include "../audio.h"
#include "../font.h"
#include "../misc.h"
#include "../pinyin_blob.h"
#include "../ui/helper.h"
#include "../ui/menu.h"
#include "../ui/ui.h"

char edit_original[17];
char edit[17];
int edit_index;

#ifdef ENABLE_PINYIN
uint8_t INPUT_SELECT = 0;
uint8_t INPUT_MODE_LAST = 0;
uint8_t INPUT_MODE = 0;
uint8_t INPUT_STAGE = 0;
uint32_t PINYIN_CODE = 0;
uint32_t PINYIN_CODE_INDEX = 100000;
uint8_t PINYIN_SEARCH_INDEX = 0;
uint8_t PINYIN_SEARCH_FOUND = 0;
uint8_t PINYIN_SEARCH_NUM = 0;
uint8_t PINYIN_NOW_INDEX = 0;
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
#endif

static bool gImeActive = false;
static char *gImeTarget = NULL;
static uint8_t gImeTargetLen = 0;
static IME_DoneCallback gImeCallback = NULL;
static void *gImeCallbackCtx = NULL;
static GUI_DisplayType_t gImeReturnDisplay = DISPLAY_MENU;

#ifdef ENABLE_PINYIN
static void PINYIN_ReadHeader(uint8_t index, uint32_t *code, uint8_t *num)
{
    uint8_t tmp[5];

    PINYIN_Read(index * 128 + 0x20000, tmp, 5);
    if (code != NULL) {
        *code = (uint32_t)tmp[0] | ((uint32_t)tmp[1] << 8) | ((uint32_t)tmp[2] << 16) | ((uint32_t)tmp[3] << 24);
    }
    if (num != NULL) {
        *num = tmp[4];
    }
}

static uint8_t PINYIN_ReadCount(uint8_t index)
{
    uint8_t num = 0;

    PINYIN_ReadHeader(index, NULL, &num);
    return num;
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

static void PINYIN_SOLVE(uint32_t tmp) {
    if (INPUT_STAGE == 0) {
        INPUT_STAGE = 1;
    }
    uint8_t prev_search_index = PINYIN_SEARCH_INDEX;
    uint8_t prev_search_num = PINYIN_SEARCH_NUM;
    uint8_t prev_search_found = PINYIN_SEARCH_FOUND;
    uint8_t prev_search_mode = PINYIN_SEARCH_MODE;
    uint8_t prev_now_index = PINYIN_NOW_INDEX;
    uint8_t prev_now_num = PINYIN_NOW_NUM;
    uint8_t prev_start_index = PINYIN_START_INDEX;
    uint8_t prev_end_index = PINYIN_END_INDEX;
    uint8_t prev_num_select = PINYIN_NUM_SELECT;

    PINYIN_SEARCH_INDEX = sear_pinyin_code(PINYIN_CODE, &PINYIN_SEARCH_NUM, &PINYIN_SEARCH_FOUND);

    if (PINYIN_SEARCH_INDEX == 255 && PINYIN_SEARCH_FOUND == 0) {
        PINYIN_CODE = tmp;
        PINYIN_CODE_INDEX *= 10;
        PINYIN_SEARCH_INDEX = prev_search_index;
        PINYIN_SEARCH_NUM = prev_search_num;
        PINYIN_SEARCH_FOUND = prev_search_found;
        PINYIN_SEARCH_MODE = prev_search_mode;
        PINYIN_NOW_INDEX = prev_now_index;
        PINYIN_NOW_NUM = prev_now_num;
        PINYIN_START_INDEX = prev_start_index;
        PINYIN_END_INDEX = prev_end_index;
        PINYIN_NUM_SELECT = prev_num_select;
        if (PINYIN_CODE_INDEX == 100000)INPUT_STAGE = 0;
        return;
    }

    if (INPUT_STAGE) {
        bool force_prefix = true;

        if (PINYIN_SEARCH_FOUND && !force_prefix) {
            if (PINYIN_SEARCH_INDEX != 255) {
                PINYIN_NOW_INDEX = PINYIN_SEARCH_INDEX;
                PINYIN_NOW_NUM = PINYIN_SEARCH_NUM;
                PINYIN_SEARCH_MODE = 1;
                PINYIN_NUM_SELECT = 0;
            }
        } else {
            if (PINYIN_SEARCH_INDEX == 255) {
                return;
            }
            PINYIN_SEARCH_MODE = 2;
            PINYIN_START_INDEX = PINYIN_SEARCH_INDEX;
            PINYIN_END_INDEX = PINYIN_SEARCH_INDEX;
            PINYIN_NOW_INDEX = PINYIN_SEARCH_INDEX;
            PINYIN_NOW_NUM = PINYIN_SEARCH_NUM;
            PINYIN_NUM_SELECT = 0;

            for (uint8_t i = PINYIN_START_INDEX; i < 214; ++i) {
                uint32_t tmp_code = 0;
                PINYIN_ReadHeader(i, &tmp_code, NULL);
                if (judge_belong(PINYIN_CODE, tmp_code)) {
                    PINYIN_END_INDEX = i;
                } else {
                    break;
                }
            }
        }
    }
}

static void UPDATE_CHN(void)
{
    uint8_t tmp[5];

    PINYIN_Read(PINYIN_NOW_INDEX * 128 + 0X20000 + 16 + PINYIN_NUM_SELECT * 16 + 6, tmp, 5);
    CHN_NOW_ADD = tmp[1] | tmp[2] << 8 | tmp[3] << 16 | tmp[4] << 24;
    CHN_NOW_NUM = tmp[0];
    CHN_NOW_PAGE = 0;
}
#endif

#ifdef ENABLE_PINYIN
uint32_t formatInt(uint32_t number) {
    uint32_t formatted = number;
    uint32_t length = 0;

    while (number != 0) {
        number /= 10;
        length++;
    }
    if (length < 6) {
        for (uint8_t i = 0; i < 6 - length; ++i) {
            formatted *= 10;
        }
    }
    return formatted;
}

uint32_t get_num(const char *a) {
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

bool judge_belong(uint32_t a, uint32_t b)
{
    for (uint32_t i = 100000; i >= 1; i /= 10) {
        if (a / i == 0)break;
        if (a / i != b / i)return false;
        a = a - a / i * i;
        b = b - b / i * i;
    }
    return true;
}

uint8_t sear_pinyin_code(uint32_t target, uint8_t *pinyin_num, uint8_t *found)
{
    int left = 0;
    int right = 213;
    *found = 0;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        uint8_t tmp[5];
        PINYIN_Read(mid * 128 + 0x20000, tmp, 5);
        uint32_t mid_num = tmp[0] | tmp[1] << 8 | tmp[2] << 16 | tmp[3] << 24;
        *pinyin_num = tmp[4];
        if (mid_num == target) {
            *found = 1;
            return mid;
        } else if (target < mid_num) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }

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

static void IME_Finish(bool accepted) {
    gImeActive = false;
    if (accepted && gImeTarget != NULL && gImeTargetLen > 0) {
        const uint8_t copy_len = gImeTargetLen > MAX_EDIT_INDEX ? MAX_EDIT_INDEX : gImeTargetLen;
        memcpy(gImeTarget, edit, copy_len);
        if (copy_len < gImeTargetLen) {
            gImeTarget[copy_len] = '\0';
        }
    }
    gRequestDisplayScreen = gImeReturnDisplay;
    if (gImeCallback != NULL) {
        gImeCallback(accepted, gImeCallbackCtx);
    }
}

static void IME_ResetState(void) {
#ifdef ENABLE_PINYIN
    INPUT_SELECT = 0;
    INPUT_MODE_LAST = 0;
    INPUT_MODE = 0;
    INPUT_STAGE = 0;
    PINYIN_CODE = 0;
    PINYIN_CODE_INDEX = 100000;
    PINYIN_SEARCH_INDEX = 0;
    PINYIN_SEARCH_FOUND = 0;
    PINYIN_SEARCH_NUM = 0;
    PINYIN_NOW_INDEX = 0;
    PINYIN_NOW_NUM = 0;
    PINYIN_SEARCH_MODE = 0;
    PINYIN_START_INDEX = 0;
    PINYIN_END_INDEX = 0;
    PINYIN_NOW_PAGE = 0;
    PINYIN_NUM_SELECT = 0;
    CHN_NOW_ADD = 0;
    CHN_NOW_NUM = 0;
    CHN_NOW_PAGE = 0;
#endif
}

static uint8_t IME_GetEditLimit(void) {
    return gImeTargetLen > MAX_EDIT_INDEX ? MAX_EDIT_INDEX : gImeTargetLen;
}

#ifdef ENABLE_PINYIN
static void IME_UpdateEditFlags(void) {
    for (int j = 0; j < MAX_EDIT_INDEX; ++j) {
        if ((uint8_t)edit[j] >= 0xb0 && j != MAX_EDIT_INDEX - 1) {
            edit_chn[j] = 1;
            j++;
            edit_chn[j] = 2;
        } else {
            edit_chn[j] = 0;
        }
    }
}

static void IME_NormalizeEditIndex(uint8_t limit) {
    if (edit_index <= 0 || edit_index >= limit) {
        return;
    }
    if (edit_chn[edit_index] == 2) {
        edit_index--;
    }
}

static void IME_PrepareAsciiWrite(uint8_t limit) {
    IME_UpdateEditFlags();
    IME_NormalizeEditIndex(limit);
    if (edit_index < 0 || edit_index >= limit) {
        return;
    }
    if (edit_chn[edit_index] == 1 && edit_index + 1 < MAX_EDIT_INDEX) {
        edit[edit_index + 1] = '_';
    }
}
#endif

void IME_Start(char *buffer, uint8_t length, IME_DoneCallback callback, void *ctx) {
    const uint8_t max_len = length > MAX_EDIT_INDEX ? MAX_EDIT_INDEX : length;
    gImeTarget = buffer;
    gImeTargetLen = max_len;
    gImeCallback = callback;
    gImeCallbackCtx = ctx;
    gImeReturnDisplay = gScreenToDisplay;
    gImeActive = true;

    uint8_t copy_len = 0;
    if (buffer != NULL) {
        while (copy_len < max_len && buffer[copy_len] != '\0') {
            edit[copy_len] = buffer[copy_len];
            copy_len++;
        }
    }
    while (copy_len < MAX_EDIT_INDEX) {
        edit[copy_len++] = '_';
    }
    edit[MAX_EDIT_INDEX] = '\0';
    memcpy(edit_original, edit, sizeof(edit_original));
    edit_index = 0;
    IME_ResetState();
#ifdef ENABLE_PINYIN
    IME_UpdateEditFlags();
#endif

    gRequestDisplayScreen = DISPLAY_IME;
}

static void IME_Key_0_to_9(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld) {
    if (bKeyHeld || !bKeyPressed || !gImeActive)
        return;

    gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
    const uint8_t limit = IME_GetEditLimit();
    if (edit_index < 0 || edit_index >= limit)
        return;

#ifdef ENABLE_PINYIN
    IME_UpdateEditFlags();
    IME_NormalizeEditIndex(limit);
    if (edit_index < 0 || edit_index >= limit)
        return;
    if (INPUT_MODE == 0) {
        if (Key >= 2 && PINYIN_CODE_INDEX && INPUT_STAGE <= 1) {
            uint32_t tmp = PINYIN_CODE;
            PINYIN_CODE += Key * PINYIN_CODE_INDEX;
            PINYIN_CODE_INDEX /= 10;
            PINYIN_SOLVE(tmp);
        } else if (INPUT_STAGE == 2) {
            uint8_t SHOW_NUM =
                    CHN_NOW_NUM - CHN_NOW_PAGE * 6 > 6 ? 6 : CHN_NOW_NUM - CHN_NOW_PAGE * 6;
            if (Key > 0 && Key <= SHOW_NUM) {
                if (edit_index + 1 >= limit) {
                    return;
                }
                if (edit_chn[edit_index + 1] == 1 && edit_index + 2 < MAX_EDIT_INDEX) {
                    edit[edit_index + 2] = '_';
                }
                PINYIN_Read(CHN_NOW_ADD + CHN_NOW_PAGE * 6 * 2 + 2 * (Key - 1), &edit[edit_index], 2);
                edit_index += 2;
                PINYIN_NUM_SELECT = 0;
                PINYIN_CODE = 0;
                PINYIN_SEARCH_MODE = 0;
                INPUT_STAGE = 0;
                CHN_NOW_PAGE = 0;
                PINYIN_CODE_INDEX = 100000;
                if (edit_index >= limit) {
                    IME_Finish(true);
                    return;
                }
            }
        }
    } else if (INPUT_MODE == 1) {
        if (INPUT_STAGE == 0) {
            if (Key >= KEY_2) {
                INPUT_STAGE = 1;
                INPUT_SELECT = Key;
            }
        } else {
            if (Key >= 1 && Key <= 2 * num_size[INPUT_SELECT - 2]) {
                IME_PrepareAsciiWrite(limit);
                if (edit_index < 0 || edit_index >= limit) {
                    return;
                }
                if (Key > num_size[INPUT_SELECT - 2])
                    edit[edit_index] = num_excel[INPUT_SELECT - 2][Key - 1 - num_size[INPUT_SELECT - 2]] - 32;
                else
                    edit[edit_index] = num_excel[INPUT_SELECT - 2][Key - 1];
                if (++edit_index >= limit) {
                    IME_Finish(true);
                    return;
                }
                INPUT_STAGE = 0;
            }
        }
    } else if (INPUT_MODE == 2) {
        IME_PrepareAsciiWrite(limit);
        if (edit_index < 0 || edit_index >= limit) {
            return;
        }
        edit[edit_index] = '0' + Key;
        if (++edit_index >= limit) {
            IME_Finish(true);
            return;
        }
    }
#else
    edit[edit_index] = '0' + Key;
    if (++edit_index >= limit) {
        IME_Finish(true);
        return;
    }
#endif

    gRequestDisplayScreen = DISPLAY_IME;
}

static void IME_Key_MENU(const bool bKeyPressed, const bool bKeyHeld) {
    if (bKeyHeld || !bKeyPressed || !gImeActive)
        return;
    gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
    gRequestDisplayScreen = DISPLAY_IME;
    const uint8_t limit = IME_GetEditLimit();
    if (edit_index < 0) {
        return;
    }
#ifdef ENABLE_PINYIN
    if (INPUT_MODE == 0) {
        if (PINYIN_CODE && PINYIN_SEARCH_MODE == 0)return;
        if (PINYIN_SEARCH_MODE == 1) {
            if (INPUT_STAGE == 1) {
                PINYIN_NUM_SELECT = 0;
                INPUT_STAGE = 2;
                UPDATE_CHN();
                return;
            } else if (INPUT_STAGE == 2) {
                if (PINYIN_NUM_SELECT < PINYIN_SEARCH_NUM - 1)PINYIN_NUM_SELECT++;
                else PINYIN_NUM_SELECT = 0;
                UPDATE_CHN();
                return;
            }
        } else if (PINYIN_SEARCH_MODE == 2) {
            if (INPUT_STAGE == 1) {
                INPUT_STAGE = 2;
                UPDATE_CHN();
                return;
            } else if (INPUT_STAGE == 2) {
                if (PINYIN_NUM_SELECT + 1 < PINYIN_NOW_NUM) {
                    PINYIN_NUM_SELECT++;
                } else {
                    uint8_t next_index = PINYIN_NOW_INDEX + 1;
                    if (next_index > PINYIN_END_INDEX || next_index < PINYIN_START_INDEX) {
                        next_index = PINYIN_START_INDEX;
                    }
                    PINYIN_NOW_INDEX = next_index;
                    PINYIN_NUM_SELECT = 0;
                    PINYIN_ReadHeader(PINYIN_NOW_INDEX, NULL, &PINYIN_NOW_NUM);
                }
                UPDATE_CHN();
                return;
            }
        }
    } else if (INPUT_MODE == 3) {
        INPUT_MODE = INPUT_MODE_LAST;
    }
#endif

    if (edit_index < limit) {
#ifdef ENABLE_PINYIN
        IME_UpdateEditFlags();
        IME_NormalizeEditIndex(limit);
        if (edit_chn[edit_index] == 1)
            edit_index++;
#endif
        edit_index++;
#ifdef ENABLE_PINYIN
        if (INPUT_MODE == 3)INPUT_MODE = INPUT_MODE_LAST;
#endif
        if (edit_index < limit) {
#ifdef ENABLE_PINYIN
            if (INPUT_MODE == 0 && edit_index + 1 >= limit)
                INPUT_MODE = 1;
#endif
            return;
        }
    }
    IME_Finish(true);
}

static void IME_Key_EXIT(bool bKeyPressed, bool bKeyHeld) {
    if (bKeyHeld || !bKeyPressed || !gImeActive)
        return;
#ifdef ENABLE_PINYIN
    if (INPUT_MODE == 0) {
        if (INPUT_STAGE == 1 && PINYIN_CODE > 0) {
            if (PINYIN_CODE_INDEX != 0) {
                PINYIN_CODE = PINYIN_CODE / (PINYIN_CODE_INDEX * 100) * (PINYIN_CODE_INDEX * 100);
                PINYIN_CODE_INDEX *= 10;
            } else {
                PINYIN_CODE = PINYIN_CODE - PINYIN_CODE % 10;
                PINYIN_CODE_INDEX = 1;
            }

            uint32_t tmp = PINYIN_CODE;
            PINYIN_SEARCH_MODE = 0;
            PINYIN_NUM_SELECT = 0;
            PINYIN_SOLVE(tmp);

            if (PINYIN_CODE == 0) {
                INPUT_STAGE = 0;
            }
            gRequestDisplayScreen = DISPLAY_IME;
            return;
        } else if (INPUT_STAGE == 2) {
            INPUT_STAGE = 1;
            gRequestDisplayScreen = DISPLAY_IME;
            return;
        }
    } else if (INPUT_MODE == 1) {
        if (INPUT_STAGE == 1) {
            INPUT_STAGE = 0;
            gRequestDisplayScreen = DISPLAY_IME;
            return;
        }
    }
#endif
    IME_Finish(false);
}

static void IME_Key_STAR(const bool bKeyPressed, const bool bKeyHeld) {
    if (bKeyHeld || !bKeyPressed || !gImeActive)
        return;

    gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
    const uint8_t limit = IME_GetEditLimit();

    if (edit_index < 0 || edit_index >= limit) {
        return;
    }

#ifdef ENABLE_PINYIN
    IME_UpdateEditFlags();
    IME_NormalizeEditIndex(limit);
    if (edit_index < 0 || edit_index >= limit) {
        return;
    }
#endif

    if (edit_index < limit) {
#ifndef ENABLE_PINYIN
        edit[edit_index] = '-';
        if (++edit_index >= limit) {
            IME_Finish(true);
            return;
        }
#else
        INPUT_MODE++;
        if (INPUT_MODE >= 3)INPUT_MODE = 0;
        if (INPUT_MODE == 0 && edit_index + 1 >= limit)
            INPUT_MODE = 1;
        if (INPUT_MODE == 0) {
            PINYIN_CODE = 0;
            PINYIN_CODE_INDEX = 100000;
        }
        INPUT_STAGE = 0;
#endif

        gRequestDisplayScreen = DISPLAY_IME;
    }
}

static void IME_Key_UP_DOWN(bool bKeyPressed, bool bKeyHeld, int8_t Direction) {
    if (!gImeActive)
        return;
    if (bKeyHeld || !bKeyPressed)
        return;

    gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
    const uint8_t limit = IME_GetEditLimit();

    if (edit_index < 0 || edit_index >= limit)
        return;

#ifdef ENABLE_PINYIN
    if (INPUT_MODE == 0) {
        if (INPUT_STAGE == 2) {
            if (PINYIN_SEARCH_MODE == 1 || PINYIN_SEARCH_MODE == 2) {
                if (Direction == 1) {
                    if (CHN_NOW_PAGE) CHN_NOW_PAGE--;
                } else if (Direction == -1) {
                    if ((CHN_NOW_PAGE + 1) * 6 < CHN_NOW_NUM)CHN_NOW_PAGE++;
                }
                gRequestDisplayScreen = DISPLAY_IME;
                return;
            }
        }
    }

    if (((INPUT_MODE == 0 || INPUT_MODE == 1) && INPUT_STAGE == 0) || INPUT_MODE == 2 || INPUT_MODE == 3) {
        INPUT_MODE_LAST = INPUT_MODE;
        INPUT_MODE = 3;

        IME_PrepareAsciiWrite(limit);
        if (edit_index < 0 || edit_index >= limit) {
            return;
        }
        char c = edit[edit_index] + Direction;
        while (c >= 32 && c <= 126) {
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z' )||
               ( c >= '0' && c <= '9')) {
                c += Direction;
            } else break;
        }
        edit[edit_index] = ((uint8_t) c < 32) ? 126 : ((uint8_t) c > 126) ? 32 : c;
    }
#else
    if (isChineseChar(edit[edit_index], edit_index, limit)) {
        edit[edit_index + 1] = '_';
        edit[edit_index] = '_';
    }
    const char unwanted[] = "$%&!\"':;?^`|{}";
    char c = edit[edit_index] + Direction;
    unsigned int i = 0;
    while (i < sizeof(unwanted) && c >= 32 && c <= 126) {
        if (c == unwanted[i++]) {
            c += Direction;
            i = 0;
        }
    }
    edit[edit_index] = ((uint8_t) c < 32) ? 126 : ((uint8_t) c > 126) ? 32 : c;
#endif

    gRequestDisplayScreen = DISPLAY_IME;
}

static void IME_Key_F(bool bKeyPressed, bool bKeyHeld) {
    if (bKeyHeld || !bKeyPressed || !gImeActive)
        return;

    gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
    const uint8_t limit = IME_GetEditLimit();
    if (edit_index < 0 || edit_index >= limit) {
        return;
    }

#ifdef ENABLE_PINYIN
    bool flag_space = true;
    if ((INPUT_MODE == 0 && INPUT_STAGE > 0 )|| (INPUT_MODE == 1 && INPUT_STAGE > 0))flag_space = false;
#endif

    if (edit_index < limit
#ifdef ENABLE_PINYIN
        && flag_space
#endif
            ) {

#ifdef ENABLE_PINYIN
        IME_PrepareAsciiWrite(limit);
        if (edit_index < 0 || edit_index >= limit) {
            return;
        }
#endif
        edit[edit_index] = ' ';
        if (++edit_index >= limit) {
            IME_Finish(true);
            return;
        }
        gRequestDisplayScreen = DISPLAY_IME;
    }
}

static void IME_Key_SidePage(bool bKeyPressed, bool bKeyHeld, int8_t direction) {
    if (bKeyHeld || !bKeyPressed || !gImeActive)
        return;
#ifdef ENABLE_PINYIN
    if (INPUT_MODE != 0 || INPUT_STAGE < 1 || PINYIN_SEARCH_MODE == 0) {
        return;
    }

    const uint8_t page_size = 6;
    bool changed = false;

    if (PINYIN_SEARCH_MODE == 1) {
        const uint8_t total = PINYIN_NOW_NUM;
        if (total == 0) {
            return;
        }
        const uint8_t total_pages = (total + page_size - 1) / page_size;
        if (total_pages <= 1) {
            return;
        }
        const uint8_t page = PINYIN_NUM_SELECT / page_size;
        uint8_t pos = PINYIN_NUM_SELECT % page_size;
        uint8_t new_page = (direction > 0) ? (uint8_t)((page + 1) % total_pages)
                                           : (uint8_t)((page == 0) ? (total_pages - 1) : (page - 1));
        uint8_t start = new_page * page_size;
        uint8_t avail = total - start;
        if (pos >= avail) {
            pos = (uint8_t)(avail - 1);
        }
        uint8_t new_select = start + pos;
        if (new_select != PINYIN_NUM_SELECT) {
            PINYIN_NUM_SELECT = new_select;
            if (INPUT_STAGE == 2) {
                UPDATE_CHN();
            }
            changed = true;
        }
    } else if (PINYIN_SEARCH_MODE == 2) {
        const uint16_t total = PINYIN_CountRange(PINYIN_START_INDEX, PINYIN_END_INDEX);
        if (total == 0) {
            return;
        }
        const uint16_t total_pages = (total + page_size - 1) / page_size;
        if (total_pages <= 1) {
            return;
        }
        const uint16_t flat_index = PINYIN_FlatIndex(PINYIN_START_INDEX, PINYIN_NOW_INDEX, PINYIN_NUM_SELECT);
        const uint16_t page = flat_index / page_size;
        uint16_t pos = flat_index % page_size;
        uint16_t new_page = (direction > 0) ? (uint16_t)((page + 1) % total_pages)
                                            : (uint16_t)((page == 0) ? (total_pages - 1) : (page - 1));
        uint16_t start = new_page * page_size;
        uint16_t avail = total - start;
        if (pos >= avail) {
            pos = avail - 1;
        }
        uint16_t target = start + pos;
        uint8_t entry_index = 0;
        uint8_t entry_offset = 0;
        if (PINYIN_GetFlatEntry(PINYIN_START_INDEX, PINYIN_END_INDEX, target, &entry_index, &entry_offset)) {
            PINYIN_NOW_INDEX = entry_index;
            PINYIN_NUM_SELECT = entry_offset;
            PINYIN_ReadHeader(PINYIN_NOW_INDEX, NULL, &PINYIN_NOW_NUM);
            if (INPUT_STAGE == 2) {
                UPDATE_CHN();
            }
            changed = true;
        }
    }

    if (changed) {
        gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
        gRequestDisplayScreen = DISPLAY_IME;
    }
#endif
}

void IME_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld) {
    if (!gImeActive)
        return;
    switch (Key) {
        case KEY_0:
        case KEY_1:
        case KEY_2:
        case KEY_3:
        case KEY_4:
        case KEY_5:
        case KEY_6:
        case KEY_7:
        case KEY_8:
        case KEY_9:
            IME_Key_0_to_9(Key, bKeyPressed, bKeyHeld);
            break;
        case KEY_MENU:
            IME_Key_MENU(bKeyPressed, bKeyHeld);
            break;
        case KEY_UP:
            IME_Key_UP_DOWN(bKeyPressed, bKeyHeld, 1);
            break;
        case KEY_DOWN:
            IME_Key_UP_DOWN(bKeyPressed, bKeyHeld, -1);
            break;
        case KEY_EXIT:
            IME_Key_EXIT(bKeyPressed, bKeyHeld);
            break;
        case KEY_STAR:
            IME_Key_STAR(bKeyPressed, bKeyHeld);
            break;
        case KEY_F:
            IME_Key_F(bKeyPressed, bKeyHeld);
            break;
        case KEY_SIDE1:
            IME_Key_SidePage(bKeyPressed, bKeyHeld, -1);
            break;
        case KEY_SIDE2:
            IME_Key_SidePage(bKeyPressed, bKeyHeld, 1);
            break;
        default:
            if (!bKeyHeld && bKeyPressed)
                gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
            break;
    }
}

bool IME_IsActive(void) {
    return gImeActive;
}

void IME_Cancel(void) {
    if (!gImeActive)
        return;
    IME_Finish(false);
}
