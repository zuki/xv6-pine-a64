#include <types.h>
#include <rtc.h>
#include <arm.h>
#include <console.h>
#include <linux/time.h>

static const unsigned char rtc_days_in_month[] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

#define LEAPS_THRU_END_OF(y) ((y)/4 - (y)/100 + (y)/400)

static inline int is_leap_year(int year)
{
    return (!(year % 4) && (year % 100)) || !(year % 400);
}

static int rtc_month_days(int month, int year)
{
    return rtc_days_in_month[month] + (is_leap_year(year) && month == 1);
}

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
    // TODO: rtcから取得
    uint32_t ymd = get_yymmdd(2026, 11, 5);
    uint32_t hms = get_hhmmss(THURSDAY, 11, 56, 0);
    put32(LOSC_CTRL_REG, LOSC_CTRL_KEY_FIELD | LOSC_CTRL_LOSC_SRC_SEL);
    put32(RTC_YY_MM_DD_REG, ymd);
    put32(RTC_HH_MM_SS_REG, hms);
    put32(LOSC_CTRL_REG, LOSC_CTRL_RTC_HHMMSS_ACCE | LOSC_CTRL_RTC_YYMMDD_ACCE);
    while (get32(LOSC_CTRL_REG) & (LOSC_CTRL_RTC_HHMMSS_ACCE | LOSC_CTRL_RTC_YYMMDD_ACCE));

    info("rtc_init ok");
}

static time_t rtc_tm_to_time(const unsigned int year0, const unsigned int mon0,
        const unsigned int day, const unsigned int hour,
        const unsigned int min, const unsigned int sec)
{
    unsigned int year = year0 + 1900 - SUNXI_YEARM_MIN;
    unsigned int mon = mon0;

    /* 1..12 -> 11,12,1..10 */
    if (0 >= (mon -= 2)) {
        mon += 12;  /* Puts Feb last since it has leap day */
        year -= 1;
    }

    return ((((time_t)
                (year/4 - year/100 + year/400 + 367*mon/12 + day) + year*365 - 719499
            ) * 24 + hour /* now have hours - midnight tomorrow handled here */
        ) * 60 + min /* now have minutes */
    ) * 60 + sec; /* finally seconds */
}

static void rtc_time_to_tm(time_t time, struct rtc_time *tm)
{
    unsigned int month, year;
    unsigned long secs;
    int days;

    /* time must be positive */
    days = time / 86400LL;
    secs = time - (unsigned int) days * 86400;

    /* day of the week, 1970-01-01 was a Thursday */
    tm->tm_wday = (days + 4) % 7;

    year = 1970 + days / 365;
    days -= (year - 1970) * 365
        + LEAPS_THRU_END_OF(year - 1)
        - LEAPS_THRU_END_OF(1970 - 1);
    if (days < 0) {
        year -= 1;
        days += 365 + is_leap_year(year);
    }
    tm->tm_year = year - 1900;
    tm->tm_yday = days + 1;

    for (month = 0; month < 11; month++) {
        int newdays;

        newdays = days - rtc_month_days(month, year);
        if (newdays < 0)
            break;
        days = newdays;
    }
    tm->tm_mon = month;
    tm->tm_mday = days + 1;

    tm->tm_hour = secs / 3600;
    secs -= tm->tm_hour * 3600;
    tm->tm_min = secs / 60;
    tm->tm_sec = secs - tm->tm_min * 60;

    tm->tm_isdst = 0;
}

int rtc_settime(const struct timespec *tp)
{
    struct rtc_time t;

    rtc_time_to_tm(tp->tv_sec, &t);

    trace("secs=%d, mins=%d, hours=%d, mday=%d, mon=%d, year=%d, wday=%d",
        t.tm_sec, t.tm_min, t.tm_hour, t.tm_mday, t.tm_mon, t.tm_year, t.tm_wday);

    int leap = is_leap_year(t.tm_year + 1900);
    put32(RTC_YY_MM_DD_REG, ((leap << RTC_YY_MM_DD_LEAP) |
        ((t.tm_year + 1900 - SUNXI_YEARM_MIN) << RTC_YY_MM_DD_YEAR_SHIFT) |
        ((t.tm_mon + 1) << RTC_YY_MM_DD_MONTH_SHIFT) |
        (t.tm_mday << RTC_YY_MM_DD_DAY_SHIFT)));
    put32(RTC_HH_MM_SS_REG, ((t.tm_wday << RTC_HH_MM_SS_WK_NO_SHIFT) |
        (t.tm_hour << RTC_HH_MM_SS_HOUR_SHIFT) |
        (t.tm_min << RTC_HH_MM_SS_MINUTE_SHIFT) |
        (t.tm_sec << RTC_HH_MM_SS_SECOND_SHIFT)));

    return 0;
}

int rtc_gettime(struct timespec *tp)
{
    struct rtc_time t;

    uint32_t ymd = get32(RTC_YY_MM_DD_REG);
    uint32_t hms = get32(RTC_HH_MM_SS_REG);

    tp->tv_sec = rtc_tm_to_time(t.tm_year, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
    tp->tv_nsec = 0L;

    trace("tp_sec: %lld", tp->tv_sec);

    return 0;

}
