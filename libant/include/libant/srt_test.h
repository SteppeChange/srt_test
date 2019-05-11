#pragma once

#include <list>
#include <string>

namespace ant_tests {
    class ANTSrtTest
    {
    public:
        typedef void (*log_function)(char const *text);

        ANTSrtTest(log_function logFunc);

        log_function _logFunc;

        int o_debug;
        bool o_listen;
        bool o_echo;
        uint16_t o_local_ant_port;
        uint16_t o_local_srt_port;
        bool o_rendezvous_mode;
        std::list<std::string> o_remote_address;
        int o_bufsize;
        int o_hwm;
        int o_lwm;
        int o_send_timeout_ms;
        int o_inter_timeout_ms;
        int o_timeout;
        int o_client_proxy;
        int o_server_proxy;

        void start();
    };

} // namespace ant_tests