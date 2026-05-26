#include "economy_metrics.h"
#include <stdio.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#ifndef _WIN32
#define _mkdir(path) mkdir(path, 0755)
#endif

static void ensure_logs_dir(void)
{
    _mkdir("logs");
}

static int file_has_content(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    return size > 0;
}

void economy_metrics_log(const char *event, int amount, int gold_before, int gold_after, const char *context)
{
    ensure_logs_dir();
    const char *path = "logs/economy_metrics.csv";
    int needs_header = !file_has_content(path);
    FILE *f = fopen(path, "a");
    if (!f) return;

    if (needs_header)
        fprintf(f, "timestamp,event,amount,gold_before,gold_after,context\n");

    time_t now = time(NULL);
    fprintf(f, "%lld,%s,%d,%d,%d,%s\n",
        (long long)now,
        event ? event : "",
        amount,
        gold_before,
        gold_after,
        context ? context : "");
    fclose(f);
}
