#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <fstream>
#include <string>

#define FMT_HEADER_ONLY

#include "fmt/format.h"

typedef enum Severity
{
    LOG_INFO = 0,
    LOG_WARNING,
    LOG_ERROR
} Severity;

const std::string severity_str[3] = {
    "INFO",
    "WARNING",
    "ERROR"
};

class Logger
{
public:
    Logger(const std::string &log_file)
    {
        fout.open(log_file, std::ofstream::out | std::ofstream::trunc);
    }

    ~Logger()
    {
        fout.close();
    }

    template <typename... Args>
    void DoLog(Severity severity, const std::string &context, fmt::format_string<Args...> fmt, Args &&...args)
    {
        fout << fmt::format("[{:<7}] [{}] {}", severity_str[severity], context, fmt::format(fmt, std::forward<Args>(args)...)) << std::endl;
    }
private:
    std::ofstream fout;
};

#endif