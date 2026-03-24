#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    const char* data;
    size_t count;
} String_View;

#define SV_Fmt "%.*s"
#define SV_Arg(sv) (int)(sv).count, (sv).data

String_View sv_from_cstr(const char* s);
String_View sv_chop_by_delim(String_View* sv, char delim);
size_t get_filesize(FILE* f);

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s -c <file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char* filename = argv[2];
    FILE* f = fopen(filename, "r");
    if (f == NULL) {
        perror("fopen failed");
    }

    size_t filesize = get_filesize(f);

    if (strcmp(argv[1], "-c") == 0) {
        printf("  %ld %s\n", filesize, argv[2]);
    } else if (strcmp(argv[1], "-l") == 0) {
        char* buf = (char*)malloc((size_t) filesize);
        if (buf == NULL) {
            perror("malloc failed");
        }
        if (fread(buf, 1, filesize, f) != filesize) {
            fprintf(stderr, "error occured when reading the file %s\n", filename);
            exit(EXIT_FAILURE);
        }
        String_View sv = sv_from_cstr(buf);
        size_t line_count = 0;
        while (sv.count > 0) {
            sv_chop_by_delim(&sv, '\n');
            line_count += 1;
        }
        printf("  %ld %s\n", line_count, filename);
        free(buf);
    }

    if (fclose(f) == EOF) {
        perror("fclose failed");
    }

    return 0;
}

String_View sv_from_cstr(const char* s) {
    return (String_View){
        .data = s,
        .count = strlen(s),
    };
}

/* Returns the part before the delimiter as a String_View.
 *
 * The given String_View is set to be after the delimiter.
 * */
String_View sv_chop_by_delim(String_View* sv, char delim) {
    if (sv->count == 0) {
        return (String_View){ .data = sv->data };
    }

    String_View result = { .data = sv->data, .count = sv->count };

    while (sv->count > 0 && sv->data[0] != delim) {
        sv->data += 1;
        sv->count -= 1;
    }

    result.count -= sv->count;

    // go over delimiter
    if (sv->count > 0) {
        sv->data += 1;
        sv->count -= 1;
    }

    return result;
}

size_t get_filesize(FILE* f) {
    long save_offset = ftell(f);
    if (save_offset == -1) {
        perror("ftell failed");
    }

    if (fseek(f, 0, SEEK_END) == -1) {
        perror("fseek failed");
    }

    long filesize = ftell(f);
    if (filesize == -1) {
        perror("ftell failed");
    }

    if (fseek(f, save_offset, SEEK_SET) == -1) {
        perror("fseek failed");
    }

    return (size_t) filesize;
}
