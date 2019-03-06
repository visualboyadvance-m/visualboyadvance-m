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

#ifndef NO_PNG
extern "C" {
#include <png.h>
}
#endif

#include "NLS.h"
#include "System.h"
#include "Util.h"
#include "common/Port.h"
#include "gba/Flash.h"
#include "gba/GBA.h"
#include "gba/Globals.h"
#include "gba/RTC.h"

#include "fex/fex.h"

extern "C" {
#include "common/memgzio.h"
}

#include "gb/gbGlobals.h"
#include "gba/gbafilter.h"

#ifndef _MSC_VER
#define _stricmp strcasecmp
#endif // ! _MSC_VER

extern int systemColorDepth;
extern int systemRedShift;
extern int systemGreenShift;
extern int systemBlueShift;

extern uint16_t systemColorMap16[0x10000];
extern uint32_t systemColorMap32[0x10000];

static int(ZEXPORT *utilGzWriteFunc)(gzFile, const voidp, unsigned int) = NULL;
static int(ZEXPORT *utilGzReadFunc)(gzFile, voidp, unsigned int) = NULL;
static int(ZEXPORT *utilGzCloseFunc)(gzFile) = NULL;
static z_off_t(ZEXPORT *utilGzSeekFunc)(gzFile, z_off_t, int) = NULL;

bool FileExists(const char *filename)
{
#ifdef _WIN32
        return (_access(filename, 0) != -1);
#else
        struct stat buffer;
        return (stat(filename, &buffer) == 0);
#endif
}

// Get user-specific config dir manually.
// apple:   ~/Library/Application Support/
// windows: %APPDATA%
// unix:    ${XDG_CONFIG_HOME:-~/.config}
std::string get_xdg_user_config_home()
{
    std::string path;
#ifdef __APPLE__
    std::string home(getenv("HOME"));
    path = home + "/Library/Application Support/";
#elif _WIN32
    std::string app_data(getenv("LOCALAPPDATA"));
    path = app_data + '\\';
#else // Unix
    char *xdg_var = getenv("XDG_CONFIG_HOME");
    if (!xdg_var || !*xdg_var)
    {
	std::string xdg_default(getenv("HOME"));
	xdg_default += "/.config";
	path = xdg_default;
    }
    else
    {
	path = xdg_var;
    }
    path += '/';
#endif
    return path;
}

// Get user-specific data dir manually.
// apple:   ~/Library/Application Support/
// windows: %APPDATA%
// unix:    ${XDG_DATA_HOME:-~/.local/share}
std::string get_xdg_user_data_home()
{
    std::string path;
#ifdef __APPLE__
    std::string home(getenv("HOME"));
    path = home + "/Library/Application Support/";
#elif _WIN32
    std::string app_data(getenv("LOCALAPPDATA"));
    path = app_data + '\\';
#else // Unix
    char *xdg_var = getenv("XDG_DATA_HOME");
    if (!xdg_var || !*xdg_var)
    {
	std::string xdg_default(getenv("HOME"));
	xdg_default += "/.local/share";
	path = xdg_default;
    }
    else
    {
	path = xdg_var;
    }
    path += '/';
#endif
    return path;
}

void utilReadScreenPixels(uint8_t *dest, int w, int h)
{
        uint8_t *b = dest;
        int sizeX = w;
        int sizeY = h;
        switch (systemColorDepth) {
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
                                        int blue = *pixU8++;
                                        int green = *pixU8++;
                                        int red = *pixU8++;

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
                                *b++ = ((v >> systemBlueShift) & 0x001f) << 3;  // B
                                *b++ = ((v >> systemGreenShift) & 0x001f) << 3; // G
                                *b++ = ((v >> systemRedShift) & 0x001f) << 3;   // R
                        }
                        pixU32++;
                }
        } break;
        }
}

bool utilWritePNGFile(const char *fileName, int w, int h, uint8_t *pix)
{
#ifndef NO_PNG
        uint8_t writeBuffer[512 * 3];

        FILE *fp = fopen(fileName, "wb");

        if (!fp) {
                systemMessage(MSG_ERROR_CREATING_FILE, N_("Error creating file %s"), fileName);
                return false;
        }

        png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!png_ptr) {
                fclose(fp);
                return false;
        }

        png_infop info_ptr = png_create_info_struct(png_ptr);

        if (!info_ptr) {
                png_destroy_write_struct(&png_ptr, NULL);
                fclose(fp);
                return false;
        }

        if (setjmp(png_jmpbuf(png_ptr))) {
                png_destroy_write_struct(&png_ptr, NULL);
                fclose(fp);
                return false;
        }

        png_init_io(png_ptr, fp);

        png_set_IHDR(png_ptr,
                     info_ptr,
                     w,
                     h,
                     8,
                     PNG_COLOR_TYPE_RGB,
                     PNG_INTERLACE_NONE,
                     PNG_COMPRESSION_TYPE_DEFAULT,
                     PNG_FILTER_TYPE_DEFAULT);

        png_write_info(png_ptr, info_ptr);

        uint8_t *b = writeBuffer;

        int sizeX = w;
        int sizeY = h;

        switch (systemColorDepth) {
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
                        png_write_row(png_ptr, writeBuffer);

                        b = writeBuffer;
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
                                        int blue = *pixU8++;
                                        int green = *pixU8++;
                                        int red = *pixU8++;

                                        *b++ = red;
                                        *b++ = green;
                                        *b++ = blue;
                                }
                        }
                        png_write_row(png_ptr, writeBuffer);

                        b = writeBuffer;
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

                        png_write_row(png_ptr, writeBuffer);

                        b = writeBuffer;
                }
        } break;
        }

        png_write_end(png_ptr, info_ptr);

        png_destroy_write_struct(&png_ptr, &info_ptr);

        fclose(fp);

        return true;
#else
        return false;
#endif
}

void utilPutDword(uint8_t *p, uint32_t value)
{
        *p++ = value & 255;
        *p++ = (value >> 8) & 255;
        *p++ = (value >> 16) & 255;
        *p = (value >> 24) & 255;
}

void utilPutWord(uint8_t *p, uint16_t value)
{
        *p++ = value & 255;
        *p = (value >> 8) & 255;
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
                                        int red = *pixU8++;
                                        int green = *pixU8++;
                                        int blue = *pixU8++;

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

extern bool cpuIsMultiBoot;

bool utilIsGBAImage(const char *file)
{
        cpuIsMultiBoot = false;
        if (strlen(file) > 4) {
                const char *p = strrchr(file, '.');

                if (p != NULL) {
                        if ((_stricmp(p, ".agb") == 0) || (_stricmp(p, ".gba") == 0) ||
                            (_stricmp(p, ".bin") == 0) || (_stricmp(p, ".elf") == 0))
                                return true;
                        if (_stricmp(p, ".mb") == 0) {
                                cpuIsMultiBoot = true;
                                return true;
                        }
                }
        }

        return false;
}

bool utilIsGBImage(const char *file)
{
        if (strlen(file) > 4) {
                const char *p = strrchr(file, '.');

                if (p != NULL) {
                        if ((_stricmp(p, ".dmg") == 0) || (_stricmp(p, ".gb") == 0) ||
                            (_stricmp(p, ".gbc") == 0) || (_stricmp(p, ".cgb") == 0) ||
                            (_stricmp(p, ".sgb") == 0))
                                return true;
                }
        }

        return false;
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

// strip .gz or .z off end
void utilStripDoubleExtension(const char *file, char *buffer)
{
        if (buffer != file) // allows conversion in place
                strcpy(buffer, file);

        if (utilIsGzipFile(file)) {
                char *p = strrchr(buffer, '.');

                if (p)
                        *p = 0;
        }
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

                fex_err_t err = fex_next(fe);
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

#ifdef WIN32
#include <windows.h>
#endif

IMAGE_TYPE utilFindType(const char *file, char (&buffer)[2048]);

IMAGE_TYPE utilFindType(const char *file)
{
        char buffer[2048];
        return utilFindType(file, buffer);
}

IMAGE_TYPE utilFindType(const char *file, char (&buffer)[2048])
{
#ifdef WIN32
        DWORD dwNum = MultiByteToWideChar(CP_ACP, 0, file, -1, NULL, 0);
        wchar_t *pwText;
        pwText = new wchar_t[dwNum];
        if (!pwText) {
                return IMAGE_UNKNOWN;
        }
        MultiByteToWideChar(CP_ACP, 0, file, -1, pwText, dwNum);
        char *file_conv = fex_wide_to_path(pwText);
        //	if ( !utilIsImage( file_conv ) ) // TODO: utilIsArchive() instead?
        //	{
        fex_t *fe = scan_arc(file_conv, utilIsImage, buffer);
        if (!fe)
                return IMAGE_UNKNOWN;
        fex_close(fe);
        file = buffer;
        //	}
        free(file_conv);
#else
        //	if ( !utilIsImage( file ) ) // TODO: utilIsArchive() instead?
        //	{
        fex_t *fe = scan_arc(file, utilIsImage, buffer);
        if (!fe)
                return IMAGE_UNKNOWN;
        fex_close(fe);
        file = buffer;
//	}
#endif
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
#ifdef WIN32
        DWORD dwNum = MultiByteToWideChar(CP_ACP, 0, file, -1, NULL, 0);
        wchar_t *pwText;
        pwText = new wchar_t[dwNum];
        if (!pwText) {
                return NULL;
        }
        MultiByteToWideChar(CP_ACP, 0, file, -1, pwText, dwNum);
        char *file_conv = fex_wide_to_path(pwText);
        delete[] pwText;
        fex_t *fe = scan_arc(file_conv, accept, buffer);
        if (!fe)
                return NULL;
        free(file_conv);
#else
        fex_t *fe = scan_arc(file, accept, buffer);
        if (!fe)
                return NULL;
#endif
        // Allocate space for image
        fex_err_t err = fex_stat(fe);
        int fileSize = fex_size(fe);
        if (size == 0)
                size = fileSize;

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

void utilExtract(const char *filepath, const char *filename)
{
        fex_t *fex;
        std::string archive_name(filepath);
        archive_name.append(filename);

        fex_open(&fex, archive_name.c_str());
        while (!fex_done(fex)) {
                std::string extracted_filename(filepath);
                extracted_filename.append(fex_name(fex));
#ifdef WIN32
                replaceAll(extracted_filename, "/", "\\");
#endif

                std::string new_dir(filepath);
                new_dir.append(fex_name(fex));
#ifdef WIN32
                replaceAll(new_dir, "/", "\\");
#endif
                new_dir = new_dir.substr(0, new_dir.find_last_of("\\"));
                if (!FileExists(new_dir.c_str()))
                        mkdir(new_dir.c_str()
#ifndef WIN32
                                  ,
                              0777
#endif
                              );

                if (FileExists(extracted_filename.c_str())) {
                        std::string new_name(filepath);
                        new_name.append("old-");
                        new_name.append(fex_name(fex));
#ifdef WIN32
                        replaceAll(new_name, "/", "\\");
#endif
                        remove(new_name.c_str());
                        rename(extracted_filename.c_str(), new_name.c_str());
                }

                FILE *extracted_file = fopen(extracted_filename.c_str(), "wb");
                const void *p;
                fex_data(fex, &p);
                fwrite(p, fex_size(fex), 1, extracted_file);
                fclose(extracted_file);
                fex_next(fex);
        }
        fex_close(fex);
}

void utilWriteInt(gzFile gzFile, int i)
{
        utilGzWrite(gzFile, &i, sizeof(int));
}

int utilReadInt(gzFile gzFile)
{
        int i = 0;
        utilGzRead(gzFile, &i, sizeof(int));
        return i;
}

void utilReadData(gzFile gzFile, variable_desc *data)
{
        while (data->address) {
                utilGzRead(gzFile, data->address, data->size);
                data++;
        }
}

void utilReadDataSkip(gzFile gzFile, variable_desc *data)
{
        while (data->address) {
                utilGzSeek(gzFile, data->size, SEEK_CUR);
                data++;
        }
}

void utilWriteData(gzFile gzFile, variable_desc *data)
{
        while (data->address) {
                utilGzWrite(gzFile, data->address, data->size);
                data++;
        }
}

gzFile utilGzOpen(const char *file, const char *mode)
{
        utilGzWriteFunc = (int(ZEXPORT *)(gzFile, void *const, unsigned int))gzwrite;
        utilGzReadFunc = gzread;
        utilGzCloseFunc = gzclose;
        utilGzSeekFunc = gzseek;

        return gzopen(file, mode);
}

gzFile utilMemGzOpen(char *memory, int available, const char *mode)
{
        utilGzWriteFunc = memgzwrite;
        utilGzReadFunc = memgzread;
        utilGzCloseFunc = memgzclose;
        utilGzSeekFunc = memgzseek;

        return memgzopen(memory, available, mode);
}

int utilGzWrite(gzFile file, const voidp buffer, unsigned int len)
{
        return utilGzWriteFunc(file, buffer, len);
}

int utilGzRead(gzFile file, voidp buffer, unsigned int len)
{
        return utilGzReadFunc(file, buffer, len);
}

int utilGzClose(gzFile file)
{
        return utilGzCloseFunc(file);
}

z_off_t utilGzSeek(gzFile file, z_off_t offset, int whence)
{
        return utilGzSeekFunc(file, offset, whence);
}

long utilGzMemTell(gzFile file)
{
        return memtell(file);
}

void utilGBAFindSave(const int size)
{
        uint32_t *p = (uint32_t *)&rom[0];
        uint32_t *end = (uint32_t *)(&rom[0] + size);
        int detectedSaveType = 0;
        int flashSize = 0x10000;
        bool rtcFound = false;

        while (p < end) {
                uint32_t d = READ32LE(p);

                if (d == 0x52504545) {
                        if (memcmp(p, "EEPROM_", 7) == 0) {
                                if (detectedSaveType == 0 || detectedSaveType == 4)
                                        detectedSaveType = 1;
                        }
                } else if (d == 0x4D415253) {
                        if (memcmp(p, "SRAM_", 5) == 0) {
                                if (detectedSaveType == 0 || detectedSaveType == 1 ||
                                    detectedSaveType == 4)
                                        detectedSaveType = 2;
                        }
                } else if (d == 0x53414C46) {
                        if (memcmp(p, "FLASH1M_", 8) == 0) {
                                if (detectedSaveType == 0) {
                                        detectedSaveType = 3;
                                        flashSize = 0x20000;
                                }
                        } else if (memcmp(p, "FLASH512_", 9) == 0) {
                                if (detectedSaveType == 0) {
                                        detectedSaveType = 3;
                                        flashSize = 0x10000;
                                }
                        } else if (memcmp(p, "FLASH", 5) == 0) {
                                if (detectedSaveType == 0) {
                                        detectedSaveType = 4;
                                        flashSize = 0x10000;
                                }
                        }
                } else if (d == 0x52494953) {
                        if (memcmp(p, "SIIRTC_V", 8) == 0)
                                rtcFound = true;
                }
                p++;
        }
        // if no matches found, then set it to NONE
        if (detectedSaveType == 0) {
                detectedSaveType = 5;
        }
        if (detectedSaveType == 4) {
                detectedSaveType = 3;
        }
        rtcEnable(rtcFound);
        rtcEnableRumble(!rtcFound);
        saveType = detectedSaveType;
        flashSetSize(flashSize);
}

void utilUpdateSystemColorMaps(bool lcd)
{
        switch (systemColorDepth) {
        case 16: {
                for (int i = 0; i < 0x10000; i++) {
                        systemColorMap16[i] = ((i & 0x1f) << systemRedShift) |
                                              (((i & 0x3e0) >> 5) << systemGreenShift) |
                                              (((i & 0x7c00) >> 10) << systemBlueShift);
                }
                if (lcd)
                        gbafilter_pal(systemColorMap16, 0x10000);
        } break;
        case 24:
        case 32: {
                for (int i = 0; i < 0x10000; i++) {
                        systemColorMap32[i] = ((i & 0x1f) << systemRedShift) |
                                              (((i & 0x3e0) >> 5) << systemGreenShift) |
                                              (((i & 0x7c00) >> 10) << systemBlueShift);
                }
                if (lcd)
                        gbafilter_pal32(systemColorMap32, 0x10000);
        } break;
        }
}

// Check for existence of file.
bool utilFileExists(const char *filename)
{
        FILE *f = fopen(filename, "r");
        if (f == NULL) {
                return false;
        } else {
                fclose(f);
                return true;
        }
}
