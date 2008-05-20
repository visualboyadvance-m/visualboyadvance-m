#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <zlib.h>
#include "protect.h"

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

  if ((fp = fopen(executable_filename, "rb")))
  {
    size_t file_size;
    uint8_t *buffer;

    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);

    if ((buffer = malloc(file_size))) //Mallocing the whole file? Oh Noes!
    {
      const uint8_t *p;

      rewind(fp);
      fread(buffer, 1, file_size, fp);

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
