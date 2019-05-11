//
// Created by Aleksey Dorofeev on 2019-01-16.
//

#include "logger.h"
#include "utils.hpp"
#include "libsrt.h"
#include <functional>

enum {
    SRT_BUF_SIZE = 50000,
    SRT_DEF_MSS  = 1360,    // SRT_LIVE_DEF_PLSIZE(1316) + UDP.hdr(28) + SRT.hdr(16)
};

extern "C" void srt_log_handler(void* opaque, int level, const char* file, int line, const char* area, const char* msg)
{
    ant::Log::Log_level ant_level;
    switch (level) {
        case srt_logging::LogLevel::debug: ant_level = ant::Log::EDebug; break;
        case srt_logging::LogLevel::note: ant_level = ant::Log::EInfo; break;
        case srt_logging::LogLevel::warning: ant_level = ant::Log::EWarning; break;
        case srt_logging::LogLevel::error: ant_level = ant::Log::EError; break;
        default: ant_level = ant::Log::EDebug; break;
    }
    LOG(ant_level, ant::Log::ESrt, "%s\n", msg);
}

ant::Srt_connection::Srt_connection()
    : _status(SRTS_INIT)
    , _bufsize(0)
    , _max_size(-1)
    , _hwm(-1)
    , _lwm(-1)
    , _mss(SRT_DEF_MSS)
    , _congestion(ENoCongestion)
    , _read_count(0)
{
    memset(&_addr, 0, sizeof(_addr));
}

ant::Srt::Srt(Srt_events* events, Network::ptr a_net)
    : _events(events)
    , _thread(nullptr)
    , _break_loop(false)
    , _sock(SRT_EMPTY_CONN_ID)
    , _poll_id(-1)
    , _congestion(0)
    , _ant_network(a_net)
{
    // TODO need to change from enable_log_name
    srt_setloglevel(srt_logging::LogLevel::note);
    srt_setlogflags( 0
                    | SRT_LOGF_DISABLE_TIME
                    | SRT_LOGF_DISABLE_SEVERITY
                    | SRT_LOGF_DISABLE_THREADNAME
                    | SRT_LOGF_DISABLE_EOL
                    );
    char NAME[] = "SRTLIB";
    srt_setloghandler(NAME, srt_log_handler);

    srt_startup();

#if defined(USE_SRT_RECEIVE_LIMITER)
    _receive_limiter.reset(new Traffic_limiter(1000, RECEIVE_LIMIT_BYTES_PER_SECOND));
    _is_receive_limit_reached = false;
#endif
}

ant::Srt::~Srt()
{
    stop();
    srt_cleanup();
}

void ant::Srt::start(sockaddr_storage const& bind_addr)
{
	LOG(ant::Log::EDebug, ant::Log::EAnt, "Srt::start %s\n", ant::print_sockaddr(bind_addr).c_str())
    _poll_id = srt_epoll_create();

    if(!listen(bind_addr))
		LOG(ant::Log::EError, ant::Log::EAnt, "srt listen failed\n");

	_thread = new std::thread(&Srt::thread_proc, this);
}

void ant::Srt::stop()
{
	LOG(ant::Log::EDebug, ant::Log::EAnt, "Srt::stop\n")

	if (_thread) {
        _break_loop = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        _thread->join();
        delete _thread;
        _thread = nullptr;
    }

    if (_sock != -1) {
        srt_close(_sock);
        _sock = -1;
    }

    {
        std::lock_guard<std::mutex> lock(_peers_mt);
        for (auto itr: _peers) {
            srt_close(itr.second->_sock);
        }
        _peers.clear();
    }

    if (_poll_id != -1)
        srt_epoll_release(_poll_id);
}

void ant::Srt::set_stat_handler(Srt_connection_id const& conn_id, channel_statistics::ptr const& a_stats)
{
    std::lock_guard<std::mutex> lock(_peers_mt);

    auto it = _peers.find(conn_id);
    assert(it != _peers.end());
    if (it != _peers.end())
        it->second->_stats = a_stats;
}

void ant::Srt::set_buffer(Srt_connection_id const& conn_id, int size, int hwm, int lwm)
{
    std::lock_guard<std::mutex> lock(_peers_mt);

    auto it = _peers.find(conn_id);
    assert(it != _peers.end());
    if (it != _peers.end()) {
        assert(it->second->_bufsize == 0);
        if (it->second->_bufsize == 0) {
            it->second->_max_size = size;
            it->second->_hwm = hwm;
            it->second->_lwm = lwm;
        }
    }
}

void ant::Srt::srt_connecting_from_addr(Srt_connecting_cb const& ext_connect_cb,
        const SRTSOCKET s, struct sockaddr const *addr, const socklen_t addr_len)
{
    //nb: function called from srt_connect(), no need to lock mutex!
    Srt_connection::ptr peer = _peers[s];
    memcpy(&peer->_local_addr, addr, addr_len);
    LOG(ant::Log::EDebug, ant::Log::EAnt, "srt outgoing connection(%d) from addr %s\n",
            s, ant::print_sockaddr(peer->_local_addr).c_str())

    if (ext_connect_cb)
        ext_connect_cb(s, peer->_local_addr);
}

bool ant::Srt::listen(sockaddr_storage const &bind_addr)
{
    if (_sock != -1)
        return false;

    _addr = bind_addr;

    _sock = srt_socket(_addr.ss_family, SOCK_DGRAM, IPPROTO_UDP);
    if (_sock == -1) {
        LOG(ant::Log::EError, ant::Log::EAnt, "srt_socket() error: %s\n", srt_getlasterror_str())
        return false;
    }

    int opt = SRTT_FILE;
    int opt_len = sizeof opt;
    srt_setsockflag(_sock, SRTO_TRANSTYPE, &opt, opt_len);
    opt = 1;
    srt_setsockflag(_sock, SRTO_REUSEADDR, &opt, opt_len);
    opt = 0;
    srt_setsockflag(_sock, SRTO_PASSPHRASE, &opt, opt_len);
    opt = 0;
    srt_setsockflag(_sock, SRTO_RCVSYN, &opt, opt_len);
    //fixme: fix for using address 127.0.0.1
    opt = SRT_DEF_MSS;
    srt_setsockflag(_sock, SRTO_MSS, &opt, opt_len);
    opt = 0;
    srt_setsockflag(_sock, SRTO_MAXBW, &opt, opt_len);

    socklen_t addr_len = 0;
    short port = 0;
    if (_addr.ss_family == AF_INET) {
        addr_len = sizeof(sockaddr_in);
        port = ntohs(SOCK_ADDR_IN_PORT(&_addr));
    } else if (_addr.ss_family == AF_INET6) {
        addr_len = sizeof(sockaddr_in6);
        port = ntohs(SOCK_ADDR_IN6_PORT(&_addr));
    }
    LOG(ant::Log::EInfo, ant::Log::EAnt, "binding :%d..\n", port)

    int rc = srt_bind(_sock, (struct sockaddr *) &_addr, addr_len);
    if (rc == SRT_ERROR) {
        LOG(ant::Log::EError, ant::Log::EAnt, "srt_bind() error: %s\n", srt_getlasterror_str())
        return false;
    }

    srt_getsockname(_sock, (struct sockaddr *) &_addr, (int *) &addr_len);
    LOG(ant::Log::EInfo, ant::Log::EAnt, "libsrt bound to local %s\n", ant::print_sockaddr(_addr).c_str())

    rc = srt_listen(_sock, 1);
    if (rc == SRT_ERROR) {
        LOG(ant::Log::EError, ant::Log::EAnt, "srt_listen() error: %s\n", srt_getlasterror_str())
        return false;
    }
    return true;
}

bool ant::Srt::connect(sockaddr_storage const& to_addr, Srt_connection_id &conn_id, Srt_connecting_cb const& connecting_cb)
{
    std::lock_guard<std::mutex> lock(_peers_mt);

    LOG(ant::Log::EDebug, ant::Log::EAnt, "srt connecting to %s\n", print_sockaddr(to_addr).c_str())

    auto itr = _peers.find(conn_id);
    if (itr != _peers.end()) {
        LOG(ant::Log::EDebug, ant::Log::EAnt, "connection(%d) is already exists\n", conn_id)
        return true;
    }

    SRTSOCKET sock = srt_socket(to_addr.ss_family, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == -1) {
        LOG(ant::Log::EDebug, ant::Log::EAnt, "srt_socket() error: %s\n", srt_getlasterror_str())
        return false;
    }

    int opt = SRTT_FILE;
    int opt_len = sizeof opt;
    srt_setsockflag(sock, SRTO_TRANSTYPE, &opt, opt_len);
    opt = 0;
    srt_setsockflag(sock, SRTO_PASSPHRASE, &opt, opt_len);
    opt = 0;
    srt_setsockflag(sock, SRTO_SNDSYN, &opt, opt_len);
    opt = 0;
    srt_setsockflag(sock, SRTO_RCVSYN, &opt, opt_len);
    opt = 0;
    srt_setsockflag(sock, SRTO_LINGER, &opt, opt_len);
    //fixme: fix for using address 127.0.0.1
    opt = SRT_DEF_MSS;
    srt_setsockflag(sock, SRTO_MSS, &opt, opt_len);
    opt = 0;
    srt_setsockflag(sock, SRTO_MAXBW, &opt, opt_len);

    int events = SRT_EPOLL_IN | SRT_EPOLL_ERR;
    int rc = srt_epoll_add_usock(_poll_id, sock, &events);
    LOG(ant::Log::EDebug, ant::Log::EAnt, "connect: add polling socket: %d\n", sock)
    if (rc == SRT_ERROR) {
        LOG(ant::Log::EError, ant::Log::EAnt, "srt_epoll_add_usock() error: %s\n", srt_getlasterror_str())
        return false;
    }

    // add new peer
    Srt_connection::ptr peer = std::make_shared<Srt_connection>();
    peer->_sock = sock;
    peer->_status = SRTS_CONNECTING;
    peer->_addr = to_addr;

    _peers[peer->_sock] = peer;

    // export connection id before calling callback
    conn_id = sock;

    auto connect_callback = std::bind(&Srt::srt_connecting_from_addr, this, connecting_cb,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

    socklen_t soc_size = 0;
    if (to_addr.ss_family == AF_INET) {
        soc_size = sizeof(sockaddr_in);
    } else if (to_addr.ss_family == AF_INET6) {
        soc_size = sizeof(sockaddr_in6);
    } else {
        RUNTIME_ERROR("unsupported family %d\n", to_addr.ss_family);
    }

    rc = srt_connect(sock, (struct sockaddr *) &to_addr, soc_size);
    if (rc == SRT_ERROR) {
        LOG(ant::Log::EDebug, ant::Log::EAnt, "srt_connect() error: %s\n", srt_getlasterror_str())
        return false;
    }

    return true;
}

int ant::Srt::send(Srt_connection_id const& conn_id, std::vector<uint8_t>&& data)
{
    std::lock_guard<std::mutex> lock(_peers_mt);

    auto itr = _peers.find(conn_id);
    if (itr == _peers.end())
        return ESendFailed;
    Srt_connection::ptr peer = itr->second;

    if (peer->_stats)
        peer->_stats->push_sending_event(data.size());

    peer->_send_buf.emplace_back(data);
    peer->_bufsize += data.size();

    if (!peer->_congestion) {
        internal_send(peer);
    } else {
        LOG(ant::Log::EWarning, ant::Log::EAnt, "peer (%s): HWM(+%d=%d)\n",
            ant::print_sockaddr(peer->_addr).c_str(), data.size(), peer->_bufsize+data.size())
    }

    if (peer->_bufsize) {
        if (peer->_stats)
            peer->_stats->push_buffer_event(peer->_bufsize);

        if (peer->_hwm != -1 && peer->_lwm != -1 && peer->_bufsize >= peer->_hwm && !peer->_congestion) {
            LOG(ant::Log::EWarning, ant::Log::EAnt, "peer (%s): HWM(%d)\n",
                ant::print_sockaddr(peer->_addr).c_str(), peer->_bufsize)

            peer->_congestion = Srt_connection::ECongestion;
            _congestion = congested_connection_count();

            int events = SRT_EPOLL_IN | SRT_EPOLL_ERR | SRT_EPOLL_OUT;
            srt_epoll_remove_usock(_poll_id, peer->_sock);
            int rc = srt_epoll_add_usock(_poll_id, peer->_sock, &events);
            if (rc == SRT_ERROR) {
                LOG(ant::Log::EError, ant::Log::EAnt, "srt_epoll_add_usock() error: %s\n", srt_getlasterror_str())
                assert(0);
            }
        }
    }

    return peer->_congestion ? ESendHWM : ESendOK;
}

void ant::Srt::internal_send(Srt_connection::ptr peer)
{
    int sent_bytes = 0;
    int error = 0;
    while (peer->_bufsize) {
        std::vector<uint8_t> &sbuf = peer->_send_buf.front();
        size_t len = sbuf.size();

        int rc = srt_sendmsg(peer->_sock, (const char *) sbuf.data(), len, -1, 1);
        if (rc > 0) {
            LOG(ant::Log::EDebug, ant::Log::EAnt, "srt_sendmsg(%d, %d) = %d bytes\n", peer->_sock, len, rc)
            peer->_bufsize -= rc;
            sent_bytes += rc;

            if (peer->_stats)
                peer->_stats->push_sent_event(rc);

            if (rc == len) {
                peer->_send_buf.pop_front();
            } else {
                sbuf.erase(sbuf.begin(), sbuf.begin() + rc);  //todo: move ptr instead of call erase()
                break;
            }
        } else {
            srt_getlasterror(&error);
            if (error) {
                LOG(ant::Log::EError, ant::Log::EAnt, "srt_sendmsg(%d) error: %s(%d)\n",
                    peer->_sock, srt_getlasterror_str(), error)

                SRT_SOCKSTATUS status = srt_getsockstate(peer->_sock);
                if (status == SRTS_BROKEN || status == SRTS_NONEXIST || status == SRTS_CLOSED) {
                    peer->_status = status;
                    LOG(ant::Log::EError, ant::Log::EAnt, "peer (%s): connection lost\n",
                        ant::print_sockaddr(peer->_addr).c_str())
                    error = SRT_ECONNLOST;
                }
            }
            break;
        }
    }
}

void ant::Srt::close(Srt_connection_id const& conn_id)
{
	LOG(ant::Log::EDebug, ant::Log::EAnt, "Srt::close %d\n", conn_id);

	auto itr = _peers.find(conn_id);
    if (itr == _peers.end())
        return;

    std::lock_guard<std::mutex> lock(_peers_mt);

    Srt_connection::ptr peer = itr->second;
    srt_close(peer->_sock);
    _peers.erase(itr);
}

void ant::Srt::thread_proc()
{
    LOGS(Log::EInfo, Log::EAnt, "SRT thread is running\n")

    int peers_count = _peers.size();

    int events = SRT_EPOLL_IN | SRT_EPOLL_ERR;
    LOG(ant::Log::EDebug, ant::Log::EAnt, "listen: add polling socket: %d\n", _sock)
    int rc = srt_epoll_add_usock(_poll_id, _sock, &events);
    if (rc == SRT_ERROR) {
        LOG(ant::Log::EError, ant::Log::EAnt, "srt_epoll_add_usock() error: %s\n", srt_getlasterror_str())
        return;
    }

    auto start_time = std::chrono::steady_clock::now();

    while (!_break_loop) {
        int rnum = 1+peers_count;
        int wnum = _congestion;
        SRTSOCKET rfds[rnum], wfds[wnum];

        Chronometer<std::chrono::milliseconds> ch;
        int rc = srt_epoll_wait(_poll_id, rfds, &rnum, wfds, &wnum, 200LL, nullptr, 0, nullptr, 0);
        ch.stop();
        // LOG(ant::Log::EDebug, ant::Log::EAnt, "epoll slept for %u ms, rnum: %d, wnum: %d\n", ch.count(), rnum, wnum)
        _epoll_time_ms += ch.count();
        _epoll_events += rnum;
        if (rc > 0) {
            if (rnum) {
                LOG(ant::Log::EDebug, ant::Log::EAnt, "epoll signalled %d read events\n", rnum)
            }
            for (int i = 0; i < rnum; ++i) {
                SRT_SOCKSTATUS status = srt_getsockstate(rfds[i]);

                LOG(ant::Log::EDebug, ant::Log::EAnt, "epoll signalled for socket %d into state %d\n",
                    rfds[i], status)

                switch (status) {
                    case SRTS_CONNECTED: {
                        std::lock_guard<std::mutex> lock(_peers_mt);
                        connection_received(rfds[i]);
#if defined(USE_SRT_RECEIVE_LIMITER)
                        if (_receive_limiter) {
                            bool is_enable_receive = _receive_limiter->get_rest_limit() > 0;
                            if (!_is_receive_limit_reached && !is_enable_receive) {
                                _is_receive_limit_reached = true;
                                LOG(ant::Log::EInfo, ant::Log::EAnt, "traffic limiter is ON\n")
                                std::for_each(_peers.begin(), _peers.end(), [this](Connection_map_value const &val) {
                                    srt_epoll_remove_usock(_poll_id, val.second->_sock);
                                });
                            }
                        }
#endif
                        break;
                    }

                    case SRTS_LISTENING: {
                        std::lock_guard<std::mutex> lock(_peers_mt);
                        connection_established();
                        break;
                    }

                    case SRTS_CLOSED:
                    case SRTS_BROKEN:
                    {
                        std::lock_guard<std::mutex> lock(_peers_mt);
                        connection_broken(rfds[i]);
                        break;
                    }

                    default:
                        LOG(ant::Log::EWarning, ant::Log::EAnt, "epoll signalled for socket %d into state %d\n",
                            rfds[i], status)
                        assert(0);
                        break;
                }
                peers_count = _peers.size();
            }

            if (wnum) {
                LOG(ant::Log::EDebug, ant::Log::EAnt, "epoll signalled %d write events\n", wnum)
            }
            for (int i = 0; i < wnum; ++i) {
                SRT_SOCKSTATUS status = srt_getsockstate(wfds[i]);

                LOG(ant::Log::EDebug, ant::Log::EAnt, "epoll signalled for socket %d into state %d\n",
                    wfds[i], status)

                switch (status) {
                    case SRTS_CONNECTED: {
                        std::lock_guard<std::mutex> lock(_peers_mt);
                        connection_ready_to_send(wfds[i]);
                        break;
                    }

                    default:
                    LOG(ant::Log::EWarning, ant::Log::EAnt, "epoll signalled for socket %d into state %d\n",
                        rfds[i], status)
                        assert(0);
                        break;
                }
            }

        } else if (rc < 0) {
            int error;
            srt_getlasterror(&error);
            if (error) {
                LOG(ant::Log::EError, ant::Log::EAnt, "epoll error: %s(%d)\n", srt_getlasterror_str(), error)
                break;
            }
#if defined(USE_SRT_RECEIVE_LIMITER)
            if (_receive_limiter) {
                bool is_enable_receive = _receive_limiter->tick();
                if (_is_receive_limit_reached && is_enable_receive) {
                    _is_receive_limit_reached = false;
                    LOG(ant::Log::EInfo, ant::Log::EAnt, "traffic limiter is OFF\n")
                    std::lock_guard<std::mutex> lock(_peers_mt);
                    std::for_each(_peers.begin(), _peers.end(), [this](Connection_map_value const &val) {
                        connection_received(val.second->_sock);
                    });

                    is_enable_receive = _receive_limiter->get_rest_limit() > 0;
                    if (is_enable_receive) {
                        std::for_each(_peers.begin(), _peers.end(), [this](Connection_map_value const &val) {
                            int events = SRT_EPOLL_IN | SRT_EPOLL_ERR;
                            srt_epoll_add_usock(_poll_id, val.second->_sock, &events);
                        });
                    } else {
                        _is_receive_limit_reached = true;
                        LOG(ant::Log::EInfo, ant::Log::EAnt, "traffic limiter is still ON\n")
                    }
                }
            }
#endif
        }
        //fixme: for debug purposes only
        auto stop_time = std::chrono::steady_clock::now();
        auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(stop_time - start_time);
        if (delta.count() > 1000) {
            for (auto &itr: _peers) {
                LOG(Log::EInfo, Log::EAnt, "connection(%d): TS: %d, epoll_time: %u ms, events: %u; read count: %u\n",
                    itr.first, delta.count(), _epoll_time_ms, _epoll_events, itr.second->_read_count)
                itr.second->_read_count = 0;
            }
            _epoll_time_ms = 0;
            _epoll_events = 0;

            start_time = stop_time;
        }
    }

    LOGS(Log::EInfo, Log::ESrt, "thread stopped\n")
}

void ant::Srt::connection_established()
{
    int len = 0;
    if (_addr.ss_family == AF_INET)
        len = sizeof(sockaddr_in);
    if (_addr.ss_family == AF_INET6)
        len = sizeof(sockaddr_in6);

    Srt_connection::ptr peer = std::make_shared<Srt_connection>();
    peer->_status = SRTS_CONNECTED;
    peer->_sock = srt_accept(_sock, (struct sockaddr *) &peer->_addr, &len);
    assert(peer->_sock != SRT_ERROR);

    LOG(ant::Log::EDebug, ant::Log::EAnt, "peer (%s): new incoming connection\n",
        ant::print_sockaddr(peer->_addr).c_str())

    int opt = 0;
    int opt_len = sizeof opt;
    srt_getsockflag(peer->_sock, SRTO_UDP_SNDBUF, &opt, &opt_len);
    LOG(ant::Log::EInfo, ant::Log::EAnt, "SRTO_UDP_SNDBUF is %d bytes\n", opt)
    srt_getsockflag(peer->_sock, SRTO_UDP_SNDBUF, &opt, &opt_len);
    LOG(ant::Log::EInfo, ant::Log::EAnt, "SRTO_UDP_RCVBUF is %d bytes\n", opt)
    srt_getsockflag(peer->_sock, SRTO_MSS, &opt, &opt_len);
    LOG(ant::Log::EInfo, ant::Log::EAnt, "SRTO_MSS is %d bytes\n", opt)
    peer->_mss = opt;

    opt = 0;
    srt_setsockflag(peer->_sock, SRTO_SNDSYN, &opt, opt_len);
    opt = 0;
    srt_setsockflag(peer->_sock, SRTO_RCVSYN, &opt, opt_len);
    opt = 0;
    srt_setsockflag(peer->_sock, SRTO_LINGER, &opt, opt_len);
    opt = 0;
    srt_setsockflag(peer->_sock, SRTO_INPUTBW, &opt, opt_len);
    opt = 100;
    srt_setsockflag(peer->_sock, SRTO_OHEADBW, &opt, opt_len);

    int events = SRT_EPOLL_IN | SRT_EPOLL_ERR;
    int rc = srt_epoll_add_usock(_poll_id, peer->_sock, &events);
    LOG(ant::Log::EDebug, ant::Log::EAnt, "add polling socket: %d\n", peer->_sock)
    if (rc == SRT_ERROR) {
        assert(0);
        LOG(ant::Log::EError, ant::Log::EAnt, "srt_epoll_add_usock() error: %s\n", srt_getlasterror_str())
        return;
    }

    _peers[peer->_sock] = peer;

    if (_events) {
        _peers_mt.unlock();
        _ant_network->do_asynch(std::bind(&Srt_events::srt_on_accept, _events, peer->_sock, peer->_addr));
        _peers_mt.lock();
    }
}

void ant::Srt::connection_received(SRTSOCKET s)
{
    Srt_connection::ptr peer = _peers[s];

    peer->_read_count++;

    for(;;) {
        Chronometer<std::chrono::milliseconds> ch;
        int opt = 0;
        int opt_len = sizeof opt;
        srt_getsockflag(peer->_sock, SRTO_RCVDATA, &opt, &opt_len);
        int buf_size = opt * peer->_mss;

        LOG(ant::Log::EDebug, ant::Log::EAnt, "srt: ready for receiving %d bytes\n", buf_size)
#if defined(USE_SRT_RECEIVE_LIMITER)
        if (_receive_limiter && _receive_limiter->get_rest_limit() < buf_size)
            buf_size = _receive_limiter->get_rest_limit();
#endif
        std::vector<uint8_t> rbuf(buf_size ? buf_size : SRT_BUF_SIZE);

        int rc = srt_recvmsg(peer->_sock, (char *) rbuf.data(), rbuf.size());
        if (rc > 0) {
            rbuf.resize(rc);
            ch.stop();
            LOG(ant::Log::EDebug, ant::Log::EAnt, "srt_recv(%d) = %d bytes at %u msec\n", peer->_sock, rc, ch.count())
            ch.reset();

            SRT_SOCKSTATUS peer_sock_status = peer->_status;

            _peers_mt.unlock();

            if (peer_sock_status != SRTS_CONNECTED) {
                int opt = 0;
                int opt_len = sizeof opt;
                srt_getsockflag(peer->_sock, SRTO_UDP_SNDBUF, &opt, &opt_len);
                LOG(ant::Log::EInfo, ant::Log::EAnt, "SRTO_UDP_SNDBUF is %d bytes\n", opt)
                srt_getsockflag(peer->_sock, SRTO_UDP_SNDBUF, &opt, &opt_len);
                LOG(ant::Log::EInfo, ant::Log::EAnt, "SRTO_UDP_RCVBUF is %d bytes\n", opt)
                srt_getsockflag(peer->_sock, SRTO_MSS, &opt, &opt_len);
                LOG(ant::Log::EInfo, ant::Log::EAnt, "SRTO_MSS is %d bytes\n", opt)
                peer->_mss = opt;

                opt = 0;
                opt_len = sizeof opt;
                srt_setsockflag(peer->_sock, SRTO_INPUTBW, &opt, opt_len);
                opt = 100;
                srt_setsockflag(peer->_sock, SRTO_OHEADBW, &opt, opt_len);

                if (_events)
                    _ant_network->do_asynch(
                            std::bind(&Srt_events::srt_on_connect, _events, s, peer->_addr));
            }

            if (_events)
                _ant_network->do_asynch(std::bind(&Srt_events::srt_on_recv, _events, s, rbuf));

            _peers_mt.lock();

            if (peer->_status != SRTS_CONNECTED) {
                peer->_status = SRTS_CONNECTED;
            }
#if defined(USE_SRT_RECEIVE_LIMITER)
            if (_receive_limiter) {
                bool is_enable_receive = _receive_limiter->ready_data(rc);
                LOG(ant::Log::EDebug, ant::Log::EAnt, "traffic limiter: rest limit is %d bytes\n",
                    _receive_limiter->get_rest_limit())
                if (!is_enable_receive)
                    break;
            }
#endif
        } else if (rc < 0) {
            int error;
            srt_getlasterror(&error);
            if (error) {
                LOG(ant::Log::EError, ant::Log::EAnt, "srt_recv() error: %s(%d)\n", srt_getlasterror_str(), error)
            }
            break;
        }
    }
}

void ant::Srt::connection_ready_to_send(SRTSOCKET s)
{
    Srt_connection::ptr peer = _peers[s];

    internal_send(peer);

    if (peer->_bufsize <= peer->_lwm && peer->_congestion) {
        LOG(ant::Log::EWarning, ant::Log::EAnt, "peer (%s): LWM\n", ant::print_sockaddr(peer->_addr).c_str())

        peer->_congestion = Srt_connection::ENoCongestion;
        _congestion = congested_connection_count();

        int events = SRT_EPOLL_IN | SRT_EPOLL_ERR;
        srt_epoll_remove_usock(_poll_id, peer->_sock);
        int rc = srt_epoll_add_usock(_poll_id, peer->_sock, &events);
        if (rc == SRT_ERROR) {
            LOG(ant::Log::EError, ant::Log::EAnt, "srt_epoll_add_usock() error: %s\n",
                srt_getlasterror_str())
            assert(0);
        }

        if (peer->_stats)
            peer->_stats->push_buffer_event(peer->_bufsize);

        if (_events) {
            _peers_mt.unlock();
            _ant_network->do_asynch(std::bind(&Srt_events::srt_on_lwm, _events, peer->_sock));
            _peers_mt.lock();
        }
    }
}

void ant::Srt::connection_broken(SRTSOCKET s)
{
    auto itr = _peers.find(s);
    if (itr == _peers.end()) {
        assert(0);
        return;
    }
    Srt_connection::ptr peer = itr->second;

    int error;
    srt_getlasterror(&error);

    if (peer->_status == SRTS_CONNECTING) {
        if (_events) {
            _peers_mt.unlock();
            _ant_network->do_asynch(std::bind(&Srt_events::srt_on_connect_error, _events,
                                              peer->_sock, peer->_addr, srt_getlasterror_str()));
            _peers_mt.lock();
        }
    } else {
        if (error) {
            LOG(ant::Log::EWarning, ant::Log::EAnt,
                "peer (%s): connection(%d) lost with error: %s(%d)\n",
                ant::print_sockaddr(peer->_addr).c_str(), peer->_sock, srt_getlasterror_str(), error)
        } else {
            LOG(ant::Log::EWarning, ant::Log::EAnt,
                "peer (%s): connection(%d) closed by remote side\n",
                ant::print_sockaddr(peer->_addr).c_str(), peer->_sock)
        }

        peer->_send_buf.clear();
        peer->_bufsize = 0;

        if (peer->_stats)
            peer->_stats->push_buffer_event(peer->_bufsize);

        if (_events) {
            _peers_mt.unlock();
            _ant_network->do_asynch(std::bind(&Srt_events::srt_on_break, _events, s));
            _peers_mt.lock();
        }
    }

    srt_epoll_remove_usock(_poll_id, peer->_sock);
    _peers.erase(itr);

    _congestion = congested_connection_count();
}
