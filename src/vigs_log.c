#include "vigs_log.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>

static const char *g_log_level_to_str[vigs_log_level_max + 1] =
{
    "OFF",
    "ERROR",
    "WARN",
    "INFO",
    "DEBUG",
    "TRACE"
};

static pthread_once_t g_log_init = PTHREAD_ONCE_INIT;
static vigs_log_level g_log_level = vigs_log_level_off;

static void vigs_log_init_once(void)
{
    char *level_str = getenv("VIGS_DEBUG");
    int level = level_str ? atoi(level_str) : vigs_log_level_off;

    if (level < 0) {
        g_log_level = vigs_log_level_off;
    } else if (level > vigs_log_level_max) {
        g_log_level = (vigs_log_level)vigs_log_level_max;
    } else {
        g_log_level = (vigs_log_level)level;
    }
}

static void vigs_log_init(void)
{
    pthread_once(&g_log_init, vigs_log_init_once);
}

static void vigs_log_print_current_time(void)
{
    char buff[128];
    struct tm tm;
    struct timeval tv = { 0, 0 };
    time_t ti;

    gettimeofday(&tv, NULL);

    ti = tv.tv_sec;

    localtime_r(&ti, &tm);
    strftime(buff, sizeof(buff),
             "%H:%M:%S", &tm);
    fprintf(stderr, "%s", buff);
}

void vigs_log_event(vigs_log_level log_level,
                    const char *func,
                    int line,
                    const char *format, ...)
{
    va_list args;

    vigs_log_init();

    vigs_log_print_current_time();
    fprintf(stderr,
            " %-5s %s:%d",
            g_log_level_to_str[log_level],
            func,
            line);
    if (format) {
        va_start(args, format);
        fprintf(stderr, " - ");
        vfprintf(stderr, format, args);
        va_end(args);
    }
    fprintf(stderr, "\n");
}

int vigs_log_is_enabled_for_level(vigs_log_level log_level)
{
    vigs_log_init();

    return log_level <= g_log_level;
}
