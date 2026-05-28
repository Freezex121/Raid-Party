#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdbool.h>

void telemetry_init(void);
void telemetry_begin_run(int run_id);
void telemetry_push_json(const char *event_type, const char *json_fields);
void telemetry_csv_append(const char *filename, const char *header, const char **fields, int field_count);
void telemetry_log_tutorial(const char *tutorial_id, const char *action, const char *screen);
void telemetry_flush_local_only(void);
void telemetry_flush_upload(void);
void telemetry_shutdown(void);

#endif
