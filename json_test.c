#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#define JSON_IMPLEMENTATION
#include "json.h"

typedef struct Test {
    int   total;
    int   passed;
    bool  curr_passed;
    char *curr_test;
} Test;

typedef void (*TestFn)(Test *);

void test(Test *t, char *name, TestFn testfn) {
    t->total++;
    t->curr_test   = name;
    t->curr_passed = true;
    testfn(t);
    if (t->curr_passed) {
        t->passed++;
    }
}

void test_fail(Test *t, char *format, ...) {
    if (t->curr_passed) {
        printf("%s\n", t->curr_test);
        t->curr_passed = false;
    }
    va_list args;
    va_start(args, format);
    printf("    ");
    vprintf(format, args);
    printf("\n");
}

bool test_summary(Test *t) {
    printf("%d / %d ", t->passed, t->total);
    if (t->passed == t->total) {
        printf("PASS\n");
        return true;
    }
    printf("FAIL\n");
    return false;
}

#define TEST(t, testfn) test(t, #testfn, testfn)
#define ALEN(a) (sizeof(a) / sizeof(*(a)))

void test_json_scan_null(Test *t) {
    struct {
        char *    s;
        JSONValue v;
        JSONError err;
    } tt[] = {
        {"", {.type = JSON_NULL}, JSON_EOF},
        {"nu", {.type = JSON_NULL}, JSON_EOF},
        {"asdf", {.type = JSON_NULL}, JSON_UNEXPECTED},
        {"null", {.type = JSON_NULL}, JSON_OK},
        {"nullx", {.type = JSON_NULL}, JSON_OK},
    };
    for (int i = 0; i < ALEN(tt); i++) {
        JSONScanner s = {tt[i].s};
        JSONValue   v;
        JSONError   err = json_scan_null(&s, &v);
        if (err != tt[i].err) {
            test_fail(t, "%d error: got %s; want %s", i, json_error(err), json_error(tt[i].err));
            continue;
        }
        if (v.type != tt[i].v.type) {
            test_fail(t, "%d type: got %d; want %d", i, v.type, tt[i].v.type);
            continue;
        }
    }
}

void test_json_scan_bool(Test *t) {
    struct {
        char *    s;
        JSONValue v;
        JSONError err;
    } tt[] = {
        {"", {.type = JSON_BOOL}, JSON_EOF},
        {"fa", {.type = JSON_BOOL}, JSON_EOF},
        {"tru", {.type = JSON_BOOL}, JSON_EOF},
        {"trux", {.type = JSON_BOOL}, JSON_UNEXPECTED},
        {"false", {.type = JSON_BOOL, .v.b = false}, JSON_OK},
        {"true", {.type = JSON_BOOL, .v.b = true}, JSON_OK},
        {"truex", {.type = JSON_BOOL, .v.b = true}, JSON_OK},
    };
    for (int i = 0; i < ALEN(tt); i++) {
        JSONScanner s = {tt[i].s};
        JSONValue   v;
        JSONError   err = json_scan_bool(&s, &v);
        if (err != tt[i].err) {
            test_fail(t, "%d error: got %s; want %s", i, json_error(err), json_error(tt[i].err));
            continue;
        }
        if (v.type != tt[i].v.type) {
            test_fail(t, "%d type: got %d; want %d", i, v.type, tt[i].v.type);
            continue;
        }
        if (tt[i].err != JSON_OK) {
            continue;
        }
        if (v.v.b != tt[i].v.v.b) {
            test_fail(t, "%d value: got %d; want %d", i, v.v.b, tt[i].v.v.b);
            continue;
        }
    }
}

void test_json_scan_string(Test *t) {
    struct {
        char *    s;
        JSONValue v;
        JSONError err;
    } tt[] = {
        {"", {.type = JSON_STRING}, JSON_EOF},
        {"\"", {.type = JSON_STRING}, JSON_EOF},
        {"abc", {.type = JSON_STRING}, JSON_UNEXPECTED},
        {"\"\\\"", {.type = JSON_STRING}, JSON_EOF},
        {"\"\\ \"", {.type = JSON_STRING}, JSON_UNEXPECTED},
        {"\"\"", {.type = JSON_STRING}, JSON_OK},
        {"\"abc\"", {.type = JSON_STRING}, JSON_OK},
        {"\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"", {.type = JSON_STRING}, JSON_OK},
        {"\"\\u\"", {.type = JSON_STRING}, JSON_UNEXPECTED},
        {"\"\\uzzzz\"", {.type = JSON_STRING}, JSON_UNEXPECTED},
        {"\"\\u1a2f\"", {.type = JSON_STRING}, JSON_OK},
    };
    for (int i = 0; i < ALEN(tt); i++) {
        JSONScanner s = {tt[i].s};
        JSONValue   v;
        JSONError   err = json_scan_string(&s, &v);
        if (err != tt[i].err) {
            test_fail(t, "%d error: got %s; want %s", i, json_error(err), json_error(tt[i].err));
            continue;
        }
        if (v.type != tt[i].v.type) {
            test_fail(t, "%d type: got %d; want %d", i, v.type, tt[i].v.type);
            continue;
        }
    }
}

void test_json_scan_number(Test *t) {
    struct {
        char *    s;
        JSONValue v;
        JSONError err;
    } tt[] = {
        {"", {.type = JSON_NUMBER}, JSON_EOF},
        {"x", {.type = JSON_NUMBER}, JSON_UNEXPECTED},
        {"-1", {.type = JSON_NUMBER, .v.n = -1}, JSON_OK},
        {"-1x", {.type = JSON_NUMBER, .v.n = -1}, JSON_OK},
        {"-01", {.type = JSON_NUMBER}, JSON_UNEXPECTED},
        {"-0.1", {.type = JSON_NUMBER, .v.n = -0.1}, JSON_OK},
        {"123456789", {.type = JSON_NUMBER, .v.n = 123456789}, JSON_OK},
        {"123456789.0123456789", {.type = JSON_NUMBER, .v.n = 123456789.0123456789}, JSON_OK},
        {"-1.2e3", {.type = JSON_NUMBER, .v.n = -1.2e3}, JSON_OK},
        {"1.2e-3", {.type = JSON_NUMBER, .v.n = 1.2e-3}, JSON_OK},
        {"4.5E06", {.type = JSON_NUMBER, .v.n = 4.5e06}, JSON_OK},
    };
    for (int i = 0; i < ALEN(tt); i++) {
        JSONScanner s = {tt[i].s};
        JSONValue   v;
        JSONError   err = json_scan_number(&s, &v);
        if (err != tt[i].err) {
            test_fail(t, "%d error: got %s; want %s", i, json_error(err), json_error(tt[i].err));
            continue;
        }
        if (v.type != tt[i].v.type) {
            test_fail(t, "%d type: got %d; want %d", i, v.type, tt[i].v.type);
            continue;
        }
        if (tt[i].err != JSON_OK) {
            continue;
        }
        if (v.v.n != tt[i].v.v.n) {
            test_fail(t, "%d value: got %d; want %d", i, v.v.n, tt[i].v.v.n);
            continue;
        }
    }
}

int main(void) {
    Test t = {0};

    TEST(&t, test_json_scan_null);
    TEST(&t, test_json_scan_bool);
    TEST(&t, test_json_scan_string);
    TEST(&t, test_json_scan_number);

    if (!test_summary(&t)) {
        return 1;
    }
    return 0;
}
