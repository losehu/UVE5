// ESP32 build: adapt the RTC API used by the TLE module to the existing PCF8563 driver.

#include "rtc.h"

#include "driver/pcf8563.h"

uint8_t my_time[6] = {0};

static void rtc_update_from_pcf(const pcf8563_time_t *t)
{
    if (!t) {
        return;
    }
    if (t->year < 2000 || t->year > 2099) {
        return;
    }

    my_time[0] = (uint8_t)(t->year - 2000U);
    my_time[1] = t->month;
    my_time[2] = t->day;
    my_time[3] = t->hour;
    my_time[4] = t->minute;
    my_time[5] = t->second;
}

void RTC_INIT(void)
{
    // Best-effort initialization: read once so my_time has a sane value.
    RTC_Get();
}

void RTC_Set(void)
{
    // Keep compatibility with legacy callers, but prefer using MENU_RTC in the main menu.
    pcf8563_time_t t = {0};
    t.year = (uint16_t)(2000U + (uint16_t)my_time[0]);
    t.month = my_time[1];
    t.day = my_time[2];
    t.hour = my_time[3];
    t.minute = my_time[4];
    t.second = my_time[5];
    t.weekday = 0;
    t.voltage_low = false;

    (void)PCF8563_SetTime(&t);
}

void RTC_Get(void)
{
    pcf8563_time_t t;
    if (!PCF8563_ReadTime(&t)) {
        return;
    }
    rtc_update_from_pcf(&t);
}
