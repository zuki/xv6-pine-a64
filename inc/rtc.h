#ifndef INC_RTC_H
#define INC_RTC_H

#include <peripherals/base.h>

#define RTC_BASE            (MMIO_BASE + 0x01F00000)

#define LOSC_CTRL_REG       (RTC_BASE + 0x00)
#define     LOSC_CTRL_KEY_FIELD           (0x16aa << 16)
#define     LOSC_CTRL_RTC_HHMMSS_ACCE     (1 << 8)
#define     LOSC_CTRL_RTC_YYMMDD_ACCE     (1 << 7)
#define     LOSC_CTRL_LOSC_SRC_SEL        (1 << 0)
#define RTC_YY_MM_DD_REG    (RTC_BASE + 0x10)
#define     RTC_YY_MM_DD_LEAP             (1 << 22)
#define     RTC_YY_MM_DD_YEAR_SHIFT       (16)
#define     RTC_YY_MM_DD_MONTH_SHIFT      (8)
#define     RTC_YY_MM_DD_DAY_SHIFT        (0)
#define RTC_HH_MM_SS_REG    (RTC_BASE + 0x14)
#define     RTC_HH_MM_SS_WK_NO_SHIFT      (29)
#define     RTC_HH_MM_SS_HOUR_SHIFT       (16)
#define     RTC_HH_MM_SS_MINUTE_SHIFT     (8)
#define     RTC_HH_MM_SS_SECOND_SHIFT     (0)

#define SUNXI_YEARM_MIN     1970

enum weekday {
    MONDAY = 0,
    TUESDAY,
    WEDNESDAY,
    THURSDAY,
    FRIDAY,
    SATURDAY,
    SUNDAY
};

void rtc_init(void);

#endif
