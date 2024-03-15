#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#else // _WIN32
#include <direct.h>
#include <io.h>
#endif // _WIN32

#include <zlib.h>


#define STB_IMAGE_IMPLEMENTATION
extern "C" {
#include "stb_image.h"
}

#define STBI_MSC_SECURE_CRT
#define STB_IMAGE_WRITE_IMPLEMENTATION
extern "C" {
#include "stb_image_write.h"
}

#include "core/fex/fex.h"
#include "core/base/file_util.h"
#include "core/base/message.h"
#include "System.h"
#include "Util.h"
#include "gba/Globals.h"

#ifndef _MSC_VER
#define _stricmp strcasecmp
#endif // ! _MSC_VER

extern int systemColorDepth;
extern int systemRedShift;
extern int systemGreenShift;
extern int systemBlueShift;

extern uint16_t systemColorMap16[0x10000];
extern uint32_t systemColorMap32[0x10000];

#define MAX_CART_SIZE 0x8000000 // 128MB

// Get user-specific config dir manually.
// apple:   ~/Library/Application Support/
// windows: %APPDATA%/
// unix:    ${XDG_CONFIG_HOME:-~/.config}/
std::string get_xdg_user_config_home()
{
    std::string path;
#ifdef __APPLE__
    std::string home(getenv("HOME"));
    path = home + "/Library/Application Support";
#elif _WIN32
    char *app_data_env = getenv("LOCALAPPDATA");
    if (!app_data_env) app_data_env = getenv("APPDATA");
    std::string app_data(app_data_env);
    path = app_data;
#else // Unix
    char *xdg_var = getenv("XDG_CONFIG_HOME");
    if (!xdg_var || !*xdg_var)
    {
	std::string xdg_default(getenv("HOME"));
	path = xdg_default + "/.config";
    }
    else
    {
	path = xdg_var;
    }
#endif
    return path + FILE_SEP;
}

// Get user-specific data dir manually.
// apple:   ~/Library/Application Support/
// windows: %APPDATA%/
// unix:    ${XDG_DATA_HOME:-~/.local/share}/
std::string get_xdg_user_data_home()
{
    std::string path;
#ifdef __APPLE__
    std::string home(getenv("HOME"));
    path = home + "/Library/Application Support";
#elif _WIN32
    char *app_data_env = getenv("LOCALAPPDATA");
    if (!app_data_env) app_data_env = getenv("APPDATA");
    std::string app_data(app_data_env);
    path = app_data;
#else // Unix
    char *xdg_var = getenv("XDG_DATA_HOME");
    if (!xdg_var || !*xdg_var)
    {
	std::string xdg_default(getenv("HOME"));
	path = xdg_default + "/.local/share";
    }
    else
    {
	path = xdg_var;
    }
#endif
    return path + FILE_SEP;
}

void utilReadScreenPixels(uint8_t *dest, int w, int h)
{
        uint8_t *b = dest;
        int sizeX = w;
        int sizeY = h;
        switch (systemColorDepth) {
        case 16: {
                uint16_t *p = (uint16_t *)(g_pix + (w + 2) * 2); // skip first black line
                for (int y = 0; y < sizeY; y++) {
                        for (int x = 0; x < sizeX; x++) {
                                uint16_t v = *p++;

                                *b++ = ((v >> systemRedShift) & 0x001f) << 3;   // R
                                *b++ = ((v >> systemGreenShift) & 0x001f) << 3; // G
                                *b++ = ((v >> systemBlueShift) & 0x01f) << 3;   // B
                        }
                        p++; // skip black pixel for filters
                        p++; // skip black pixel for filters
                }
        } break;
        case 24: {
                uint8_t *pixU8 = (uint8_t *)g_pix;
                for (int y = 0; y < sizeY; y++) {
                        for (int x = 0; x < sizeX; x++) {
                                if (systemRedShift < systemBlueShift) {
                                        *b++ = *pixU8++; // R
                                        *b++ = *pixU8++; // G
                                        *b++ = *pixU8++; // B
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
                uint32_t *pixU32 = (uint32_t *)(g_pix + 4 * (w + 1));
                for (int y = 0; y < sizeY; y++) {
                        for (int x = 0; x < sizeX; x++) {
                                uint32_t v = *pixU32++;
                                *b++ = ((v >> systemBlueShift) & 0x001f) << 3;  // B
                                *b++ = ((v >> systemGreenShift) & 0x001f) << 3; // G
                                *b++ = ((v >> systemRedShift) & 0x001f) << 3;   // R
                        }
                        pixU32++;
                }
        } break;
        }
}

#define CHANNEL_NUM 3 // RGB

bool utilWritePNGFile(const char *fileName, int w, int h, uint8_t *pix)
{
        uint8_t *writeBuffer = new uint8_t[w * h * CHANNEL_NUM];

        uint8_t *b = writeBuffer;

        int sizeX = w;
        int sizeY = h;

        switch (systemColorDepth)
        {
            case 16: {
                    uint16_t *p = (uint16_t *)(pix + (w + 2) * 2); // skip first black line
                    for (int y = 0; y < sizeY; y++) {
                            for (int x = 0; x < sizeX; x++) {
                                    uint16_t v = *p++;

                                    *b++ = ((v >> systemRedShift) & 0x001f) << 3;   // R
                                    *b++ = ((v >> systemGreenShift) & 0x001f) << 3; // G
                                    *b++ = ((v >> systemBlueShift) & 0x01f) << 3;   // B
                            }
                            p++; // skip black pixel for filters
                            p++; // skip black pixel for filters
                    }
            } break;
            case 24: {
                    uint8_t *pixU8 = (uint8_t *)pix;
                    for (int y = 0; y < sizeY; y++) {
                            for (int x = 0; x < sizeX; x++) {
                                    if (systemRedShift < systemBlueShift) {
                                            *b++ = *pixU8++; // R
                                            *b++ = *pixU8++; // G
                                            *b++ = *pixU8++; // B
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
                    uint32_t *pixU32 = (uint32_t *)(pix + 4 * (w + 1));
                    for (int y = 0; y < sizeY; y++) {
                            for (int x = 0; x < sizeX; x++) {
                                    uint32_t v = *pixU32++;

                                    *b++ = ((v >> systemRedShift) & 0x001f) << 3;   // R
                                    *b++ = ((v >> systemGreenShift) & 0x001f) << 3; // G
                                    *b++ = ((v >> systemBlueShift) & 0x001f) << 3;  // B
                            }
                            pixU32++;
                    }
            } break;
        }

        bool ret = (0 != stbi_write_png(fileName, w, h, CHANNEL_NUM, writeBuffer, w * CHANNEL_NUM));
        delete[] writeBuffer;
        return ret;
}

bool utilWriteBMPFile(const char *fileName, int w, int h, uint8_t *pix)
{
        uint8_t writeBuffer[512 * 3];

        FILE *fp = fopen(fileName, "wb");

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

        uint8_t *b = writeBuffer;

        int sizeX = w;
        int sizeY = h;

        switch (systemColorDepth) {
        case 16: {
                uint16_t *p = (uint16_t *)(pix + (w + 2) * (h)*2); // skip first black line
                for (int y = 0; y < sizeY; y++) {
                        for (int x = 0; x < sizeX; x++) {
                                uint16_t v = *p++;

                                *b++ = ((v >> systemBlueShift) & 0x01f) << 3;   // B
                                *b++ = ((v >> systemGreenShift) & 0x001f) << 3; // G
                                *b++ = ((v >> systemRedShift) & 0x001f) << 3;   // R
                        }
                        p++; // skip black pixel for filters
                        p++; // skip black pixel for filters
                        p -= 2 * (w + 2);
                        fwrite(writeBuffer, 1, 3 * w, fp);

                        b = writeBuffer;
                }
        } break;
        case 24: {
                uint8_t *pixU8 = (uint8_t *)pix + 3 * w * (h - 1);
                for (int y = 0; y < sizeY; y++) {
                        for (int x = 0; x < sizeX; x++) {
                                if (systemRedShift > systemBlueShift) {
                                        *b++ = *pixU8++; // B
                                        *b++ = *pixU8++; // G
                                        *b++ = *pixU8++; // R
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
                uint32_t *pixU32 = (uint32_t *)(pix + 4 * (w + 1) * (h));
                for (int y = 0; y < sizeY; y++) {
                        for (int x = 0; x < sizeX; x++) {
                                uint32_t v = *pixU32++;

                                *b++ = ((v >> systemBlueShift) & 0x001f) << 3;  // B
                                *b++ = ((v >> systemGreenShift) & 0x001f) << 3; // G
                                *b++ = ((v >> systemRedShift) & 0x001f) << 3;   // R
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

bool utilIsGzipFile(const char *file)
{
        if (strlen(file) > 3) {
                const char *p = strrchr(file, '.');

                if (p != NULL) {
                        if (_stricmp(p, ".gz") == 0)
                                return true;
                        if (_stricmp(p, ".z") == 0)
                                return true;
                }
        }

        return false;
}

// Opens and scans archive using accept(). Returns fex_t if found.
// If error or not found, displays message and returns NULL.
static fex_t *scan_arc(const char *file, bool (*accept)(const char *), char (&buffer)[2048])
{
        fex_t *fe;
        fex_err_t err = fex_open(&fe, file);
        if (!fe) {
                systemMessage(MSG_CANNOT_OPEN_FILE, N_("Cannot open file %s: %s"), file, err);
                return NULL;
        }

        // Scan filenames
        bool found = false;
        while (!fex_done(fe)) {
                strncpy(buffer, fex_name(fe), sizeof buffer);
                buffer[sizeof buffer - 1] = '\0';

                utilStripDoubleExtension(buffer, buffer);

                if (accept(buffer)) {
                        found = true;
                        break;
                }

                err = fex_next(fe);
                if (err) {
                        systemMessage(MSG_BAD_ZIP_FILE,
                                      N_("Cannot read archive %s: %s"),
                                      file,
                                      err);
                        fex_close(fe);
                        return NULL;
                }
        }

        if (!found) {
                systemMessage(MSG_NO_IMAGE_ON_ZIP, N_("No image found in file %s"), file);
                fex_close(fe);
                return NULL;
        }
        return fe;
}

static bool utilIsImage(const char *file)
{
        return utilIsGBAImage(file) || utilIsGBImage(file);
}

IMAGE_TYPE utilFindType(const char *file, char (&buffer)[2048]);

IMAGE_TYPE utilFindType(const char *file)
{
        char buffer[2048];
        return utilFindType(file, buffer);
}

IMAGE_TYPE utilFindType(const char *file, char (&buffer)[2048])
{
        fex_t *fe = scan_arc(file, utilIsImage, buffer);
        if (!fe)
                return IMAGE_UNKNOWN;
        fex_close(fe);
        file = buffer;
        return utilIsGBAImage(file) ? IMAGE_GBA : IMAGE_GB;
}

static int utilGetSize(int size)
{
        int res = 1;
        while (res < size)
                res <<= 1;
        return res;
}

uint8_t *utilLoad(const char *file, bool (*accept)(const char *), uint8_t *data, int &size)
{
        // find image file
        char buffer[2048];
        fex_t *fe = scan_arc(file, accept, buffer);
        if (!fe)
                return NULL;

        // Allocate space for image
        fex_err_t err = fex_stat(fe);
        int fileSize = fex_size(fe);
        if (size == 0)
                size = fileSize;

        if (size > MAX_CART_SIZE)
                return NULL;

        uint8_t *image = data;

        if (image == NULL) {
                // allocate buffer memory if none was passed to the function
                image = (uint8_t *)malloc(utilGetSize(size));
                if (image == NULL) {
                        fex_close(fe);
                        systemMessage(MSG_OUT_OF_MEMORY,
                                      N_("Failed to allocate memory for %s"),
                                      "data");
                        return NULL;
                }
                size = fileSize;
        }

        // Read image
        int read = fileSize <= size ? fileSize : size; // do not read beyond file
        err = fex_read(fe, image, read);
        fex_close(fe);
        if (err) {
                systemMessage(MSG_ERROR_READING_IMAGE,
                              N_("Error reading image from %s: %s"),
                              buffer,
                              err);
                if (data == NULL)
                        free(image);
                return NULL;
        }

        size = fileSize;

        return image;
}

void replaceAll(std::string &str, const std::string &from, const std::string &to)
{
        if (from.empty())
                return;
        size_t start_pos = 0;
        while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
                str.replace(start_pos, from.length(), to);
                start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with
                                          // 'yx'
        }
}
