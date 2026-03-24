#include <stdio.h>
#include <stdlib.h>

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

    if (fseek(f, 0, SEEK_END) == -1) {
        perror("fseek failed");
    }

    long filesize = ftell(f);
    if (filesize == -1) {
        perror("ftell failed");
    }

    printf("  %ld %s\n", filesize, filename);

    if (fclose(f) == EOF) {
        perror("fclose failed");
    }
}
