#ifndef UTIL_H
#define UTIL_H

#include <string>

#ifdef _WIN32
#define FILE_SEP '\\'
#else // MacOS, Unix
#define FILE_SEP '/'
#endif

std::string get_xdg_user_config_home();
std::string get_xdg_user_data_home();

void utilGBAFindSave(const int);
void utilUpdateSystemColorMaps(bool lcd = false);

#endif // UTIL_H
