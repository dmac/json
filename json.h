#ifndef JSON_H
#define JSON_H

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct JSONScanner {
    char *s;
} JSONScanner;

typedef enum JSONError {
    JSON_OK,
    JSON_EOF,
    JSON_UNEXPECTED,
} JSONError;

typedef enum JSONType {
    JSON_NULL,
    JSON_BOOL,
    JSON_STRING,
    JSON_NUMBER,
    JSON_ARRAY,
    JSON_OBJECT,
} JSONType;

typedef struct JSONObject {
    int tmp;
} JSONObject;

typedef struct JSONArray {
    int tmp;
} JSONArray;

typedef struct JSONValue {
    JSONType type;
    char *   s;
    size_t   len;

    union {
        bool       b;
        char *     str;
        double     n;
        JSONObject obj;
        JSONArray  arr;
    } v;
} JSONValue;

char *json_error(JSONError err);

#ifdef JSON_IMPLEMENTATION

static char *errors[] = {
    "OK",
    "unexpected end of input",
    "unexpected input",
};

char *json_error(JSONError err) {
    return errors[err];
}

JSONError json_scan_null(JSONScanner *s, JSONValue *v) {
    char *expect = "null";
    v->type      = JSON_NULL;
    v->s         = s->s;
    v->len       = strlen(expect);
    for (size_t i = 0; i < v->len; i++) {
        if (s->s[i] == '\0') {
            return JSON_EOF;
        }
        if (s->s[i] != expect[i]) {
            return JSON_UNEXPECTED;
        }
    }
    s->s += v->len;
    return JSON_OK;
}

JSONError json_scan_bool(JSONScanner *s, JSONValue *v) {
    char *expect;
    v->type = JSON_BOOL;
    v->s    = s->s;
    v->len  = 0;
    if (s->s[v->len] == 't') {
        expect = "true";
        v->v.b = true;
    } else if (s->s[v->len] == 'f') {
        expect = "false";
        v->v.b = false;
    } else if (s->s[v->len] == '\0') {
        return JSON_EOF;
    } else {
        return JSON_UNEXPECTED;
    }
    v->len     = strlen(expect);
    size_t len = strlen(expect);
    for (size_t i = 0; i < len; i++) {
        if (s->s[i] == '\0') {
            return JSON_EOF;
        }
        if (s->s[i] != expect[i]) {
            return JSON_UNEXPECTED;
        }
        v->len++;
    }
    s->s += v->len;
    return JSON_OK;
}

JSONError json_scan_string(JSONScanner *s, JSONValue *v) {
    v->type = JSON_STRING;
    v->s    = s->s;
    v->len  = 0;
    if (*s->s == '\0') {
        return JSON_EOF;
    }
    if (s->s[v->len] != '"') {
        return JSON_UNEXPECTED;
    }
    v->len++;
    while (s->s[v->len] != '"') {
        if (s->s[v->len] == '\0') {
            return JSON_EOF;
        }
        if (s->s[v->len] != '\\') {
            v->len++;
            continue;
        }
        // scan escape sequence
        v->len++;
        if (s->s[v->len] == '\0') {
            return JSON_EOF;
        }
        switch (s->s[v->len]) {
        case '"':
        case '\\':
        case '/':
        case 'b':
        case 'f':
        case 'n':
        case 'r':
        case 't':
            v->len++;
            continue;
        }
        // scan unicode escape sequence
        if (s->s[v->len] != 'u') {
            return JSON_UNEXPECTED;
        }
        v->len++;
        for (int i = 0; i < 4; i++) {
            if (s->s[v->len] == '\0') {
                return JSON_EOF;
            }
            if (!isxdigit(s->s[v->len])) {
                return JSON_UNEXPECTED;
            }
            v->len++;
        }
    }
    v->len++;
    // TODO(dmac) Allocate and set v->v.s.
    s->s += v->len;
    return JSON_OK;
}

JSONError json_scan_number(JSONScanner *s, JSONValue *v) {
    v->type = JSON_NUMBER;
    v->s    = s->s;
    v->len  = 0;

    // scan sign
    if (s->s[v->len] == '\0') {
        return JSON_EOF;
    }
    if (s->s[v->len] == '-') {
        v->len++;
    }
    if (s->s[v->len] == '\0') {
        return JSON_EOF;
    }

    // scan integral
    if (s->s[v->len] == '0') {
        v->len++;
    } else if (s->s[v->len] >= '1' && s->s[v->len] <= '9') {
        while (isdigit(s->s[v->len])) {
            v->len++;
        }
    } else {
        return JSON_UNEXPECTED;
    }

    // scan decimal
    if (s->s[v->len] == '.') {
        v->len++;
        if (s->s[v->len] == '\0') {
            return JSON_EOF;
        }
        if (!isdigit(s->s[v->len])) {
            return JSON_UNEXPECTED;
        }
        while (isdigit(s->s[v->len])) {
            v->len++;
        }
    }

    // scan exponent
    if (s->s[v->len] == 'e' || s->s[v->len] == 'E') {
        v->len++;
        if (s->s[v->len] == '\0') {
            return JSON_EOF;
        }
        if (s->s[v->len] == '+' || s->s[v->len] == '-') {
            v->len++;
        }
        if (s->s[v->len] == '\0') {
            return JSON_EOF;
        }
        if (!isdigit(s->s[v->len])) {
            return JSON_UNEXPECTED;
        }
        while (isdigit(s->s[v->len])) {
            v->len++;
        }
    }

    char *end;
    v->v.n = strtod(s->s, &end);
    if (s->s + v->len != end) {
        return JSON_UNEXPECTED;
    }

    s->s += v->len;
    return JSON_OK;
}

#endif
#endif
