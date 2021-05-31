#pragma once

#include <ctime>
#include <string>

namespace eYs3D
{
    
class logger
{
public:
    static logger &instance()
    {
        static logger instance;
        return instance;
    };

private:

    enum class level
    {
        info,
        debug,
        trace,
        error,
        verbose
    };


private:

    logger() = default;
    ~logger() = default;

public:

    template<typename ...Args>
    void info(const char *format, Args... args)
    {
        log(level::info, format, args...);
    }

    template <typename... Args>
    void debug(const char *file, const char *function, int line,
               const char *format, Args... args)
    {
#if !defined(NDEBUG)
        std::string deubg_format = "<" + std::string(file) + " " + std::string(function) + " " + std::to_string(line) + "> " + std::string(format);
        log(level::debug, deubg_format.c_str(), args...);
#endif
    }

    template <typename... Args>
    void error(const char *format, Args... args)
    {
        log(level::error, format, args...);
    }

    template <typename... Args>
    void verbose(const char *format, Args... args)
    {
        log(level::verbose, format, args...);
    }

private:
    template <typename... Args>
    void log(level level, const char *format, Args... args)
    {
        std::string log_format = now() + " " + tag(level) + " " + format;
        fprintf(stderr, log_format.c_str(), args...);
    }

    std::string now()
    {
        std::time_t raw_time = std::time(nullptr);
        std::tm *time = std::localtime(&raw_time);

        if (!time)
            return nullptr;

        time->tm_year += 1900;
        time->tm_mon += 1;

        return "[" + 
               std::to_string(time->tm_year) + "/" +
               std::to_string(time->tm_mon) + "/" +
               std::to_string(time->tm_mday) + " " +
               std::to_string(time->tm_hour) + ":" +
               std::to_string(time->tm_min) + ":" +
               std::to_string(time->tm_sec) +
               "]";
    }

    std::string tag(level tag_level)
    {
        switch (tag_level)
        {
            case level::info:    return std::string("[Info]");
            case level::debug:   return std::string("[Debug]");
            case level::error:   return std::string("[Error]");
            case level::verbose: return std::string("[Verbose]");
            default:             return std::string("");
        }
    }
};

}

#define EYS3D_DEBUG(...) \
    eYs3D::logger::instance().debug(__FILE__, __func__, __LINE__, __VA_ARGS__);

#define EYS3D_INFO(...) \
    eYs3D::logger::instance().info(__VA_ARGS__);

#define EYS3D_ERROR(...) \
    eYs3D::logger::instance().error(__VA_ARGS__);

#define EYS3D_VERBOSE(...) \
    eYs3D::logger::instance().verbose(__VA_ARGS__);
