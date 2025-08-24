#ifndef LAYOUT_H_
#define LAYOUT_H_

#include <vector>
#include <string>
#include <unordered_map>

namespace layout {
extern std::vector<std::unordered_map<std::string, bool>> subplots_map;
extern std::vector<std::vector<std::string>> layout;

void SetMapToLayout();
void UpdateLayout();
void LayoutMenuButton();
void GuiUpdate();
}

#endif // LAYOUT_H_