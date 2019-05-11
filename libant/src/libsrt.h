//
// Created by Aleksey Dorofeev on 2019-01-16.
//

#ifndef LIBANT_LIBSRT_H
#define LIBANT_LIBSRT_H

#include <algorithm>
#include <memory>
#include <mutex>
#include <map>
#include <vector>
#include <thread>
#include <sys/socket.h>
#include <srt.h>
#include "network.h"
#include "channel_statistics.h"

#define SRT_DEFAULT_PORT 3010
#define SRT_EMPTY_CONN_ID -1

//#define USE_SRT_RECEIVE_LIMITER
//#define RECEIVE_LIMIT_BYTES_PER_SECOND 200000

namespace ant
{
#if defined(USE_SRT_RECEIVE_LIMITER)
    struct Traffic_limiter {
        size_t limit_period_ms;
        size_t limit_bytes;
        size_t ready_bytes;
        std::chrono::steady_clock::time_point last_tick;

        Traffic_limiter(size_t period_ms, size_t limit) : limit_period_ms(period_ms), limit_bytes(limit), ready_bytes(0) {
            last_tick = std::chrono::steady_clock::now();
        }
        bool tick() {
            auto now = std::chrono::steady_clock::now();
            auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_tick);
            if (delta.count() >= limit_period_ms) {
                last_tick = now;
                ready_bytes = 0;
            }
            return ready_bytes < limit_bytes;
        }
        bool ready_data(size_t bytes) {
            ready_bytes += bytes;
            return tick();
        }
        inline size_t get_rest_limit() {
            return ready_bytes < limit_bytes ? limit_bytes - ready_bytes : 0;
        }
    };
#endif

    struct Srt_connection {
        typedef std::shared_ptr<Srt_connection> ptr;

        Srt_connection();

        sockaddr_storage _local_addr;
        sockaddr_storage _addr;
        SRTSOCKET _sock;
        SRT_SOCKSTATUS _status;

        typedef std::deque<std::vector<uint8_t>> outgoing_buffer;
        outgoing_buffer _send_buf;
        unsigned _bufsize;
        int _max_size;
        int _hwm;
        int _lwm;
        int _mss;

        enum Congestion_state {
            ENoCongestion = 0,
            ECongestion
        };
        Congestion_state _congestion;

        channel_statistics::ptr _stats;

        size_t outgoing_buffer_size() const {
            size_t outgoing_buffer_size = 0;
            for (auto const& item: _send_buf) {
                outgoing_buffer_size += item.size();
            }
            return outgoing_buffer_size;
        }

        //fixme: for debug purposes only
        unsigned _read_count;
    };

    using Srt_connection_id = int;
    using Srt_connecting_cb = std::function<void(Srt_connection_id, sockaddr_storage)>;

    class Srt_events
    {
    public:
        virtual void srt_on_connect(Srt_connection_id const &conn_id, sockaddr_storage const &remote_addr) = 0;
        virtual void srt_on_connect_error(Srt_connection_id const &conn_id, sockaddr_storage const &to_addr,
                                          std::string const &errcode) = 0;

        virtual void srt_on_accept(Srt_connection_id const &conn_id, sockaddr_storage const &remote_addr) = 0;
        virtual void srt_on_recv(Srt_connection_id const &conn_id, std::vector<uint8_t> data) = 0;
        virtual void srt_on_lwm(Srt_connection_id const &conn_id) = 0;
        virtual void srt_on_break(Srt_connection_id const &conn_id) = 0;
    };

    class Srt
    {
    protected:
        Srt_events *_events;
        std::thread *_thread;
        bool _break_loop;

        sockaddr_storage _addr; // listening address
        SRTSOCKET _sock;        // listening socket
        int _poll_id;

        int _congestion;        // the number of congested peers

        std::map<SRTSOCKET, Srt_connection::ptr> _peers;
        using Connection_map_value = std::map<SRTSOCKET, Srt_connection::ptr>::value_type;
        std::mutex _peers_mt;

        Network::ptr _ant_network;

#if defined(USE_SRT_RECEIVE_LIMITER)
        std::unique_ptr<Traffic_limiter> _receive_limiter;
        bool _is_receive_limit_reached;
#endif

        //fixme: for debug purposes only
        unsigned _epoll_time_ms{0};
        unsigned _epoll_events{0};

    public:
        typedef std::shared_ptr<Srt> ptr;

        enum ESendStatus {
            ESendOK = 0,
            ESendHWM = 1,
            ESendFailed = 2
        };

        Srt(Srt_events *events, Network::ptr a_net);
        ~Srt();

        void start(sockaddr_storage const& bind_addr);
        void stop();
        // set buffer parameters, size == -1 means no restriction
        void set_buffer(Srt_connection_id const& conn_id, int size, int hwm, int lwm);
        bool connect(sockaddr_storage const& to_addr, Srt_connection_id &conn_id, Srt_connecting_cb const& connecting_cb);
        int send(Srt_connection_id const& conn_id, std::vector<uint8_t>&& data);
        void close(Srt_connection_id const& conn_id);
        sockaddr_storage getbindaddr() const { return _addr; }
        void set_stat_handler(Srt_connection_id const& conn_id, channel_statistics::ptr const& a_stats);

    private:
        void srt_connecting_from_addr(Srt_connecting_cb const& ext_connect_cb,
                                      const SRTSOCKET s, struct sockaddr const *addr, const socklen_t addr_len);
        bool listen(sockaddr_storage const &bind_addr);
        void thread_proc();
        void internal_send(Srt_connection::ptr peer);

        void connection_established();
        void connection_received(SRTSOCKET s);
        void connection_ready_to_send(SRTSOCKET s);
        void connection_broken(SRTSOCKET s);

        inline size_t congested_connection_count() {
            size_t congested_connections = 0;
            std::for_each(_peers.begin(), _peers.end(), [&congested_connections](Connection_map_value const& val) {
                if (val.second->_congestion != Srt_connection::ENoCongestion)
                    ++congested_connections;
            });
            return congested_connections;
        }
    };

}

#endif //LIBANT_LIBSRT_H
