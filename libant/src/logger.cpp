#include <sys/time.h>
#include <memory>
#include <algorithm>
#include <stdio.h>
#include <string.h>
#include "logger.h"

namespace ant {

    // NB: length of strings must be equals!
    static const char *LOG_LEVEL[] = {
            "[ERR] ",
            "[WRN] ",
            "[INF] ",
            "[DBG] ",
            "[EXT] "
    };

    const char *log_level_to_string(Log::Log_level level)
    {
        return LOG_LEVEL[level];
    }

    void Log::set(log_function func)
    {
        Logger::instance()->set(func);
    }

    void Log::enable_log_level(Log_level level)
    {
        Logger::instance()->enable_log_level(level);
    }

    void Log::enable_log_name(Log_name name, Log_level level)
    {
        Logger::instance()->enable_log_name(name, level);
    }

    Logger::Logger()
        : log_level(Log::EWarning)
    {
        loggers.resize(Log::EEnd);

        loggers[Log::EAnt] =    { log_level, "[ANT] ",    true  };
        loggers[Log::EDht] =    { log_level, "[DHT] ",    true  };
        loggers[Log::EUtp] =    { log_level, "[UTP] ",    true  };
        loggers[Log::ENet] =    { log_level, "[NET] ",    true  };
		loggers[Log::ERelay] =    { log_level, "[RELAY] ",    true  };
		loggers[Log::ESrt] =    { log_level, "[SRT] ",    true  };

        std::for_each(loggers.begin(), loggers.end(), [](std::vector<Log_entity>::value_type &le){
            le.size = le.prefix.length() + strlen(log_level_to_string(le.level)); });
    }

    void Logger::set(Log::log_function func)
    {
        log_func = func;
    }

    void Logger::enable_log_level(Log::Log_level level)
    {
        log_level = level;
        std::for_each(loggers.begin(), loggers.end(), [level](std::vector<Log_entity>::value_type &le){
            le.level = level; });
    }

    void Logger::enable_log_name(Log::Log_name name, Log::Log_level level)
    {
        Log_entity &le = loggers[name];
        le.level = level;
    }

    Log::Log_level Logger::get_log_level(Log::Log_name name) const
    {
        Log_entity const &le = loggers[name];
        return le.level;
    }

} // namespace
