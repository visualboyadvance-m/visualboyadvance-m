/*
 * Sets Finder flags in a MacBinary file, for the Retro68 build.
 *
 *   macbinary-set-flags <in.bin> <out.bin> <flags>
 *
 * ORs <flags> (hexadecimal, e.g. 2000 for hasBundle) into the file's Finder
 * flags and writes the result, with the MacBinary II header CRC recomputed.
 * Rez writes the application without hasBundle, and without it the Finder
 * ignores the application's BNDL and shows a generic icon in place of its own;
 * hfsutils' hcopy -m then carries the flags over onto the disk image.
 *
 * Built for the build host (cmake/HostCompile.cmake), so plain C.
 */

#include <stdio.h>
#include <stdlib.h>

/* CRC-16/XMODEM (CCITT polynomial, initial value 0): MacBinary II's. */
static unsigned crc16(const unsigned char* data, size_t len)
{
    unsigned crc = 0;
    size_t i;
    int bit;

    for (i = 0; i < len; i++) {
        crc ^= (unsigned)data[i] << 8;
        for (bit = 0; bit < 8; bit++)
            crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
        crc &= 0xFFFF;
    }
    return crc;
}

int main(int argc, char** argv)
{
    FILE* in;
    FILE* out;
    unsigned char* data;
    long size;
    unsigned long flags;
    unsigned crc;

    if (argc != 4) {
        fprintf(stderr, "usage: %s <in.bin> <out.bin> <hex flags>\n", argv[0]);
        return 2;
    }
    flags = strtoul(argv[3], NULL, 16);

    in = fopen(argv[1], "rb");
    if (!in) {
        perror(argv[1]);
        return 1;
    }
    fseek(in, 0, SEEK_END);
    size = ftell(in);
    fseek(in, 0, SEEK_SET);
    if (size < 128) {
        fprintf(stderr, "%s: too short for MacBinary\n", argv[1]);
        return 1;
    }
    data = (unsigned char*)malloc((size_t)size);
    if (!data || fread(data, 1, (size_t)size, in) != (size_t)size) {
        fprintf(stderr, "%s: cannot read\n", argv[1]);
        return 1;
    }
    fclose(in);

    if (data[0] != 0 || data[74] != 0 || data[1] == 0 || data[1] > 63) {
        fprintf(stderr, "%s: not MacBinary\n", argv[1]);
        return 1;
    }

    /* The Finder flags: the high byte at 73, the low one at 101 (MacBinary II). */
    data[73] |= (unsigned char)(flags >> 8);
    data[101] |= (unsigned char)(flags & 0xFF);

    crc = crc16(data, 124);
    data[124] = (unsigned char)(crc >> 8);
    data[125] = (unsigned char)(crc & 0xFF);

    out = fopen(argv[2], "wb");
    if (!out || fwrite(data, 1, (size_t)size, out) != (size_t)size || fclose(out) != 0) {
        perror(argv[2]);
        return 1;
    }
    free(data);
    return 0;
}
