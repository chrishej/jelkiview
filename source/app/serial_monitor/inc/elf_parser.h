#ifndef ELF_PARSER_H_
#define ELF_PARSER_H_

#include <string>
#include "serial_back.h"


namespace elf_parser {
    bool ElfFileChanged(std::string elf_file_path);
    bool ParseElfFile();
    void FileSymbolMapFromJson(FileSymbolMap& grouped_variables);
    std::vector<std::string> GetVariableTypes();
} // namespace elf_parser

#endif // ELF_PARSER_H_