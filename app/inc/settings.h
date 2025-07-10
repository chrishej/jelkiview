#ifndef SETTINGS_H_
#define SETTINGS_H_
#include <string>
#include <cstdint>

struct Settings
{
    std::string file_path;
    uint8_t     header_line_idx;
    char        time_name[32];
    char        separator[2];
    char        file_filter[32];
    bool        auto_size_y;
};

namespace settings {
Settings const * GetSettings();
void ShowSettingsButton();
}

#endif // SETTINGS_H_