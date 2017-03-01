#ifndef JSON_H
#define JSON_H

#include <ctype.h>
#include <stdbool.h>
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
    JSON_COLON,
} JSONType;

typedef struct JSONValue JSONValue;
struct JSONValue {
    JSONType type;
    size_t   count; // number of elements in array or object
    char *   key;   // key for object elements

    union {
        bool        b;
        char *      s;
        double      n;
        JSONValue **e; // elements of array or object
    } v;
};

typedef struct JSONParser {
    char *s;

    char * mem;
    size_t memsize;
    size_t memf;
    size_t memb;

    JSONValue *container;
} JSONParser;

void json_parser_init(JSONParser *p, char *str, void *mem);

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
    "NULL",
    "BOOL",
    "STRING",
    "NUMBER",
    "ARRAY",
    "OBJECT",

    "ARRAY_END",
    "OBJECT_END",
    "COMMA",
    "COLON",
};

void json_debug_print(JSONValue *v, int level) {
    printf("%*s", 4 * level, " ");
    if (v->key != NULL) {
        printf("\"%s\": ", v->key);
    }
    switch (v->type) {
    case JSON_NULL:
        printf("null\n");
        break;
    case JSON_BOOL:
        printf("%s\n", v->v.b ? "true" : "false");
        break;
    case JSON_STRING:
        printf("\"%s\"\n", v->v.s);
        break;
    case JSON_NUMBER:
        printf("%g\n", v->v.n);
        break;
    case JSON_ARRAY:
    case JSON_OBJECT:
        printf("%s[%zu]\n", json_type(v->type), v->count);
        for (size_t i = 0; i < v->count; i++) {
            json_debug_print(v->v.e[i], level + 1);
        }
        break;
    default:
        printf("unknown type: %s\n", json_type(v->type));
        break;
    }
}

char *json_type(JSONType type) {
    return types[type];
}

size_t json_memavail(JSONParser *p) {
    if (p->memf > p->memsize || p->memb > p->memsize || p->memf > p->memb) {
        return 0;
    }
    return p->memb - p->memf;
}

void *json_allocf(JSONParser *p, size_t size) {
    if (json_memavail(p) < size) {
        return NULL;
    }
    void *m = p->mem + p->memf;
    p->memf += size;
    return m;
}

JSONError json_pushb(JSONParser *p, JSONValue *v) {
    if (json_memavail(p) < sizeof(v)) {
        return JSON_OOM;
    }
    p->memb -= sizeof(v);
    JSONValue **pv = (JSONValue **)(p->mem + p->memb);
    *pv            = v;
    return JSON_OK;
}

JSONValue *json_popb(JSONParser *p) {
    if (p->memb >= p->memsize) {
        return NULL;
    }
    JSONValue *pv = *(JSONValue **)(p->mem + p->memb);
    p->memb += sizeof(p);
    return pv;
}

JSONParser json_parser_new(char *str, void *mem, size_t memsize) {
    JSONParser p = {0};
    p.s          = str;
    p.mem        = mem;
    p.memsize    = memsize;
    p.memb       = memsize;
    return p;
}

void json_scan_whitespace(JSONParser *p) {
    while (isspace(*p->s)) {
        p->s++;
    }
}

JSONError json_scan_null(JSONParser *p, JSONValue *v) {
    char * expect = "null";
    size_t len    = strlen(expect);
    v->type       = JSON_NULL;
    for (size_t i = 0; i < len; i++) {
        if (p->s[i] == '\0') {
            return JSON_EOF;
        }
        if (p->s[i] != expect[i]) {
            return JSON_UNEXPECTED;
        }
    }
    p->s += len;
    return JSON_OK;
}

JSONError json_scan_bool(JSONParser *p, JSONValue *v) {
    char * expect;
    size_t len = 0;
    v->type    = JSON_BOOL;
    if (p->s[len] == 't') {
        expect = "true";
        v->v.b = true;
    } else if (p->s[len] == 'f') {
        expect = "false";
        v->v.b = false;
    } else if (p->s[len] == '\0') {
        return JSON_EOF;
    } else {
        return JSON_UNEXPECTED;
    }
    for (size_t i = 0; i < strlen(expect); i++) {
        if (p->s[i] == '\0') {
            return JSON_EOF;
        }
        if (p->s[i] != expect[i]) {
            return JSON_UNEXPECTED;
        }
        len++;
    }
    p->s += len;
    return JSON_OK;
}

JSONError json_scan_string(JSONParser *p, JSONValue *v) {
    size_t len = 0;
    v->type    = JSON_STRING;
    if (*p->s == '\0') {
        return JSON_EOF;
    }
    if (p->s[len] != '"') {
        return JSON_UNEXPECTED;
    }
    len++;
    while (p->s[len] != '"') {
        if (p->s[len] == '\0') {
            return JSON_EOF;
        }
        if (p->s[len] != '\\') {
            len++;
            continue;
        }
        // scan escape sequence
        len++;
        if (p->s[len] == '\0') {
            return JSON_EOF;
        }
        switch (p->s[len]) {
        case '"':
        case '\\':
        case '/':
        case 'b':
        case 'f':
        case 'n':
        case 'r':
        case 't':
            len++;
            continue;
        }
        // scan unicode escape sequence
        if (p->s[len] != 'u') {
            return JSON_UNEXPECTED;
        }
        len++;
        for (int i = 0; i < 4; i++) {
            if (p->s[len] == '\0') {
                return JSON_EOF;
            }
            if (!isxdigit(p->s[len])) {
                return JSON_UNEXPECTED;
            }
            len++;
        }
    }
    len++;
    v->v.s = json_allocf(p, len - 1); // +1 for null-terminator, -2 for quotes
    if (v->v.s == NULL) {
        return JSON_OOM;
    }
    memcpy(v->v.s, p->s + 1, len - 2);
    v->v.s[len - 2] = '\0';
    p->s += len;
    return JSON_OK;
}

JSONError json_scan_number(JSONParser *p, JSONValue *v) {
    size_t len = 0;
    v->type    = JSON_NUMBER;

    // scan sign
    if (p->s[len] == '\0') {
        return JSON_EOF;
    }
    if (p->s[len] == '-') {
        len++;
    }
    if (p->s[len] == '\0') {
        return JSON_EOF;
    }

    // scan integral
    if (p->s[len] == '0') {
        len++;
    } else if (p->s[len] >= '1' && p->s[len] <= '9') {
        while (isdigit(p->s[len])) {
            len++;
        }
    } else {
        return JSON_UNEXPECTED;
    }

    // scan decimal
    if (p->s[len] == '.') {
        len++;
        if (p->s[len] == '\0') {
            return JSON_EOF;
        }
        if (!isdigit(p->s[len])) {
            return JSON_UNEXPECTED;
        }
        while (isdigit(p->s[len])) {
            len++;
        }
    }

    // scan exponent
    if (p->s[len] == 'e' || p->s[len] == 'E') {
        len++;
        if (p->s[len] == '\0') {
            return JSON_EOF;
        }
        if (p->s[len] == '+' || p->s[len] == '-') {
            len++;
        }
        if (p->s[len] == '\0') {
            return JSON_EOF;
        }
        if (!isdigit(p->s[len])) {
            return JSON_UNEXPECTED;
        }
        while (isdigit(p->s[len])) {
            len++;
        }
    }

    char *end;
    v->v.n = strtod(p->s, &end);
    if (p->s + len != end) {
        return JSON_UNEXPECTED;
    }

    p->s += len;
    return JSON_OK;
}

JSONError json_scan_array_start(JSONParser *p, JSONValue *v) {
    size_t len = 0;
    v->type    = JSON_ARRAY;
    v->count   = 0;
    if (p->s[len] == '\0') {
        return JSON_EOF;
    }
    if (p->s[len] != '[') {
        return JSON_UNEXPECTED;
    }
    len++;
    p->s += len;
    return JSON_OK;
}

JSONError json_scan_object_start(JSONParser *p, JSONValue *v) {
    size_t len = 0;
    v->type    = JSON_OBJECT;
    v->count   = 0;
    if (p->s[len] == '\0') {
        return JSON_EOF;
    }
    if (p->s[len] != '{') {
        return JSON_UNEXPECTED;
    }
    len++;
    p->s += len;
    return JSON_OK;
}

JSONError json_scan_value(JSONParser *p, JSONType type, JSONValue *v) {
    JSONError err = JSON_OK;
    if (type == JSON_NULL) {
        err = json_scan_null(p, v);
    } else if (type == JSON_BOOL) {
        err = json_scan_bool(p, v);
    } else if (type == JSON_NUMBER) {
        err = json_scan_number(p, v);
    } else if (type == JSON_STRING) {
        err = json_scan_string(p, v);
    } else if (type == JSON_ARRAY) {
        err = json_scan_array_start(p, v);
    } else if (type == JSON_OBJECT) {
        err = json_scan_object_start(p, v);
    } else {
        err = JSON_UNEXPECTED;
    }
    return err;
}

JSONError json_scan_to_next_token(JSONParser *p, JSONType *t) {
    JSONError err = JSON_OK;
    json_scan_whitespace(p);
    char c = *p->s;
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
    } else if (c == ':') {
        *t = JSON_COLON;
    } else if (c == '\0') {
        err = JSON_EOF;
    } else {
        err = JSON_UNEXPECTED;
    }
    return err;
}

JSONError json_finalize_container(JSONParser *p) {
    p->container->v.e         = (JSONValue **)(p->mem + p->memf);
    JSONValue **back_pointers = (JSONValue **)(p->mem + p->memb);
    for (size_t i = 0; i < p->container->count; i++) {
        if (p->memf + sizeof(JSONValue *) > p->memb) {
            return JSON_OOM;
        }
        if (p->memb + sizeof(JSONValue *) > p->memsize) {
            return JSON_OOM;
        }
        p->container->v.e[i] = back_pointers[p->container->count - 1 - i];
        p->memf += sizeof(JSONValue *);
        p->memb += sizeof(JSONValue *);
    }
    p->container = json_popb(p);
    p->s++;
    return JSON_OK;
}

JSONError json_parse(JSONParser *p, JSONValue **root) {
    JSONError err;
    JSONType  type;
    while (true) {
        err = json_scan_to_next_token(p, &type);
        if (err == JSON_EOF) {
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
            JSONValue *v = json_allocf(p, sizeof(*v));
            if (v == NULL) {
                return JSON_OOM;
            }
            *v = (JSONValue){0};

            if (p->container != NULL && p->container->type == JSON_OBJECT) {
                // parse string key and colon
                if (type != JSON_STRING) {
                    return JSON_UNEXPECTED;
                }
                if ((err = json_scan_string(p, v)) != JSON_OK) {
                    return err;
                }
                v->key = v->v.s;
                v->v.s = NULL;
                if ((err = json_scan_to_next_token(p, &type)) != JSON_OK) {
                    return err;
                }
                if (type != JSON_COLON) {
                    return JSON_UNEXPECTED;
                }
                p->s++;
                if ((err = json_scan_to_next_token(p, &type)) != JSON_OK) {
                    return err;
                }
            }

            if ((err = json_scan_value(p, type, v)) != JSON_OK) {
                return err;
            }
            if (p->container != NULL) {
                p->container->count++;
                if ((err = json_pushb(p, v)) != JSON_OK) {
                    return err;
                }
            }
            if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) {
                if ((err = json_pushb(p, p->container)) != JSON_OK) {
                    return err;
                }
                p->container = v;
            }
        } else if (p->container != NULL &&
                   ((p->container->type == JSON_ARRAY && type == JSON_ARRAY_END) ||
                    (p->container->type == JSON_OBJECT && type == JSON_OBJECT_END))) {
            if ((err = json_finalize_container(p)) != JSON_OK) {
                return err;
            }
        } else {
            return JSON_UNEXPECTED;
        }

        // skip over comma
        if (p->container != NULL && p->container->count > 0) {
            if ((err = json_scan_to_next_token(p, &type)) != JSON_OK) {
                return err;
            }
            if (!(type == JSON_COMMA ||
                  (p->container->type == JSON_ARRAY && type == JSON_ARRAY_END) ||
                  (p->container->type == JSON_OBJECT && type == JSON_OBJECT_END))) {
                return JSON_UNEXPECTED;
            }
            if (type == JSON_COMMA) {
                p->s++;
            }
        }
    }
    if (p->memf == 0) {
        // no input
        return JSON_EOF;
    }
    *root = (JSONValue *)p->mem;
    return JSON_OK;
}

#endif
#endif
