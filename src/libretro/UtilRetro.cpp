#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Util.h"
#include "gba/gbafilter.h"

IMAGE_TYPE utilFindType(const char* file)
{
    if (utilIsGBAImage(file))
        return IMAGE_GBA;

    if (utilIsGBImage(file))
        return IMAGE_GB;

    return IMAGE_UNKNOWN;
}

static int utilGetSize(int size)
{
    int res = 1;
    while(res < size)
        res <<= 1;
    return res;
}

uint8_t *utilLoad(const char *file, bool (*accept)(const char *), uint8_t *data, int &size)
{
    FILE *fp = NULL;

    fp = fopen(file,"rb");
    if (!fp)
    {
        log("Failed to open file %s", file);
        return NULL;
    }
    fseek(fp, 0, SEEK_END); //go to end

    size = ftell(fp); // get position at end (length)
    rewind(fp);

    uint8_t *image = data;
    if(image == NULL)
    {
        image = (uint8_t *)malloc(utilGetSize(size));
        if(image == NULL)
        {
            log("Failed to allocate memory for %s", file);
            return NULL;
        }
    }

    if (fread(image, 1, size, fp) != (size_t)size) {
        log("Failed to read from %s", file);
        fclose(fp);
        return NULL;
    }

    fclose(fp);
    return image;
}
