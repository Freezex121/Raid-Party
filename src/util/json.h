#ifndef UTIL_JSON_H
#define UTIL_JSON_H

#include <stdbool.h>

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT,
} JsonType;

typedef struct JsonValue JsonValue;

typedef struct {
    char *key;
    JsonValue *value;
} JsonPair;

struct JsonValue {
    JsonType type;
    union {
        bool boolean;
        double number;
        char *string;
        struct {
            JsonValue **items;
            int count;
        } array;
        struct {
            JsonPair *pairs;
            int count;
        } object;
    } as;
};

JsonValue *json_load_file(const char *path, char *error, int error_size);
void json_free(JsonValue *value);

const JsonValue *json_object_get(const JsonValue *object, const char *key);
const JsonValue *json_array_get(const JsonValue *array, int index);
int json_array_count(const JsonValue *array);

const char *json_string(const JsonValue *value, const char *fallback);
int json_int(const JsonValue *value, int fallback);
bool json_bool(const JsonValue *value, bool fallback);

#endif
