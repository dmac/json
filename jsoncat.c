#include <stdio.h>
#include <stdlib.h>

#define JSON_IMPLEMENTATION
#include "json.h"

char *file_string(char *filename) {
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        fprintf(stderr, "error opening file: %s", filename);
        return NULL;
    }
    size_t size = ftell(f);
    rewind(f);

    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        fprintf(stderr, "out of memory");
        return NULL;
    }
    if (fread(buf, 1, size, f) != size) {
        free(buf);
        fclose(f);
        fprintf(stderr, "error reading file: %s", filename);
        return NULL;
    }
    buf[size] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: jsoncat <filename>\n");
        return 1;
    }
    char *      s = file_string(argv[1]);
    char        buf[1000000];
    JSONError   err;
    JSONValue * root;
    JSONScanner scanner = json_scanner_new(s, buf, sizeof(buf));
    if ((err = json_parse(&scanner, &root)) != JSON_OK) {
        fprintf(stderr, "%s: %s", json_error(err), scanner.s);
    }
    json_debug_print(root, 0);
    return 0;
}
