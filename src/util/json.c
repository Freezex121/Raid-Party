#include "json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *src;
    int pos;
    int len;
    char error[160];
} JsonParser;

static void set_error(JsonParser *p, const char *msg)
{
    if (!p->error[0])
        snprintf(p->error, sizeof(p->error), "%s at byte %d", msg, p->pos);
}

static void skip_ws(JsonParser *p)
{
    while (p->pos < p->len && isspace((unsigned char)p->src[p->pos]))
        p->pos++;
}

static char *dup_range(const char *start, int len)
{
    char *out = (char *)malloc((size_t)len + 1);
    if (!out) return NULL;
    memcpy(out, start, (size_t)len);
    out[len] = '\0';
    return out;
}

static JsonValue *new_value(JsonType type)
{
    JsonValue *value = (JsonValue *)calloc(1, sizeof(JsonValue));
    if (value)
        value->type = type;
    return value;
}

static bool append_char(char **buf, int *len, int *cap, char ch)
{
    if (*len + 1 >= *cap)
    {
        int next = *cap > 0 ? *cap * 2 : 32;
        char *grown = (char *)realloc(*buf, (size_t)next);
        if (!grown) return false;
        *buf = grown;
        *cap = next;
    }
    (*buf)[(*len)++] = ch;
    return true;
}

static char *parse_raw_string(JsonParser *p)
{
    if (p->src[p->pos] != '"')
    {
        set_error(p, "expected string");
        return NULL;
    }
    p->pos++;

    char *buf = NULL;
    int len = 0;
    int cap = 0;

    while (p->pos < p->len)
    {
        char ch = p->src[p->pos++];
        if (ch == '"')
        {
            if (!append_char(&buf, &len, &cap, '\0'))
            {
                free(buf);
                set_error(p, "out of memory");
                return NULL;
            }
            return buf;
        }

        if (ch == '\\')
        {
            if (p->pos >= p->len)
            {
                free(buf);
                set_error(p, "unterminated escape");
                return NULL;
            }
            char esc = p->src[p->pos++];
            switch (esc)
            {
                case '"': ch = '"'; break;
                case '\\': ch = '\\'; break;
                case '/': ch = '/'; break;
                case 'b': ch = '\b'; break;
                case 'f': ch = '\f'; break;
                case 'n': ch = '\n'; break;
                case 'r': ch = '\r'; break;
                case 't': ch = '\t'; break;
                case 'u':
                    if (p->pos + 4 <= p->len)
                        p->pos += 4;
                    ch = '?';
                    break;
                default:
                    free(buf);
                    set_error(p, "invalid escape");
                    return NULL;
            }
        }

        if (!append_char(&buf, &len, &cap, ch))
        {
            free(buf);
            set_error(p, "out of memory");
            return NULL;
        }
    }

    free(buf);
    set_error(p, "unterminated string");
    return NULL;
}

static JsonValue *parse_value(JsonParser *p);

static JsonValue *parse_string(JsonParser *p)
{
    char *text = parse_raw_string(p);
    if (!text) return NULL;
    JsonValue *value = new_value(JSON_STRING);
    if (!value)
    {
        free(text);
        set_error(p, "out of memory");
        return NULL;
    }
    value->as.string = text;
    return value;
}

static JsonValue *parse_number(JsonParser *p)
{
    const char *start = p->src + p->pos;
    char *end = NULL;
    double number = strtod(start, &end);
    if (end == start)
    {
        set_error(p, "expected number");
        return NULL;
    }
    p->pos += (int)(end - start);
    JsonValue *value = new_value(JSON_NUMBER);
    if (!value)
    {
        set_error(p, "out of memory");
        return NULL;
    }
    value->as.number = number;
    return value;
}

static JsonValue *parse_literal(JsonParser *p, const char *literal, JsonType type, bool boolean)
{
    int n = (int)strlen(literal);
    if (p->pos + n > p->len || strncmp(p->src + p->pos, literal, (size_t)n) != 0)
    {
        set_error(p, "invalid literal");
        return NULL;
    }
    p->pos += n;
    JsonValue *value = new_value(type);
    if (!value)
    {
        set_error(p, "out of memory");
        return NULL;
    }
    if (type == JSON_BOOL)
        value->as.boolean = boolean;
    return value;
}

static bool append_value(JsonValue ***items, int *count, int *cap, JsonValue *value)
{
    if (*count >= *cap)
    {
        int next = *cap > 0 ? *cap * 2 : 8;
        JsonValue **grown = (JsonValue **)realloc(*items, sizeof(JsonValue *) * (size_t)next);
        if (!grown) return false;
        *items = grown;
        *cap = next;
    }
    (*items)[(*count)++] = value;
    return true;
}

static bool append_pair(JsonPair **pairs, int *count, int *cap, char *key, JsonValue *value)
{
    if (*count >= *cap)
    {
        int next = *cap > 0 ? *cap * 2 : 8;
        JsonPair *grown = (JsonPair *)realloc(*pairs, sizeof(JsonPair) * (size_t)next);
        if (!grown) return false;
        *pairs = grown;
        *cap = next;
    }
    (*pairs)[*count].key = key;
    (*pairs)[*count].value = value;
    (*count)++;
    return true;
}

static JsonValue *parse_array(JsonParser *p)
{
    JsonValue *array = new_value(JSON_ARRAY);
    if (!array)
    {
        set_error(p, "out of memory");
        return NULL;
    }
    p->pos++;
    skip_ws(p);
    if (p->pos < p->len && p->src[p->pos] == ']')
    {
        p->pos++;
        return array;
    }

    int cap = 0;
    while (p->pos < p->len)
    {
        JsonValue *item = parse_value(p);
        if (!item)
        {
            json_free(array);
            return NULL;
        }
        if (!append_value(&array->as.array.items, &array->as.array.count, &cap, item))
        {
            json_free(item);
            json_free(array);
            set_error(p, "out of memory");
            return NULL;
        }

        skip_ws(p);
        if (p->pos < p->len && p->src[p->pos] == ',')
        {
            p->pos++;
            skip_ws(p);
            continue;
        }
        if (p->pos < p->len && p->src[p->pos] == ']')
        {
            p->pos++;
            return array;
        }
        json_free(array);
        set_error(p, "expected comma or ]");
        return NULL;
    }

    json_free(array);
    set_error(p, "unterminated array");
    return NULL;
}

static JsonValue *parse_object(JsonParser *p)
{
    JsonValue *object = new_value(JSON_OBJECT);
    if (!object)
    {
        set_error(p, "out of memory");
        return NULL;
    }
    p->pos++;
    skip_ws(p);
    if (p->pos < p->len && p->src[p->pos] == '}')
    {
        p->pos++;
        return object;
    }

    int cap = 0;
    while (p->pos < p->len)
    {
        char *key = parse_raw_string(p);
        if (!key)
        {
            json_free(object);
            return NULL;
        }

        skip_ws(p);
        if (p->pos >= p->len || p->src[p->pos] != ':')
        {
            free(key);
            json_free(object);
            set_error(p, "expected colon");
            return NULL;
        }
        p->pos++;

        JsonValue *value = parse_value(p);
        if (!value)
        {
            free(key);
            json_free(object);
            return NULL;
        }

        if (!append_pair(&object->as.object.pairs, &object->as.object.count, &cap, key, value))
        {
            free(key);
            json_free(value);
            json_free(object);
            set_error(p, "out of memory");
            return NULL;
        }

        skip_ws(p);
        if (p->pos < p->len && p->src[p->pos] == ',')
        {
            p->pos++;
            skip_ws(p);
            continue;
        }
        if (p->pos < p->len && p->src[p->pos] == '}')
        {
            p->pos++;
            return object;
        }
        json_free(object);
        set_error(p, "expected comma or }");
        return NULL;
    }

    json_free(object);
    set_error(p, "unterminated object");
    return NULL;
}

static JsonValue *parse_value(JsonParser *p)
{
    skip_ws(p);
    if (p->pos >= p->len)
    {
        set_error(p, "unexpected end of file");
        return NULL;
    }

    char ch = p->src[p->pos];
    if (ch == '"') return parse_string(p);
    if (ch == '{') return parse_object(p);
    if (ch == '[') return parse_array(p);
    if (ch == '-' || isdigit((unsigned char)ch)) return parse_number(p);
    if (ch == 't') return parse_literal(p, "true", JSON_BOOL, true);
    if (ch == 'f') return parse_literal(p, "false", JSON_BOOL, false);
    if (ch == 'n') return parse_literal(p, "null", JSON_NULL, false);

    set_error(p, "unexpected token");
    return NULL;
}

static char *read_file(const char *path, int *out_len)
{
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size < 0)
    {
        fclose(file);
        return NULL;
    }

    char *buffer = (char *)malloc((size_t)size + 1);
    if (!buffer)
    {
        fclose(file);
        return NULL;
    }
    size_t read = fread(buffer, 1, (size_t)size, file);
    fclose(file);
    buffer[read] = '\0';
    if (out_len) *out_len = (int)read;
    return buffer;
}

JsonValue *json_load_file(const char *path, char *error, int error_size)
{
    int len = 0;
    char *text = read_file(path, &len);
    if (!text)
    {
        if (error && error_size > 0)
            snprintf(error, (size_t)error_size, "could not read %s", path);
        return NULL;
    }

    JsonParser p = { text, 0, len, "" };
    JsonValue *root = parse_value(&p);
    skip_ws(&p);
    if (root && p.pos < p.len)
    {
        json_free(root);
        root = NULL;
        set_error(&p, "trailing characters");
    }

    if (!root && error && error_size > 0)
        snprintf(error, (size_t)error_size, "%s: %s", path, p.error[0] ? p.error : "invalid JSON");

    free(text);
    return root;
}

void json_free(JsonValue *value)
{
    if (!value) return;
    if (value->type == JSON_STRING)
    {
        free(value->as.string);
    }
    else if (value->type == JSON_ARRAY)
    {
        for (int i = 0; i < value->as.array.count; i++)
            json_free(value->as.array.items[i]);
        free(value->as.array.items);
    }
    else if (value->type == JSON_OBJECT)
    {
        for (int i = 0; i < value->as.object.count; i++)
        {
            free(value->as.object.pairs[i].key);
            json_free(value->as.object.pairs[i].value);
        }
        free(value->as.object.pairs);
    }
    free(value);
}

const JsonValue *json_object_get(const JsonValue *object, const char *key)
{
    if (!object || object->type != JSON_OBJECT || !key) return NULL;
    for (int i = 0; i < object->as.object.count; i++)
        if (strcmp(object->as.object.pairs[i].key, key) == 0)
            return object->as.object.pairs[i].value;
    return NULL;
}

const JsonValue *json_array_get(const JsonValue *array, int index)
{
    if (!array || array->type != JSON_ARRAY || index < 0 || index >= array->as.array.count) return NULL;
    return array->as.array.items[index];
}

int json_array_count(const JsonValue *array)
{
    if (!array || array->type != JSON_ARRAY) return 0;
    return array->as.array.count;
}

const char *json_string(const JsonValue *value, const char *fallback)
{
    return value && value->type == JSON_STRING ? value->as.string : fallback;
}

int json_int(const JsonValue *value, int fallback)
{
    return value && value->type == JSON_NUMBER ? (int)value->as.number : fallback;
}

bool json_bool(const JsonValue *value, bool fallback)
{
    return value && value->type == JSON_BOOL ? value->as.boolean : fallback;
}
