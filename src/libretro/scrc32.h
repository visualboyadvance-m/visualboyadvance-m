#ifndef _S_CRC32_H
#define _S_CRC32_H

static const uint32_t crc32tab[16] = {
	0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC, 0x76DC4190,
	0x6B6B51F4, 0x4DB26158, 0x5005713C, 0xEDB88320, 0xF00F9344,
	0xD6D6A3E8, 0xCB61B38C, 0x9B64C2B0, 0x86D3D2D4, 0xA00AE278,
	0xBDBDF21C
	};

#define DO1_CRC32(buf) crc = crc32tab[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 4);\
                       crc = crc32tab[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 4);
#define DO2_CRC32(buf) \
    DO1_CRC32(buf);    \
    DO1_CRC32(buf);
#define DO4_CRC32(buf) \
    DO2_CRC32(buf);    \
    DO2_CRC32(buf);
#define DO8_CRC32(buf) \
    DO4_CRC32(buf);    \
    DO4_CRC32(buf);

unsigned long crc32(unsigned long crc, const unsigned char* buf, unsigned int len)
{
    if (buf == 0)
        return 0L;
    crc = crc ^ 0xffffffffL;
    while (len >= 8) {
        DO8_CRC32(buf);
        len -= 8;
    }
    if (len)
        do {
            DO1_CRC32(buf);
        } while (--len);
    return crc ^ 0xffffffffL;
}

#endif
