#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <fstream>
#include <stdexcept>
#include <string>

#define FMT_HEADER_ONLY
#include "3rdparty/fmt/format.h"

typedef enum Severity
{
    LOG_INFO = 0,
    LOG_WARNING,
    LOG_WARN_POPUP,
    LOG_ERROR
} Severity;

const std::string severity_str[4] = {
    "INFO",
    "WARNING",
    "WARNING",
    "ERROR"
};

typedef void (*MessageBoxFunc)(Severity, const char *);

class Logger
{
public:
    Logger(const std::string &log_file)
    {
        fout.open(log_file, std::ofstream::out | std::ofstream::trunc);

        if (!fout.is_open())
        {
            throw std::runtime_error("Logger::Logger: Unable to create a file for logging!");
        }

        fout << fmt::format("{} created", log_file) << std::endl;
    }

    ~Logger()
    {
        fout.close();
    }

    void SetDoMessageBox(MessageBoxFunc doMessageBox)
    {
        DoMessageBox = doMessageBox;
    }

    template <typename... Args>
    void DoLog(Severity severity, const std::string &context, fmt::format_string<Args...> fmt, Args &&...args)
    {
        std::string formatted = fmt::format(fmt, std::forward<Args>(args)...);

        if (severity == LOG_WARN_POPUP || severity == LOG_ERROR)
        {
            DoMessageBox(severity, formatted.c_str());
        }

        fout << fmt::format("[{:<7}] [{}] {}", severity_str[severity], context, formatted) << std::endl;
    }
private:
    std::ofstream fout;

    MessageBoxFunc DoMessageBox;
};

#endif