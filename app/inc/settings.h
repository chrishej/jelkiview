#ifndef SETTINGS_H_
#define SETTINGS_H_
#include <string>

struct Settings
{
    std::string file_path;
    uint8_t     header_line_idx;
    char        time_name[32];
    char        separator[2];
    char        file_filter[32];
    bool        auto_size_y;
};

Settings const * GetSettings(void);
void ShowSettingsButton(void);

#endif // SETTINGS_H_