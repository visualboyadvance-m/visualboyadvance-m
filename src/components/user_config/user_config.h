#ifndef VBAM_COMPONENTS_USER_CONFIG_USER_CONFIG_H_
#define VBAM_COMPONENTS_USER_CONFIG_USER_CONFIG_H_

#include <string>

#if defined(_WIN32)
static constexpr char kFileSep = '\\';
#else  // !defined(_WIN32)
static constexpr char kFileSep = '/';
#endif  // defined(_WIN32)

std::string get_xdg_user_config_home();
std::string get_xdg_user_data_home();

#endif  // VBAM_COMPONENTS_USER_CONFIG_USER_CONFIG_H_