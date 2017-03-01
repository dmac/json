#ifndef JSON_H
#define JSON_H

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef enum JSONError {
    JSON_OK,
    JSON_EOF,
    JSON_UNEXPECTED,
    JSON_OOM,
} JSONError;

typedef enum JSONType {
    JSON_NULL,
    JSON_BOOL,
    JSON_STRING,
    JSON_NUMBER,
    JSON_ARRAY,
    JSON_OBJECT,

    JSON_ARRAY_END,
    JSON_OBJECT_END,
    JSON_COMMA,
} JSONType;

typedef struct JSONValue {
    JSONType type;
    char *   s;
    size_t   len;
    size_t   count; // number of elements in array or object

    union {
        bool               b;
        char *             s;
        double             n;
        struct JSONValue **elements; // array or object
    } v;
} JSONValue;

typedef struct JSONScanner {
    char * s;
    char * mem;
    size_t memsize;

    size_t memf;
    size_t memb;

    JSONValue *container;
} JSONScanner;

void json_scanner_init(JSONScanner *s, char *str, void *mem);

void *json_allocf(JSONScanner *s, size_t size);
JSONError json_pushb(JSONScanner *s, JSONValue *v);
JSONValue *json_popb(JSONScanner *s);

char *json_error(JSONError err);
char *json_type(JSONType type);

#ifdef JSON_IMPLEMENTATION

// Pseudocode for scanning:
//
// pushf: set JSONValue memory at front
// pushb: set JSONValue * memory at back
// popb: deallocate and return JSONValue * memory at back
// (memory at front is never deallocated since we save pointers to it)
//
// loop:
//     at value (object, array, string, number, bool, null):
//         pushf value v
//         if in container c:
//             increment c count
//             pushb value pointer &v
//         if v is container (object, array):
//             pushb value pointer &c
//             set container pointer to v
//         continue
//
//     at container c end:
//         copy and reverse c count pointers from back to front
//         set c elements pointer to start of new front pointers
//         popb c count pointers
//         set container pointer to value pointer at back
//         popb new container pointer
//         continue

static char *errors[] = {
    "OK",
    "unexpected end of input",
    "unexpected input",
    "out of memory",
};

char *json_error(JSONError err) {
    return errors[err];
}

static char *types[] = {
    "null",   // JSON_NULL,
    "bool",   // JSON_BOOL,
    "string", // JSON_STRING,
    "number", // JSON_NUMBER,
    "array",  // JSON_ARRAY,
    "object", // JSON_OBJECT,

    "array_end",  // JSON_ARRAY_END,
    "object_end", // JSON_OBJECT_END,
    "comma",      // JSON_COMMA,
};

char *json_type(JSONType type) {
    return types[type];
}

size_t json_memavail(JSONScanner *s) {
    if (s->memf > s->memsize || s->memb > s->memsize || s->memf > s->memb) {
        return 0;
    }
    return s->memb - s->memf;
}

void *json_allocf(JSONScanner *s, size_t size) {
    if (json_memavail(s) < size) {
        return NULL;
    }
    void *p = s->mem + s->memf;
    s->memf += size;
    return p;
}

JSONError json_pushb(JSONScanner *s, JSONValue *v) {
    if (json_memavail(s) < sizeof(v)) {
        return JSON_OOM;
    }
    s->memb -= sizeof(v);
    JSONValue **p = (JSONValue **)(s->mem + s->memb);
    *p            = v;
    return JSON_OK;
}

JSONValue *json_popb(JSONScanner *s) {
    if (s->memb >= s->memsize) {
        return NULL;
    }
    JSONValue *p = *(JSONValue **)(s->mem + s->memb);
    s->memb += sizeof(p);
    return p;
}

JSONScanner json_scanner_new(char *str, void *mem, size_t memsize) {
    JSONScanner s = {0};
    s.s           = str;
    s.mem         = mem;
    s.memsize     = memsize;
    s.memb        = memsize;
    return s;
}

void json_scan_whitespace(JSONScanner *s) {
    while (isspace(*s->s)) {
        s->s++;
    }
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
    v->v.s = json_allocf(s, v->len - 1); // +1 for null-terminator, -2 for quotes
    if (v->v.s == NULL) {
        return JSON_OOM;
    }
    memcpy(v->v.s, s->s + 1, v->len - 2);
    v->v.s[v->len - 1] = '\0';
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

JSONError json_scan_array_start(JSONScanner *s, JSONValue *v) {
    v->type  = JSON_ARRAY;
    v->s     = s->s;
    v->len   = 0;
    v->count = 0;
    if (s->s[v->len] == '\0') {
        return JSON_EOF;
    }
    if (s->s[v->len] != '[') {
        return JSON_UNEXPECTED;
    }
    v->len++;
    s->s += v->len;
    return JSON_OK;
}

JSONError json_scan_value(JSONScanner *s, JSONType type, JSONValue *v) {
    JSONError err = JSON_OK;
    if (type == JSON_NULL) {
        err = json_scan_null(s, v);
    } else if (type == JSON_BOOL) {
        err = json_scan_bool(s, v);
    } else if (type == JSON_NUMBER) {
        err = json_scan_number(s, v);
    } else if (type == JSON_STRING) {
        err = json_scan_string(s, v);
    } else if (type == JSON_ARRAY) {
        err = json_scan_array_start(s, v);
    } else if (type == JSON_OBJECT) {
        abort(); // TODO
    } else {
        err = JSON_UNEXPECTED;
    }
    return err;
}

JSONError json_scan_to_next_token(JSONScanner *s, JSONType *t) {
    JSONError err = JSON_OK;
    json_scan_whitespace(s);
    char c = *s->s;
    if (c == 'n') {
        *t = JSON_NULL;
    } else if (c == 't' || c == 'f') {
        *t = JSON_BOOL;
    } else if (c == '"') {
        *t = JSON_STRING;
    } else if (c == '-' || (c >= '0' && c <= '9')) {
        *t = JSON_NUMBER;
    } else if (c == '[') {
        *t = JSON_ARRAY;
    } else if (c == '{') {
        *t = JSON_OBJECT;
    } else if (c == ']') {
        *t = JSON_ARRAY_END;
    } else if (c == '}') {
        *t = JSON_OBJECT_END;
    } else if (c == ',') {
        *t = JSON_COMMA;
    } else if (c == '\0') {
        err = JSON_EOF;
    } else {
        err = JSON_UNEXPECTED;
    }
    return err;
}

JSONError json_finalize_container(JSONScanner *s) {
    size_t size = sizeof(JSONValue *) * s->container->count;
    if (size / sizeof(JSONValue *) != s->container->count) {
        // size_t overflow
        // TODO(dmac) Consider using something faster than division.
        return JSON_OOM;
    }
    if (size > json_memavail(s)) {
        return JSON_OOM;
    }
    if (size > s->memsize - s->memb) {
        // TODO(dmac) This isn't really OOM, it represents a programmer error
        // since we're trying to pop more off the back than exists.
        return JSON_OOM;
    }
    s->container->v.elements  = (JSONValue **)(s->mem + s->memf);
    JSONValue **back_pointers = (JSONValue **)(s->mem + s->memb);
    for (size_t i = 0; i < s->container->count; i++) {
        s->container->v.elements[i] = back_pointers[s->container->count - 1 - i];
    }
    s->memf += size;
    s->memb += size;
    s->container = json_popb(s);
    s->s++;
    return JSON_OK;
}

JSONError json_parse(JSONScanner *s, JSONValue **root) {
    JSONError err;
    JSONType  type;
    while (true) {
        err = json_scan_to_next_token(s, &type);
        if (err == JSON_EOF) {
            // TODO(dmac) This isn't exactly right.
            // Ensure we've parsed exactly one top-level value
            // and that we're not in a container.
            break;
        } else if (err != JSON_OK) {
            return err;
        }
        if (type == JSON_NULL ||
            type == JSON_BOOL ||
            type == JSON_STRING ||
            type == JSON_NUMBER ||
            type == JSON_ARRAY ||
            type == JSON_OBJECT) {
            JSONValue *v = json_allocf(s, sizeof(*v));
            if (v == NULL) {
                return JSON_OOM;
            }
            if ((err = json_scan_value(s, type, v)) != JSON_OK) {
                return err;
            }
            if (*root == NULL) {
                *root = v;
            }
            if (s->container != NULL) {
                s->container->count++;
                if ((err = json_pushb(s, v)) != JSON_OK) {
                    return err;
                }
            }
            if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) {
                if ((err = json_pushb(s, s->container)) != JSON_OK) {
                    return err;
                }
                s->container = v;
            }
        } else if (type == JSON_ARRAY_END || type == JSON_OBJECT_END) {
            if ((err = json_finalize_container(s)) != JSON_OK) {
                return err;
            }
        } else {
            return JSON_UNEXPECTED;
        }
        if (s->container != NULL &&
            (type == JSON_NULL || type == JSON_BOOL || type == JSON_STRING ||
             type == JSON_NUMBER || type == JSON_ARRAY_END || type == JSON_OBJECT_END)) {
            if ((err = json_scan_to_next_token(s, &type)) != JSON_OK) {
                return err;
            }
            if (!(type == JSON_COMMA ||
                  (s->container->type == JSON_ARRAY && type == JSON_ARRAY_END) ||
                  (s->container->type == JSON_OBJECT && type == JSON_OBJECT_END))) {
                return JSON_UNEXPECTED;
            }
            if (type == JSON_COMMA) {
                s->s++;
            }
        }
    }
    *root = (JSONValue *)s->mem;
    return JSON_OK;
}

#endif
#endif
