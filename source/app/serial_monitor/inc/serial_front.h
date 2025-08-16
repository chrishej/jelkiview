#ifndef SERIAL_FRONT_H_
#define SERIAL_FRONT_H_

#include <mutex>

#define COMMAND_CHAR "$"
#define ERROR_CHAR "!"
#define TX_CHAR "<"
#define RX_CHAR ">"

namespace serial_front {
    extern std::mutex mtx;
    void SerialWindow();
    void AddLog(const char* fmt, ...);
} // namespace serial_front

#endif // SERIAL_FRONT_H_