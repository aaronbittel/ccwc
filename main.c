#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>

typedef struct {
    const char* data;
    size_t count;
} String_View;

#define SV_Fmt "%.*s"
#define SV_Arg(sv) (int)(sv).count, (sv).data

#define UNICODE_ASCII_UPPER 0x7F              // 1 1111111
#define UNICODE_2BYTE_UPPER 0xDF              // 110 11111
#define UNICODE_3BTYE_UPPER 0xEF              // 1110 1111
#define UNICODE_4BTYE_UPPER 0xF7              // 11110 111

#define UNICODE_CONTINUATION_UPPER 0xBF       // 10 111111
#define UNICODE_CONTINUATION_LOWER 0x80       // 10 000000

#define C_FLAG 0x01 << 0
#define L_FLAG 0x01 << 1
#define W_FLAG 0x01 << 2
#define M_FLAG 0x01 << 3

#define FILENAMES_CAPACITY 256

static size_t filenames_count = 0;
char* filenames[FILENAMES_CAPACITY] = {0};

String_View sv_from_cstr(const char* s);
String_View sv_chop_by_delim(String_View* sv, char delim);
String_View sv_chop_by_func(String_View* sv, int (*func)(int));
void sv_trim_left(String_View* sv);
size_t get_filesize(FILE* f);
size_t get_unicode_length(unsigned char c);
bool is_unicode_continuation(unsigned char c);
bool flag_provided(int flags, int flag);

int main2() {
    int capacity = 256;
    char buf[capacity];
    ssize_t n;
    while ((n = read(STDIN_FILENO, buf, capacity)) > 0) {
        printf("Read(%zu): %s\n", n, buf);
        memset(buf, 0, capacity);
    }
    if (n == -1) perror("read failed");
    return 0;
}

int main(int argc, char** argv) {
    int flags = 0;

    for (size_t i = 1; i < (size_t)argc; i++) {
        if (strcmp(argv[i], "-c") == 0) {
            flags |= C_FLAG;
        } else if (strcmp(argv[i], "-l") == 0) {
            flags |= L_FLAG;
        } else if (strcmp(argv[i], "-w") == 0) {
            flags |= W_FLAG;
        } else if (strcmp(argv[i], "-m") == 0) {
            flags |= M_FLAG;
        } else {
            filenames[filenames_count++] = argv[i];
        }
    }

    if (flags == 0) { // no flags provided
        flags = C_FLAG | L_FLAG | W_FLAG;
    }

    // Read from stdin
    if (filenames_count == 0) {
        int capacity = 2;
        char buf[capacity];
        ssize_t n;
        size_t filesize = 0;
        size_t line_count = 0;
        size_t word_count = 0;
        size_t char_count = 0;
        size_t exp_uc_cont_bytes = 0;
        bool in_word = false;
        while ((n = read(STDIN_FILENO, buf, capacity)) > 0) {
            filesize += n;
            for (size_t i = 0; (ssize_t)i < n; i++) {
                if (buf[i] == '\n') line_count += 1;
                if (isspace(buf[i])) {
                    if (in_word) {
                        word_count += 1;
                        in_word = false;
                    }
                } else {
                    if (!in_word) in_word = true;
                }
                if (exp_uc_cont_bytes > 0) {
                    assert(is_unicode_continuation((unsigned char)buf[i]));
                    exp_uc_cont_bytes -= 1;
                    if (exp_uc_cont_bytes == 0) char_count += 1;
                } else {
                    if (isascii(buf[i])) {
                        char_count += 1;
                    } else {
                        exp_uc_cont_bytes = get_unicode_length((unsigned char)buf[i]) - 1;
                    }
                }
            }
        }
        if (n == -1) perror("read failed");
        if (flag_provided(flags, L_FLAG)) printf("%7zu", line_count);
        if (flag_provided(flags, W_FLAG)) printf("%7zu", word_count);
        if (flag_provided(flags, C_FLAG)) printf("%7zu", filesize);
        if (flag_provided(flags, M_FLAG)) printf("%7zu", char_count);
        printf("\n");
        exit(EXIT_SUCCESS);
    }


    size_t total_filesize = 0;
    size_t total_line_count = 0;
    size_t total_word_count = 0;
    size_t total_char_count = 0;

    for (size_t i = 0; i < filenames_count; i++) {
        const char* filename = filenames[i];
        FILE* f = fopen(filename, "r");
        if (f == NULL) {
            perror("fopen failed");
        }

        size_t filesize = get_filesize(f);
        size_t line_count, word_count, char_count;

        char* buf = (char*)malloc((size_t) filesize);
        if (buf == NULL) {
            perror("malloc failed");
        }
        if (fread(buf, 1, filesize, f) != filesize) {
            fprintf(stderr, "error occured when reading the file %s\n", filename);
            exit(EXIT_FAILURE);
        }
        String_View orig_sv = sv_from_cstr(buf);

        if (flag_provided(flags, L_FLAG)) {
            String_View sv = orig_sv;
            line_count = 0;
            while (sv.count > 0) {
                sv_chop_by_delim(&sv, '\n');
                line_count += 1;
            }
        }
        if (flag_provided(flags, W_FLAG)) {
            String_View sv = orig_sv;
            word_count = 0;
            while (sv.count > 0) {
                sv_trim_left(&sv);
                if (sv.count == 0) break;
                sv_chop_by_func(&sv, isspace);
                word_count += 1;
            }
        }
        if (flag_provided(flags, M_FLAG)) {
            String_View sv = orig_sv;
            char_count = 0;
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
        }

        free(buf);

        if (fclose(f) == EOF) {
            perror("fclose failed");
        }

        if (flag_provided(flags, L_FLAG)) printf("%7zu", line_count);
        if (flag_provided(flags, W_FLAG)) printf("%7zu", word_count);
        if (flag_provided(flags, C_FLAG)) printf("%7zu", filesize);
        if (flag_provided(flags, M_FLAG)) printf("%7zu", char_count);
        printf(" %s\n", filename);

        total_filesize += filesize;
        total_line_count += line_count;
        total_word_count += word_count;
        total_char_count += char_count;
    }

    if (filenames_count > 1) {
        if (flag_provided(flags, L_FLAG)) printf("%7zu", total_line_count);
        if (flag_provided(flags, W_FLAG)) printf("%7zu", total_word_count);
        if (flag_provided(flags, C_FLAG)) printf("%7zu", total_filesize);
        if (flag_provided(flags, M_FLAG)) printf("%7zu", total_char_count);
        printf(" total\n");
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
    if (c <= UNICODE_ASCII_UPPER) return 1;
    if (c <= UNICODE_2BYTE_UPPER) return 2;
    if (c <= UNICODE_3BTYE_UPPER) return 3;
    if (c <= UNICODE_4BTYE_UPPER) return 4;
    assert(false && "invalid utf-8 character");
}

bool is_unicode_continuation(unsigned char c) {
    return (c >= UNICODE_CONTINUATION_LOWER) && (c <= UNICODE_CONTINUATION_UPPER);
}

bool flag_provided(int flags, int flag) {
    return flags & flag;
}
