#include "log.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#ifdef __EMSCRIPTEN__
#define _mkdir(path) mkdir(path, 0755)
#endif

#define LOG_BASE "logs"

static FILE *files[CAT_TRACE + 1];
static FILE *master_log;
static char dir_path[256];
static bool initialized = false;

static const char *cat_label(LogCategory cat)
{
    switch (cat)
    {
        case CAT_SCREEN: return "SCREEN";
        case CAT_DRAFT:  return "DRAFT";
        case CAT_COMBAT: return "COMBAT";
        case CAT_DECK:   return "DECK";
        case CAT_CARD:   return "CARD";
        case CAT_INPUT:  return "INPUT";
        case CAT_ENERGY: return "ENERGY";
        case CAT_AGGRO:  return "AGGRO";
        case CAT_TRACE:  return "TRACE";
    }
    return "?";
}

static const char *lvl_label(LogLevel lvl)
{
    switch (lvl)
    {
        case LVL_INFO:  return "I";
        case LVL_WARN:  return "W";
        case LVL_ERROR: return "E";
        case LVL_DEBUG: return "D";
    }
    return "?";
}

static void ensure_dir(const char *path)
{
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++)
        if (*p == '/') { *p = '\0'; _mkdir(tmp); *p = '/'; }
    _mkdir(tmp);
}

static FILE *open_cat_file(LogCategory cat, const char *mode)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.log", dir_path, cat_label(cat));
    FILE *f = fopen(path, mode);
    return f;
}

void log_init(void)
{
    if (initialized) return;
    memset(files, 0, sizeof(files));

    ensure_dir(LOG_BASE);

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    snprintf(dir_path, sizeof(dir_path), "%s/session_%04d%02d%02d_%02d%02d%02d",
        LOG_BASE, tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
        tm->tm_hour, tm->tm_min, tm->tm_sec);

    ensure_dir(dir_path);

    master_log = fopen(TextFormat("%s/ALL.log", dir_path), "w");
    if (master_log)
        fprintf(master_log, "=== RaidParty Log: %s ===\n", dir_path);

    TraceLog(LOG_INFO, "Logging to %s/", dir_path);
    initialized = true;
}

void log_close(void)
{
    for (int i = 0; i <= CAT_TRACE; i++)
        if (files[i]) { fclose(files[i]); files[i] = NULL; }
    if (master_log) { fclose(master_log); master_log = NULL; }
}

void log_write(LogCategory cat, LogLevel level, const char *fmt, ...)
{
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    char msg[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    // Write to master log (everything)
    if (master_log)
    {
        fprintf(master_log, "[%02d:%02d:%02d] [%-6s] [%s] %s\n",
            tm->tm_hour, tm->tm_min, tm->tm_sec, cat_label(cat), lvl_label(level), msg);
        fflush(master_log);
    }

    // Write to category file
    if (!files[cat])
        files[cat] = open_cat_file(cat, "w");
    if (files[cat])
    {
        fprintf(files[cat], "[%02d:%02d:%02d] [%s] %s\n",
            tm->tm_hour, tm->tm_min, tm->tm_sec, lvl_label(level), msg);
        fflush(files[cat]);
    }

    // Also log errors and warnings via raylib
    if (level == LVL_ERROR)
        TraceLog(LOG_ERROR, "[%s] %s", cat_label(cat), msg);
    else if (level == LVL_WARN)
        TraceLog(LOG_WARNING, "[%s] %s", cat_label(cat), msg);
}
