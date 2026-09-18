#include <types.h>
#include <rtc.h>
#include <arm.h>
#include <console.h>

uint32_t get_yymmdd(uint32_t year, uint32_t month, uint32_t day)
{
    return ((year - SUNXI_YEARM_MIN) << RTC_YY_MM_DD_YEAR_SHIFT)
        | (month << RTC_YY_MM_DD_MONTH_SHIFT) | (day << RTC_YY_MM_DD_DAY_SHIFT);
}

uint32_t get_hhmmss(uint32_t wday, uint32_t hour, uint32_t minute, uint32_t second)
{
    return  (wday << RTC_HH_MM_SS_WK_NO_SHIFT)
        | (hour << RTC_HH_MM_SS_HOUR_SHIFT)
        | (minute << RTC_HH_MM_SS_MINUTE_SHIFT)
        | (second << RTC_HH_MM_SS_SECOND_SHIFT);
}
void rtc_init(void)
{
    uint32_t ymd = get_yymmdd(2026, 9, 18);
    uint32_t hms = get_hhmmss(WEDNESDAY, 11, 56, 0);
    put32(LOSC_CTRL_REG, LOSC_CTRL_KEY_FIELD | LOSC_CTRL_LOSC_SRC_SEL);
    put32(RTC_YY_MM_DD_REG, ymd);
    put32(RTC_HH_MM_SS_REG, hms);
    put32(LOSC_CTRL_REG, LOSC_CTRL_RTC_HHMMSS_ACCE | LOSC_CTRL_RTC_YYMMDD_ACCE);
    while (get32(LOSC_CTRL_REG) & (LOSC_CTRL_RTC_HHMMSS_ACCE | LOSC_CTRL_RTC_YYMMDD_ACCE));

    info("rtc_init ok");
}
