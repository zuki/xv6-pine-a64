#include <stdio.h>
#include <stdlib.h>
#include <time.h>

char * const wdays[] = { "日", "月", "火", "水", "木", "金", "土" };

void print_now(void)
{
    time_t t = time(NULL) + 9 * 60 * 60;
    struct tm *local = gmtime(&t);

    printf("%04d年%2d日%2d日 %s曜日 %2d時%2d分%2d秒 JST\n",
        local->tm_year + 1900, local->tm_mon + 1, local->tm_mday,
        wdays[local->tm_wday], local->tm_hour, local->tm_min, local->tm_sec);
}

int main(int argc, char *argv[])
{
    int fd;
    char buf[11];

    if (argc == 1) {
        print_now();
    } else if (argc == 2) {
        struct timespec tp;
        long t = atol(argv[1]);
        tp.tv_sec = (time_t)t;
        tp.tv_nsec = 0;
        if (clock_settime(CLOCK_REALTIME, &tp) < 0) {
            fprintf(stderr, "failed set date\n");
        } else {
            print_now();
        }
    }

    return 0;
}
