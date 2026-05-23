#ifndef LOG_H
#define LOG_H

typedef enum {
    CAT_SCREEN,
    CAT_DRAFT,
    CAT_COMBAT,
    CAT_DECK,
    CAT_CARD,
    CAT_INPUT,
    CAT_ENERGY,
    CAT_AGGRO,
    CAT_TRACE,
} LogCategory;

typedef enum {
    LVL_INFO,
    LVL_WARN,
    LVL_ERROR,
    LVL_DEBUG,
} LogLevel;

void log_init(void);
void log_close(void);
void log_write(LogCategory cat, LogLevel level, const char *fmt, ...);

#define LOG_I(cat, ...) log_write(cat, LVL_INFO, __VA_ARGS__)
#define LOG_W(cat, ...) log_write(cat, LVL_WARN, __VA_ARGS__)
#define LOG_E(cat, ...) log_write(cat, LVL_ERROR, __VA_ARGS__)
#define LOG_D(cat, ...) log_write(cat, LVL_DEBUG, __VA_ARGS__)
#define LOG_T(...)      log_write(CAT_TRACE, LVL_INFO, __VA_ARGS__)

#endif
