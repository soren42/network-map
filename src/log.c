#include "log.h"
#include <stdio.h>
#include <stdarg.h>

static int g_log_level = NM_LOG_ERROR;

static const char *level_names[] = {
    "ERROR", "WARN", "INFO", "DEBUG", "TRACE", "PACKET"
};

void nm_log_set_level(int level)
{
    if (level < 0) level = 0;
    if (level > NM_LOG_PACKET) level = NM_LOG_PACKET;
    g_log_level = level;
}

int nm_log_get_level(void)
{
    return g_log_level;
}

void nm_log(nm_log_level_t level, const char *fmt, ...)
{
    if ((int)level > g_log_level) return;

    fprintf(stderr, "[%s] ", level_names[level]);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fprintf(stderr, "\n");
}
