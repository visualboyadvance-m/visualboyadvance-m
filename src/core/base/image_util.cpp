#include "image_util.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
extern "C" {
#include <stb_image_write.h>
}  // extern "C"

#include "core/base/file_util.h"
#include "core/base/system.h"
#include "core/base/message.h"

bool utilWritePNGFile(const char* fileName, int w, int h, uint8_t* pix) {
    static constexpr size_t kNumChannels = 3;
    uint8_t* writeBuffer = new uint8_t[w * h * kNumChannels];

    uint8_t* b = writeBuffer;

    int sizeX = w;
    int sizeY = h;

    switch (systemColorDepth) {
        case 8: {
            uint8_t* pixU8 = (uint8_t*)pix + (w);
            for (int y = 0; y < sizeY; y++) {
                for (int x = 0; x < sizeX; x++, pixU8++) {
                    // White color fix
                    if (*pixU8 == 0xff) {
                        *b++ = 0xff;
                        *b++ = 0xff;
                        *b++ = 0xff;
                    } else {
                        *b++ = (((*pixU8 >> 5) & 0x7) << 5);
                        *b++ = (((*pixU8 >> 2) & 0x7) << 5);
                        *b++ = ((*pixU8 & 0x3) << 6);
                    }
                }

                pixU8 += 4;
            }
        } break;
        case 16: {
            uint16_t* p = (uint16_t*)(pix + (w + 2) * 2);  // skip first black line
            for (int y = 0; y < sizeY; y++) {
                for (int x = 0; x < sizeX; x++) {
                    uint16_t v = *p++;

                    *b++ = ((v >> systemRedShift) & 0x001f) << 3;    // R
                    *b++ = ((v >> systemGreenShift) & 0x001f) << 3;  // G
                    *b++ = ((v >> systemBlueShift) & 0x01f) << 3;    // B
                }
                p++;  // skip black pixel for filters
                p++;  // skip black pixel for filters
            }
        } break;
        case 24: {
            uint8_t* pixU8 = (uint8_t*)pix;
            for (int y = 0; y < sizeY; y++) {
                for (int x = 0; x < sizeX; x++) {
                    if (systemRedShift < systemBlueShift) {
                        *b++ = *pixU8++;  // R
                        *b++ = *pixU8++;  // G
                        *b++ = *pixU8++;  // B
                    } else {
                        uint8_t blue = *pixU8++;
                        uint8_t green = *pixU8++;
                        uint8_t red = *pixU8++;

                        *b++ = red;
                        *b++ = green;
                        *b++ = blue;
                    }
                }
            }
        } break;
        case 32: {
            uint32_t* pixU32 = (uint32_t*)(pix + 4 * (w + 1));
            for (int y = 0; y < sizeY; y++) {
                for (int x = 0; x < sizeX; x++) {
                    uint32_t v = *pixU32++;

                    *b++ = ((v >> systemRedShift) & 0x001f) << 3;    // R
                    *b++ = ((v >> systemGreenShift) & 0x001f) << 3;  // G
                    *b++ = ((v >> systemBlueShift) & 0x001f) << 3;   // B
                }
                pixU32++;
            }
        } break;
    }

    bool ret = (0 != stbi_write_png(fileName, w, h, kNumChannels, writeBuffer, w * kNumChannels));
    delete[] writeBuffer;
    return ret;
}

bool utilWriteBMPFile(const char* fileName, int w, int h, uint8_t* pix) {
    uint8_t writeBuffer[512 * 3];

#if __STDC_WANT_SECURE_LIB__
    FILE* fp = NULL;
    fopen_s(&fp, fileName, "wb");
#else
    FILE* fp = fopen(fileName, "wb");
#endif

    if (!fp) {
        systemMessage(MSG_ERROR_CREATING_FILE, N_("Error creating file %s"), fileName);
        return false;
    }

    struct {
        uint8_t ident[2];
        uint8_t filesize[4];
        uint8_t reserved[4];
        uint8_t dataoffset[4];
        uint8_t headersize[4];
        uint8_t width[4];
        uint8_t height[4];
        uint8_t planes[2];
        uint8_t bitsperpixel[2];
        uint8_t compression[4];
        uint8_t datasize[4];
        uint8_t hres[4];
        uint8_t vres[4];
        uint8_t colors[4];
        uint8_t importantcolors[4];
        //    uint8_t pad[2];
    } bmpheader;
    memset(&bmpheader, 0, sizeof(bmpheader));

    bmpheader.ident[0] = 'B';
    bmpheader.ident[1] = 'M';

    uint32_t fsz = sizeof(bmpheader) + w * h * 3;
    utilPutDword(bmpheader.filesize, fsz);
    utilPutDword(bmpheader.dataoffset, 0x36);
    utilPutDword(bmpheader.headersize, 0x28);
    utilPutDword(bmpheader.width, w);
    utilPutDword(bmpheader.height, h);
    utilPutDword(bmpheader.planes, 1);
    utilPutDword(bmpheader.bitsperpixel, 24);
    utilPutDword(bmpheader.datasize, 3 * w * h);

    fwrite(&bmpheader, 1, sizeof(bmpheader), fp);

    uint8_t* b = writeBuffer;

    int sizeX = w;
    int sizeY = h;

    switch (systemColorDepth) {
        case 8: {
            uint8_t* pixU8 = 0;
            pixU8 = (uint8_t*)pix + ((w + 4) * (h));

            for (int y = 0; y < sizeY; y++) {
                for (int x = 0; x < sizeX; x++, pixU8++) {
                    // White color fix
                    if (*pixU8 == 0xff) {
                        *b++ = 0xff;
                        *b++ = 0xff;
                        *b++ = 0xff;
                    } else {
                        *b++ = ((*pixU8 & 0x3) << 6);
                        *b++ = (((*pixU8 >> 2) & 0x7) << 5);
                        *b++ = (((*pixU8 >> 5) & 0x7) << 5);
                    }
                }

                pixU8++;
                pixU8++;
                pixU8++;
                pixU8++;
                pixU8 -= 2 * (w + 4);

                fwrite(writeBuffer, 1, 3 * w, fp);
                b = writeBuffer;
            }
        } break;
        case 16: {
            uint16_t* p = (uint16_t*)(pix + (w + 2) * (h) * 2);  // skip first black line
            for (int y = 0; y < sizeY; y++) {
                for (int x = 0; x < sizeX; x++) {
                    uint16_t v = *p++;

                    *b++ = ((v >> systemBlueShift) & 0x01f) << 3;    // B
                    *b++ = ((v >> systemGreenShift) & 0x001f) << 3;  // G
                    *b++ = ((v >> systemRedShift) & 0x001f) << 3;    // R
                }
                p++;  // skip black pixel for filters
                p++;  // skip black pixel for filters
                p -= 2 * (w + 2);
                fwrite(writeBuffer, 1, 3 * w, fp);

                b = writeBuffer;
            }
        } break;
        case 24: {
            uint8_t* pixU8 = (uint8_t*)pix + 3 * w * (h - 1);
            for (int y = 0; y < sizeY; y++) {
                for (int x = 0; x < sizeX; x++) {
                    if (systemRedShift > systemBlueShift) {
                        *b++ = *pixU8++;  // B
                        *b++ = *pixU8++;  // G
                        *b++ = *pixU8++;  // R
                    } else {
                        uint8_t red = *pixU8++;
                        uint8_t green = *pixU8++;
                        uint8_t blue = *pixU8++;

                        *b++ = blue;
                        *b++ = green;
                        *b++ = red;
                    }
                }
                pixU8 -= 2 * 3 * w;
                fwrite(writeBuffer, 1, 3 * w, fp);

                b = writeBuffer;
            }
        } break;
        case 32: {
            uint32_t* pixU32 = (uint32_t*)(pix + 4 * (w + 1) * (h));
            for (int y = 0; y < sizeY; y++) {
                for (int x = 0; x < sizeX; x++) {
                    uint32_t v = *pixU32++;

                    *b++ = ((v >> systemBlueShift) & 0x001f) << 3;   // B
                    *b++ = ((v >> systemGreenShift) & 0x001f) << 3;  // G
                    *b++ = ((v >> systemRedShift) & 0x001f) << 3;    // R
                }
                pixU32++;
                pixU32 -= 2 * (w + 1);

                fwrite(writeBuffer, 1, 3 * w, fp);

                b = writeBuffer;
            }
        } break;
    }

    fclose(fp);

    return true;
}
