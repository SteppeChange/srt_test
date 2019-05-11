#pragma once

namespace ant {

    class Log
    {
    public:
        enum Log_level {
            EError = 0,
            EWarning,
            EInfo,
            EDebug,
        };

        enum Log_name {
            EAnt = 0,
            EDht,
            EUtp,
            ENet,
			ERelay,
			ESrt,
			EEnd
        };

    public:
        typedef void (*log_function)(char const *text);

        static void set(log_function func);
        static void enable_log_level(Log_level level);
        static void enable_log_name(Log_name name, Log_level level);
    };
    
}
