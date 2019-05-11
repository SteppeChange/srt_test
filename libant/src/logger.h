#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <vector>
#include <stdarg.h>
#include <memory>

#include "libant/log.h"

namespace ant {

    const char *log_level_to_string(Log::Log_level ll);

    class Logger
    {
    public:
        static Logger* instance() {
            static Logger inst;
            return &inst;
        }

		struct Log_entity {
			Log::Log_level level;
			std::string prefix;
			size_t size;
		};
		std::vector<Log_entity> loggers;

		void set(Log::log_function func);
        void enable_log_level(Log::Log_level level);
        void enable_log_name(Log::Log_name name, Log::Log_level level);
        inline bool is_enabled(Logger::Log_entity const &le, Log::Log_level level) {
            return level <= le.level;
        }
        Log::Log_level get_log_level(Log::Log_name name) const;

        // http://fuckingclangwarnings.com
#ifdef __APPLE__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
#endif
        template<typename ... Args>
        void formatted_print(Log::Log_level level, Logger::Log_entity const &le, const std::string& format, Args ... args)
        {
			size_t size = snprintf(nullptr, 0, format.c_str(), args ...) + le.size + 1; // Extra space for '\0'
            std::unique_ptr<char[]> buf(new char[size]);

            char *p = buf.get();
            p += snprintf(p, le.size + 1, "%s%s", log_level_to_string(level), le.prefix.c_str());
            snprintf(p, size, format.c_str(), args ...);
            std::string formatted(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
            do_print(formatted.c_str());
        }
#ifdef __APPLE__
#pragma clang diagnostic pop
#endif

    private:
        Logger();

        void do_print(char const *text)
        {
            if (log_func && text)
                log_func(text);
        }

    private:
        Log::Log_level log_level;
        Log::log_function log_func;
    };

#	define LOG(level, logger_name, format_pattern, ...) { \
			ant::Logger::Log_entity const &le = ant::Logger::instance()->loggers[logger_name]; \
			if (ant::Logger::instance()->is_enabled(le, level)) \
				ant::Logger::instance()->formatted_print(level, le, format_pattern, ##__VA_ARGS__); }

#	define LOGS(level, logger_name, string) { \
			ant::Logger::Log_entity const &le = ant::Logger::instance()->loggers[logger_name]; \
			if (ant::Logger::instance()->is_enabled(le, level)) \
				ant::Logger::instance()->formatted_print(level, le, string); }

}
