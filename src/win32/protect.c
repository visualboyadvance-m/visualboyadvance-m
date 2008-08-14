#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <zlib.h>
#include "protect.h"

/* ========================================================================= */
// read below
static unsigned long gf2_matrix_times(mat, vec)
    unsigned long *mat;
    unsigned long vec;
{
    unsigned long sum;

    sum = 0;
    while (vec) {
        if (vec & 1)
            sum ^= *mat;
        vec >>= 1;
        mat++;
    }
    return sum;
}

// read below
static void gf2_matrix_square(square, mat)
    unsigned long *square;
    unsigned long *mat;
{
    int n;

    for (n = 0; n < 32; n++)
        square[n] = gf2_matrix_times(mat, mat[n]);
}

// This function is taken from zlib 1.2.3 (file: crc32.c)
// It is not exported by the DLL even though it is listed in zlib.h (bug?)
uLong ZEXPORT crc32_combine(crc1, crc2, len2)
    uLong crc1;
    uLong crc2;
    z_off_t len2;
{
    int n;
    unsigned long row;
    unsigned long even[32];    /* even-power-of-two zeros operator */
    unsigned long odd[32];     /* odd-power-of-two zeros operator */

    /* degenerate case */
    if (len2 == 0)
        return crc1;

    /* put operator for one zero bit in odd */
    odd[0] = 0xedb88320L;           /* CRC-32 polynomial */
    row = 1;
    for (n = 1; n < 32; n++) {
        odd[n] = row;
        row <<= 1;
    }

    /* put operator for two zero bits in even */
    gf2_matrix_square(even, odd);

    /* put operator for four zero bits in odd */
    gf2_matrix_square(odd, even);

    /* apply len2 zeros to crc1 (first square will put the operator for one
       zero byte, eight zero bits, in even) */
    do {
        /* apply zeros operator for this bit of len2 */
        gf2_matrix_square(even, odd);
        if (len2 & 1)
            crc1 = gf2_matrix_times(even, crc1);
        len2 >>= 1;

        /* if no more bits set, then done */
        if (len2 == 0)
            break;

        /* another iteration of the loop with odd and even swapped */
        gf2_matrix_square(odd, even);
        if (len2 & 1)
            crc1 = gf2_matrix_times(odd, crc1);
        len2 >>= 1;

        /* if no more bits set, then done */
    } while (len2 != 0);

    /* return combined crc */
    crc1 ^= crc2;
    return crc1;
}
/* ========================================================================= */


char *unprotect_buffer(unsigned char *buffer, size_t buffer_len)
{
  unsigned char *p = buffer, *end_p = p+buffer_len-1, previous = 0x11;
  while (p < end_p)
  {
    unsigned char current = *p;
    *p ^= previous;
    previous = current;
    *p -= 25;
    *p ^= 0x87;
    ++p;
  }
  return((char *)buffer);
}

#ifdef _DEBUG

//For building debug builds, no security ever
int ExecutableValid(const char *executable_filename)
{
  return(0);
}

#else

SET_FN_PTR(fopen, 0x01301100);
SET_FN_PTR(fread, 0x01301200);
SET_FN_PTR(malloc, 0x01301300);

typedef FILE * (*p_fopen)(const char *path, const char *mode);
typedef size_t (*p_fread)(void *ptr, size_t size, size_t nmemb, FILE *stream);
typedef void * (*p_malloc)(size_t size);

static uint8_t *memmem(const uint8_t *haystack, size_t haystacklen, const uint8_t *needle, size_t needlelen)
{
  if (needlelen)
  {
    if (needlelen <= haystacklen)
    {
      haystacklen -= needlelen-1;
      while (haystacklen--)
      {
        if (!memcmp(haystack, needle, needlelen))
        {
          return((uint8_t *)haystack);
        }
        ++haystack;
      }
    }
  }
  else
  {
    return((uint8_t *)haystack);
  }
  return(0);
}

static volatile uint32_t data[] = {
  0x00000000,
  0x0C097E1B,
  0x00000000,
  0xABCEFDCA,
  0x6876ABDC,
  0x12345678,
  0x9ABCDEF0,
  0xFEDCBA98,
  0x76543210,
  0xDEADBEEF,
  0xFEEDFACE,
  0xDEADBABE,
  0x00000000,
  0xFFFFFFFF,
  0x3AB5F60E,
  0xCCCCCCCC,
  0xA55AA55A,
  0x43570C13,
  0x74372984
};

int ExecutableValid(const char *executable_filename)
{
  FILE *fp;
  int retval = 1;  //Invalid

  if ((fp = ((p_fopen)GET_FN_PTR(fopen))(executable_filename, "rb")))
  {
    size_t file_size;
    uint8_t *buffer;

    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);

    if ((buffer = ((p_malloc)GET_FN_PTR(malloc))(file_size))) //Mallocing the whole file? Oh Noes!
    {
      const uint8_t *p;

      rewind(fp);
      ((p_fread)GET_FN_PTR(fread))(buffer, 1, file_size, fp);

      if ((p  = memmem(buffer, file_size, (const uint8_t *)data, sizeof(data))))
      {
        size_t length_till_data = p-buffer;
        uint32_t crc1, crc2, crc3, crc4;

        crc1 = crc2 = crc3 = crc4 = crc32(0L, Z_NULL, 0);
        crc1 = crc32(crc1, (const Bytef *)buffer, length_till_data);
        crc2 = crc32(crc1, (const Bytef *)data, sizeof(data));
        crc3 = crc32(crc3, (const Bytef *)p+sizeof(data), file_size-(length_till_data+sizeof(data)));
        crc4 = crc32(crc4, (const Bytef *)(data+1), sizeof(data)-sizeof(uint32_t));

        crc1 = crc32_combine(crc1, crc3, file_size-(length_till_data+sizeof(data)));
        crc2 = crc32_combine(crc2, crc3, file_size-(length_till_data+sizeof(data)));

        crc3 = adler32(0L, Z_NULL, 0);
        crc3 = adler32(crc3, (const Bytef *)buffer, length_till_data);
        crc3 = adler32(crc3, (const Bytef *)p+sizeof(data), file_size-(length_till_data+sizeof(data)));

        if ((data[sizeof(data)/sizeof(uint32_t)-4] == crc1) &&
            (data[sizeof(data)/sizeof(uint32_t)-3] == crc3) &&
            (data[sizeof(data)/sizeof(uint32_t)-2] == crc2) &&
            (data[sizeof(data)/sizeof(uint32_t)-1] == crc4) &&
            (file_size == data[2]) &&
            (data[3] == ((uint32_t *)(buffer+file_size))[-2]) &&
            (data[4] == ((uint32_t *)(buffer+file_size))[-1]))
        {
          retval = data[12]; //Valid
        }

        free(buffer);
      }
    }
    else
    {
      retval = -2; //Error Occured - Memory
    }

    fclose(fp);
  }
  else
  {
    retval = -1; //Error Occured - File
  }

  return(retval);
}

#endif
