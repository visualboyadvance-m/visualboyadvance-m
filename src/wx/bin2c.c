#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

int main(int argc, char** argv) {
    char* in_file_name  = argv[1];
    char* out_file_name = argv[2];
    char* var_name      = argv[3];
    FILE* in_file       = fopen(in_file_name,  "rb");
    char* in_file_err   = !in_file ? strerror(errno) : NULL;
    FILE* out_file      = fopen(out_file_name, "w");
    char* out_file_err  = !out_file ? strerror(errno) : NULL;
    unsigned char* buf  = (unsigned char*)malloc(4096);
    size_t bytes_read;
    int file_pos        = 0;

    if (!buf) return 1;

    if (!in_file) {
        fprintf(stderr, "Can't open input file '%s': %s\n", in_file_name, in_file_err);
        return 1;
    }

    if (!out_file) {
        fprintf(stderr, "Can't open '%s' for writing: %s\n", out_file_name, out_file_err);
        return 1;
    }

    fprintf(out_file, "/* generated from %s: do not edit */\n"
                      "const unsigned char %s[] = {\n", in_file_name, var_name);

    while ((bytes_read = fread(buf, 1, 4096, in_file))) {
        int i = 0;
        for (; i < bytes_read; i++) {
            char* comma = feof(in_file) && i == bytes_read - 1 ? "" : ",";

            fprintf(out_file, "0x%02x%s", buf[i], comma);

            if (++file_pos % 16 == 0) fprintf(out_file, "\n");
        }
    }

    fprintf(out_file, "};\n");

    free(buf);
    fclose(in_file);
    fclose(out_file);

    return 0;
}
