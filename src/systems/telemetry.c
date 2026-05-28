#include "systems/telemetry.h"
#include "game.h"
#include "util/log.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#ifndef __EMSCRIPTEN__
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef NOGDI
#define NOGDI
#endif
#ifndef NOUSER
#define NOUSER
#endif
#include <windows.h>
#include <winhttp.h>
#endif
#else
#include <sys/stat.h>
#endif

#ifndef _WIN32
#define _mkdir(path) mkdir(path, 0755)
#endif

#define TELEMETRY_MAX_EVENTS 512
#define TELEMETRY_MAX_EVENT_CHARS 2048
#define TELEMETRY_ENDPOINT_HOST L"raidparty-telemetry.boxeldergames.workers.dev"
#define TELEMETRY_ENDPOINT_PATH L"/"
#define TELEMETRY_USER_AGENT L"RaidPaperLegends/0.1.0"

static char event_buffer[TELEMETRY_MAX_EVENTS][TELEMETRY_MAX_EVENT_CHARS];
static int event_count = 0;
static bool dropped_events = false;
static int current_run_id = 0;

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

static long long now_unix(void)
{
    return (long long)time(NULL);
}

static void csv_write_escaped(FILE *f, const char *text)
{
    const char *safe = text ? text : "";
    fputc('"', f);
    for (const char *p = safe; *p; p++)
    {
        if (*p == '"') fputc('"', f);
        fputc(*p, f);
    }
    fputc('"', f);
}

static void json_escape_into(char *out, int out_size, const char *text)
{
    if (!out || out_size <= 0) return;
    const char *safe = text ? text : "";
    int pos = 0;
    for (const char *p = safe; *p && pos < out_size - 1; p++)
    {
        unsigned char c = (unsigned char)*p;
        if ((c == '"' || c == '\\') && pos < out_size - 2)
        {
            out[pos++] = '\\';
            out[pos++] = (char)c;
        }
        else if (c == '\n' && pos < out_size - 2)
        {
            out[pos++] = '\\';
            out[pos++] = 'n';
        }
        else if (c == '\r')
        {
            continue;
        }
        else
        {
            out[pos++] = (char)c;
        }
    }
    out[pos] = '\0';
}

void telemetry_init(void)
{
    ensure_logs_dir();
    event_count = 0;
    dropped_events = false;
    current_run_id = 0;
}

void telemetry_begin_run(int run_id)
{
    current_run_id = run_id > 0 ? run_id : 0;
    event_count = 0;
    dropped_events = false;
}

void telemetry_csv_append(const char *filename, const char *header, const char **fields, int field_count)
{
    if (!filename || !filename[0] || !header || !header[0]) return;
    ensure_logs_dir();

    char path[256];
    snprintf(path, sizeof(path), "logs/%s", filename);
    int needs_header = !file_has_content(path);

    FILE *f = fopen(path, "a");
    if (!f) return;

    if (needs_header)
        fprintf(f, "%s\n", header);

    fprintf(f, "%lld", now_unix());
    for (int i = 0; i < field_count; i++)
    {
        fputc(',', f);
        csv_write_escaped(f, fields ? fields[i] : "");
    }
    fputc('\n', f);
    fclose(f);
}

void telemetry_push_json(const char *event_type, const char *json_fields)
{
    if (!event_type || !event_type[0]) return;
    if (event_count >= TELEMETRY_MAX_EVENTS)
    {
        if (!dropped_events)
        {
            const char *fields[] = { "buffer_full", event_type };
            telemetry_csv_append("telemetry_uploads.csv", "timestamp,event,detail", fields, 2);
        }
        dropped_events = true;
        return;
    }

    char type[128];
    json_escape_into(type, sizeof(type), event_type);
    if (json_fields && json_fields[0])
        snprintf(event_buffer[event_count], TELEMETRY_MAX_EVENT_CHARS, "{\"type\":\"%s\",%s}", type, json_fields);
    else
        snprintf(event_buffer[event_count], TELEMETRY_MAX_EVENT_CHARS, "{\"type\":\"%s\"}", type);
    event_count++;
}

void telemetry_log_tutorial(const char *tutorial_id, const char *action, const char *screen)
{
    char runs[16];
    snprintf(runs, sizeof(runs), "%d", g_state.meta.runs_completed);
    const char *fields[] = {
        tutorial_id ? tutorial_id : "",
        action ? action : "",
        screen ? screen : "",
        runs
    };
    telemetry_csv_append(
        "tutorial_metrics.csv",
        "timestamp,tutorial_id,action,screen,runs_completed",
        fields,
        4);

    char id_json[128];
    char action_json[64];
    char screen_json[64];
    char json[384];
    json_escape_into(id_json, sizeof(id_json), tutorial_id);
    json_escape_into(action_json, sizeof(action_json), action);
    json_escape_into(screen_json, sizeof(screen_json), screen);
    snprintf(json, sizeof(json),
        "\"tutorial_id\":\"%s\",\"action\":\"%s\",\"screen\":\"%s\",\"runs_completed\":%d",
        id_json, action_json, screen_json, g_state.meta.runs_completed);
    telemetry_push_json("tutorial_seen", json);
}

void telemetry_flush_local_only(void)
{
    const char *fields[] = { "local_flush", "" };
    telemetry_csv_append("telemetry_uploads.csv", "timestamp,event,detail", fields, 2);
}

#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
static bool telemetry_http_post(const char *body)
{
    bool ok = false;
    HINTERNET session = WinHttpOpen(
        TELEMETRY_USER_AGENT,
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!session) return false;

    WinHttpSetTimeouts(session, 1500, 1500, 2500, 2500);
    HINTERNET connect = WinHttpConnect(session, TELEMETRY_ENDPOINT_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (connect)
    {
        HINTERNET request = WinHttpOpenRequest(
            connect,
            L"POST",
            TELEMETRY_ENDPOINT_PATH,
            NULL,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE);
        if (request)
        {
            const wchar_t *headers = L"Content-Type: application/json\r\n";
            DWORD body_len = (DWORD)strlen(body);
            if (WinHttpSendRequest(
                    request,
                    headers,
                    (DWORD)-1L,
                    (LPVOID)body,
                    body_len,
                    body_len,
                    0) &&
                WinHttpReceiveResponse(request, NULL))
            {
                DWORD status = 0;
                DWORD status_size = sizeof(status);
                if (WinHttpQueryHeaders(
                        request,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX,
                        &status,
                        &status_size,
                        WINHTTP_NO_HEADER_INDEX))
                {
                    ok = status >= 200 && status < 300;
                }
            }
            WinHttpCloseHandle(request);
        }
        WinHttpCloseHandle(connect);
    }
    WinHttpCloseHandle(session);
    return ok;
}
#endif

void telemetry_flush_upload(void)
{
    if (event_count <= 0)
        return;

    if (!g_state.telemetry_opt_in)
    {
        const char *fields[] = { "upload_skipped", "opt_out" };
        telemetry_csv_append("telemetry_uploads.csv", "timestamp,event,detail", fields, 2);
        event_count = 0;
        dropped_events = false;
        return;
    }

    size_t cap = 512 + (size_t)event_count * TELEMETRY_MAX_EVENT_CHARS;
    char *payload = (char *)malloc(cap);
    if (!payload)
        return;

    int pos = snprintf(payload, cap,
        "{\"schema_version\":1,\"game_version\":\"0.1.0\",\"run_id\":%d,\"timestamp\":%lld,\"events\":[",
        current_run_id > 0 ? current_run_id : g_state.meta.runs_completed + 1,
        now_unix());
    for (int i = 0; i < event_count && pos > 0 && (size_t)pos < cap; i++)
    {
        pos += snprintf(payload + pos, cap - (size_t)pos, "%s%s", i > 0 ? "," : "", event_buffer[i]);
    }
    if (pos > 0 && (size_t)pos < cap)
        snprintf(payload + pos, cap - (size_t)pos, "]}");

    bool ok = false;
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
    ok = telemetry_http_post(payload);
#else
    (void)payload;
    ok = false;
#endif

    const char *fields[] = {
        ok ? "upload_success" : "upload_failed",
        ok ? "" : "network_or_endpoint_unavailable"
    };
    telemetry_csv_append("telemetry_uploads.csv", "timestamp,event,detail", fields, 2);
    if (!ok)
        LOG_W(CAT_SCREEN, "Telemetry upload failed or is not configured.");

    free(payload);
    event_count = 0;
    dropped_events = false;
}

void telemetry_shutdown(void)
{
    event_count = 0;
    dropped_events = false;
}
