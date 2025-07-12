#ifndef SETTINGS_H_
#define SETTINGS_H_
#include <cstdint>
#include <string>

namespace settings {
const size_t kSettingsCharBufLen = 0x1F;
}

struct Settings {
    std::string file_path;
    uint8_t header_line_idx;
    char time_name[settings::kSettingsCharBufLen];
    char separator[settings::kSettingsCharBufLen];
    bool auto_size_y;
};

namespace settings {
Settings const* GetSettings();
void init();
void ShowSettingsButton();
void SetLogFilePath(std::string path);
}  // namespace settings

#endif  // SETTINGS_H_