#include <assert.h>
#include <string.h>
#include <ctype.h>
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

#define UNICODE_ASCII 0x7F              // 1xxxxxxx
#define UNICODE_2BTYE 0xDF              // 110xxxxx
#define UNICODE_3BTYE 0xEF              // 1110xxxx
#define UNICODE_4BTYE 0xF7              // 11110xxx
#define UNICODE_CONTINUATION_UPPER 0xBF // 10111111
#define UNICODE_CONTINUATION_LOWER 0x80 // 10000000

String_View sv_from_cstr(const char* s);
String_View sv_chop_by_delim(String_View* sv, char delim);
String_View sv_chop_by_func(String_View* sv, int (*func)(int));
void sv_trim_left(String_View* sv);
size_t get_filesize(FILE* f);
size_t get_unicode_length(unsigned char c);
bool is_unicode_continuation(unsigned char c);

void printBinary(unsigned char byte) {
    for (int i = 7; i >= 0; i--) {
        printf("%d", (byte >> i) & 1);
    }
    printf("\n");
}

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
    } else if (strcmp(argv[1], "-w") == 0) {
        char* buf = (char*)malloc((size_t) filesize);
        if (buf == NULL) {
            perror("malloc failed");
        }
        if (fread(buf, 1, filesize, f) != filesize) {
            fprintf(stderr, "error occured when reading the file %s\n", filename);
            exit(EXIT_FAILURE);
        }
        String_View sv = sv_from_cstr(buf);
        size_t word_count = 0;
        while (sv.count > 0) {
            sv_trim_left(&sv);
            if (sv.count == 0) break;
            sv_chop_by_func(&sv, isspace);
            word_count += 1;
        }
        printf("  %ld %s\n", word_count, filename);
        free(buf);
    } else if (strcmp(argv[1], "-m") == 0) {
        char* buf = (char*)malloc((size_t) filesize);
        if (buf == NULL) {
            perror("malloc failed");
        }
        if (fread(buf, 1, filesize, f) != filesize) {
            fprintf(stderr, "error occured when reading the file %s\n", filename);
            exit(EXIT_FAILURE);
        }
        String_View sv = sv_from_cstr(buf);
        size_t char_count = 0;
        for (size_t i = 0; i < sv.count; i++) {
            size_t char_length = get_unicode_length((unsigned char)sv.data[i]);
            for (size_t j = 0; j < char_length - 1; j++) {
                if (!is_unicode_continuation((unsigned char)sv.data[i+1])) {
                    assert(false && "invalid utf-8 character");
                }
                i += 1;
            }
            char_count += 1;
        }
        printf("  %zu %s\n", char_count, filename);
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

String_View sv_chop_by_func(String_View* sv, int (*func)(int)) {
    if (sv->count == 0) {
        return (String_View){ .data = sv->data };
    }

    String_View result = { .data = sv->data, .count = sv->count };

    while (sv->count > 0 && func(sv->data[0]) == 0) {
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

void sv_trim_left(String_View* sv) {
    if (sv->count == 0) return;

    while (sv->count > 0 && isspace(sv->data[0]) != 0) {
        sv->data += 1;
        sv->count -= 1;
    }
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

size_t get_unicode_length(unsigned char c) {
    if (c <= UNICODE_ASCII) return 1;
    if (c <= UNICODE_2BTYE) return 2;
    if (c <= UNICODE_3BTYE) return 3;
    if (c <= UNICODE_4BTYE) return 4;
    assert(false && "invalid utf-8 character");
}

bool is_unicode_continuation(unsigned char c) {
    return (c >= UNICODE_CONTINUATION_LOWER) && (c <= UNICODE_CONTINUATION_UPPER);
}
