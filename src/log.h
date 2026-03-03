#ifndef NM_LOG_H
#define NM_LOG_H

typedef enum {
    NM_LOG_ERROR   = 0,
    NM_LOG_WARN    = 1,
    NM_LOG_INFO    = 2,
    NM_LOG_DEBUG   = 3,
    NM_LOG_TRACE   = 4,
    NM_LOG_PACKET  = 5
} nm_log_level_t;

/* Set global verbosity level (0-5) */
void nm_log_set_level(int level);

/* Get current verbosity level */
int nm_log_get_level(void);

/* Log a message at the given level */
void nm_log(nm_log_level_t level, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

/* Convenience macros */
#define LOG_ERROR(...)  nm_log(NM_LOG_ERROR,  __VA_ARGS__)
#define LOG_WARN(...)   nm_log(NM_LOG_WARN,   __VA_ARGS__)
#define LOG_INFO(...)   nm_log(NM_LOG_INFO,   __VA_ARGS__)
#define LOG_DEBUG(...)  nm_log(NM_LOG_DEBUG,   __VA_ARGS__)
#define LOG_TRACE(...)  nm_log(NM_LOG_TRACE,   __VA_ARGS__)
#define LOG_PACKET(...) nm_log(NM_LOG_PACKET,  __VA_ARGS__)

#endif /* NM_LOG_H */
