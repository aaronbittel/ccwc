#include <assert.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

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

#define FLAG_BYTES 0x01 << 0
#define FLAG_LINES 0x01 << 1
#define FLAG_WORDS 0x01 << 2
#define FLAG_CHARS 0x01 << 3

#define FILENAMES_CAPACITY 256

static size_t filenames_count = 0;
char* filenames[FILENAMES_CAPACITY] = {0};

void process_fd_counts(int fd, int flags, size_t* filesize, size_t* line_count, size_t* word_count, size_t* char_count);
size_t get_filesize(FILE* f);
size_t get_unicode_length(unsigned char c);
bool is_unicode_continuation(unsigned char c);
bool is_flag_provided(int flags, int flag);
void process_word_count(char c, bool* in_word, size_t* word_count);
void process_char_count(unsigned char c, size_t* exp_uc_cont_bytes, size_t* char_count);
void print_wc_counts(int flags, size_t filesize, size_t line_count, size_t word_count, size_t char_count, const char* filename);
void print_help();
void print_version();

int main(int argc, char** argv) {
    int flags = 0;

    for (size_t i = 1; i < (size_t)argc; i++) {
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--bytes") == 0) {
            flags |= FLAG_BYTES;
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--lines") == 0) { flags |= FLAG_LINES;
        } else if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--words") == 0) {
            flags |= FLAG_WORDS;
        } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--chars") == 0) {
            flags |= FLAG_CHARS;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        } else {
            filenames[filenames_count++] = argv[i];
        }
    }

    if (flags == 0) { // no flags provided
        flags = FLAG_BYTES | FLAG_LINES | FLAG_WORDS;
    }

    // Read from stdin
    if (filenames_count == 0) filenames[filenames_count++] = "-";

    size_t total_filesize = 0;
    size_t total_line_count = 0;
    size_t total_word_count = 0;
    size_t total_char_count = 0;

    for (size_t i = 0; i < filenames_count; i++) {
        int fd = strcmp(filenames[i], "-") == 0 ? STDIN_FILENO : open(filenames[i], O_RDONLY);
        if (fd < 0) perror("open failed");

        size_t filesize = 0;
        size_t line_count = 0;
        size_t word_count = 0;
        size_t char_count = 0;

        process_fd_counts(fd, flags, &filesize, &line_count, &word_count, &char_count);
        print_wc_counts(flags, filesize, line_count, word_count, char_count, filenames[i]);

        total_filesize += filesize;
        total_line_count += line_count;
        total_word_count += word_count;
        total_char_count += char_count;

        if (fd != STDIN_FILENO) close(fd);
    }

    if (filenames_count > 1) {
        print_wc_counts(flags, total_filesize, total_line_count, total_word_count, total_char_count, "total");
    }

    return 0;
}

#define BUF_CAPACITY 8096

void process_fd_counts(int fd, int flags, size_t* filesize, size_t* line_count, size_t* word_count, size_t* char_count) {
    char buf[BUF_CAPACITY];

    bool do_c = is_flag_provided(flags, FLAG_BYTES);
    bool do_l = is_flag_provided(flags, FLAG_LINES);
    bool do_w = is_flag_provided(flags, FLAG_WORDS);
    bool do_m = is_flag_provided(flags, FLAG_CHARS);

    size_t remaining_uc_bytes = 0;
    bool in_word = false;

    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        if (do_c) *filesize += n;
        for (size_t i = 0; (ssize_t)i < n; i++) {
            if (do_l && buf[i] == '\n') *line_count += 1;
            if (do_w) process_word_count(buf[i], &in_word, word_count);
            if (do_m) process_char_count((unsigned char)buf[i],
                                         &remaining_uc_bytes, char_count);
        }
    }
    if (in_word) *word_count += 1;
    if (n == -1) perror("read failed");
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

bool is_flag_provided(int flags, int flag) {
    return flags & flag;
}

void process_word_count(char c, bool* in_word, size_t* word_count) {
    if (isspace(c)) {
        if (*in_word) {
            *word_count += 1;
            *in_word = false;
        }
    } else {
        *in_word = true;
    }
}

void process_char_count(unsigned char c, size_t* exp_uc_cont_bytes, size_t* char_count) {
    if (*exp_uc_cont_bytes > 0) {
        assert(is_unicode_continuation(c));
        *exp_uc_cont_bytes -= 1;
        if (*exp_uc_cont_bytes == 0) *char_count += 1;
    } else {
        if (isascii(c)) {
            *char_count += 1;
        } else {
            *exp_uc_cont_bytes = get_unicode_length(c) - 1;
        }
    }
}

void print_wc_counts(int flags, size_t filesize, size_t line_count, size_t word_count, size_t char_count, const char* filename) {
    if (is_flag_provided(flags, FLAG_LINES)) printf("%7zu", line_count);
    if (is_flag_provided(flags, FLAG_WORDS)) printf("%7zu", word_count);
    if (is_flag_provided(flags, FLAG_CHARS)) printf("%7zu", char_count);
    if (is_flag_provided(flags, FLAG_BYTES)) printf("%7zu", filesize);
    if (filename != NULL) printf(" %s", filename);
    printf("\n");
}

void print_help() {
    printf("Usage: ccwc [OPTION]... [FILE]...\n");
    printf("Print newline, word, and byte counts for each FILE, or standard input if none are given.\n\n");
    printf("Options:\n");
    printf("  -c, --bytes     print the byte counts\n");
    printf("  -l, --lines     print the newline counts\n");
    printf("  -w, --words     print the word counts\n");
    printf("  -m, --chars     print the character counts\n");
    printf("  -h, --help      display this help and exit\n");
    printf("  -v, --version   output version information and exit\n");
}

void print_version() {
    printf("ccwc version 1.0.0\n");
}
