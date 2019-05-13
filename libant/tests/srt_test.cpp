//
// Created by Oleg Golosovskiy on 01/12/2018.
// Created by Aleksey Dorofeev on 01/12/2018.
//

#include <iostream>
#include <thread>
#include <getopt.h>
#include <memory>
#include <random>
#include <algorithm>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <list>
#include <condition_variable>

#include "logger.h"
#include "utils.hpp"
#include "stat.h"
#include <limits.h>

#include <srt.h>
#include <logging.h>
#include <logging_api.h>

#include "apputil.hpp"
#include "uriparser.hpp"
#include "logsupport.hpp"

#include "network.h"
#include "libsrt.h"
#include "bencode.h"

const int DEFAULT_PORT = 3010;

static int o_debug = 0;
static bool o_listen = false;
static bool o_echo = false;
static uint16_t o_local_ant_port = DEFAULT_PORT;
static uint16_t o_local_srt_port = DEFAULT_PORT+1;
static bool o_rendezvous_mode = false;
static std::list<std::string> o_remote_address;
static int o_bufsize = SRT_LIVE_DEF_PLSIZE;
static int o_hwm = -1;
static int o_lwm = -1;
static int o_send_timeout_ms = 1000;
static int o_inter_timeout_ms = 1;
static int o_timeout = 60;


static void usage(char *name)
{
    fprintf(stderr, "\nUsage:\n");
    fprintf(stderr, "    run as server to receive data from all it's clients:\n");
    fprintf(stderr, "    %s -l [options]\n", name);
    fprintf(stderr, "    or run as client to send data to the server:\n");
    fprintf(stderr, "    %s [options] <dest_IP:dest_port>\n", name);
    fprintf(stderr, "    or run into rendezvous mode:\n");
    fprintf(stderr, "    %s -r [options] <dest_IP:dest_port>\n", name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "    -h              Help\n");
    fprintf(stderr, "    -v              Verbose mode; use multiple times to increase verbosity.\n");
    fprintf(stderr, "    -l              Listen mode\n");
    fprintf(stderr, "    -r              Rendezvous mode (local and remote ports must be equals!)\n");
    fprintf(stderr, "    -s <port>       Local port\n");
    fprintf(stderr, "    -e              Echo mode for server only, to send all receive data back to the client\n");
    fprintf(stderr, "    -b <bufsize>    Buffer size to send, by default %d bytes\n", o_bufsize);
    fprintf(stderr, "    -t <msec>       Period in milliseconds to send buffer, by default %d ms\n", o_send_timeout_ms);
    fprintf(stderr, "    -i <msec>       Interval in milliseconds between shot to each peer, by default 0\n");
    fprintf(stderr, "    -T <sec>        Common timeout, by default 60\n");
    fprintf(stderr, "    -H <hwm>        High Water Mark in bytes\n");
    fprintf(stderr, "    -L <lwm>        Low Water Mark in bytes\n");
    fprintf(stderr, "\n");
    exit(1);
}

//extern "C"
//void srt_log_handler(void* opaque, int level, const char* file, int line, const char* area, const char* message)
//{
//    struct timeval tv;
//    gettimeofday(&tv, NULL);
//    char cur_time_str[32];
//    strftime(cur_time_str, sizeof(cur_time_str), "%F %H:%M:%S", gmtime(&tv.tv_sec));
//    char cur_time_str_ms[40];
//    int msec = (int) (tv.tv_usec / 1000);
//    snprintf(cur_time_str_ms, sizeof(cur_time_str_ms), "%s.%03d", cur_time_str, msec);
//
//    fprintf(stdout, "%s %s", cur_time_str_ms, message);
//    fflush(stdout);
//}

void log(const char *text)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	char cur_time_str[32];
	strftime(cur_time_str, sizeof(cur_time_str), "%F %H:%M:%S", gmtime(&tv.tv_sec));
	char cur_time_str_ms[40];
	int msec = (int) (tv.tv_usec / 1000);
	snprintf(cur_time_str_ms, sizeof(cur_time_str_ms), "%s.%03d", cur_time_str, msec);

	fprintf(stdout, "%s %s", cur_time_str_ms, text);
	fflush(stdout);
}

#define EVERY_5_SEC(func) \
    if (_5_sec_interval >= 5000)\
        func;
#define EVERY_30_SEC(func) \
    if (_30_sec_interval >= 30000)\
        func;

struct Statistics {
    uint32_t sent_bytes;
    uint32_t sent_packs;
    uint32_t recv_bytes;
    uint32_t recv_packs;
};

void operator +=(Statistics &left, Statistics const& right) {
    left.sent_bytes += right.sent_bytes;
    left.sent_packs += right.sent_packs;
    left.recv_bytes += right.recv_bytes;
    left.recv_packs += right.recv_packs;
}

#define NTP_SIZE 8
typedef uint8_t ntp_bytearray[NTP_SIZE];
#define OFFSET 2208988800ULL

void ntp2tv(uint8_t *ntp, struct timeval *tv)
{
    uint64_t aux = 0;
    uint8_t *p = ntp;
    int i;

    /* we get the ntp in network byte order, so we must
     * convert it to host byte order. */
    for (i = 0; i < NTP_SIZE/2; i++) {
        aux <<= 8;
        aux |= *p++;
    }

    /* now we have in aux the NTP seconds offset */
    aux -= OFFSET;
    tv->tv_sec = aux;

    /* let's go with the fraction of second */
    aux = 0;
    for (; i < NTP_SIZE; i++) {
        aux <<= 8;
        aux |= *p++;
    }

    /* now we have in aux the NTP fraction (0..2^32-1) */
    aux *= 1000000; /* multiply by 1e6 */
    aux >>= 32;     /* and divide by 2^32 */
    tv->tv_usec = aux;
}

void tv2ntp(struct timeval *tv, uint8_t *ntp)
{
    uint64_t aux = 0;
    uint8_t *p = ntp + NTP_SIZE;
    int i;

    aux = tv->tv_usec;
    aux <<= 32;
    aux /= 1000000;

    /* we set the ntp in network byte order */
    for (i = 0; i < NTP_SIZE/2; i++) {
        *--p = aux & 0xff;
        aux >>= 8;
    }

    aux = tv->tv_sec;
    aux += OFFSET;

    /* let's go with the fraction of second */
    for (; i < NTP_SIZE; i++) {
        *--p = aux & 0xff;
        aux >>= 8;
    }
}

size_t print_tv(struct timeval *tv)
{
    return printf("%ld.%06d", tv->tv_sec, tv->tv_usec);
}

size_t print_ntp(uint8_t *ntp)
{
    int i;
    int res = 0;
    for (i = 0; i < NTP_SIZE; i++) {
        if (i == NTP_SIZE/2)
            res += printf(".");
        res += printf("%02x", ntp[i]);
    }
    res += printf("\n");
    return res;
}

const char DATA_REQUEST[] = "DATA";
const char HANDSHAKE[] = "HANDSHAKE";
const char DATA_REPLY[] = "ACK";

struct Peer {
    Peer() : recv_stat(0, 30000), send_stat(0, 30000), _congestion(false)
    {
        memset(&_sum_stat, 0, sizeof _sum_stat);
        memset(&_cur_stat, 0, sizeof _cur_stat);
    }
    sockaddr_storage _addr;

    std::vector<uint8_t> sbuf;
    std::vector<uint8_t> rbuf;

    Statistics _sum_stat;
    Statistics _cur_stat;

    Moving_average<unsigned> recv_stat;
    Moving_average<unsigned> send_stat;

    bool _congestion;
};

class Srt_test : public ant::Srt_events
{
protected:
    ant::Srt *_srt;
    std::thread *_thread;
    bool _break_loop;

    std::chrono::time_point<std::chrono::steady_clock> _last_tick;
    uint32_t _5_sec_interval;
    uint32_t _30_sec_interval;

    std::map<ant::Srt_connection_id, Peer*> _peers;
    std::mutex _peers_mt;

public:
    Srt_test(ant::Network::ptr net) : _thread(nullptr), _break_loop(false)
    {
        _srt = new ant::Srt(this, net);

        _5_sec_interval = 0;
        _30_sec_interval = 0;
    }

    virtual ~Srt_test()
    {
        delete _srt;
    }

    void start(sockaddr_storage const& bind_addr, int port, std::list<std::string> const& remote_address, ant::Srt_connecting_cb const& addr_cb)
    {
        sockaddr_storage bind_interface = bind_addr;
        ant::set_port(bind_interface, port);
        _srt->start(bind_interface);
        cmd_id = 0;

        _thread = new std::thread(&Srt_test::thread_proc, this);

        for (auto const& item: remote_address) {
            struct addrinfo hints, *dest_addr;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_DGRAM;
            hints.ai_protocol = IPPROTO_UDP;

            std::string ip;
            std::string port = std::to_string(SRT_DEFAULT_PORT);
            auto pos = item.find_last_of(':');
            if (pos != std::string::npos) {
                ip = item.substr(0, pos);
                port = item.substr(pos + 1, item.length() - 1);
            } else {
                ip = item;
            }
            int rc = getaddrinfo(ip.c_str(), port.c_str(), &hints, &dest_addr);
            if (rc) {
                LOG(ant::Log::EError, ant::Log::EAnt, "getaddrinfo() error: %s\n", gai_strerror(rc))
                continue;
            }

            sockaddr_storage sa;
            memcpy(&sa, dest_addr->ai_addr, dest_addr->ai_addrlen);

            ant::Srt_connection_id conn_id = SRT_EMPTY_CONN_ID;
            _srt->connect(sa, conn_id, addr_cb);

            freeaddrinfo(dest_addr);
        }
    }

    void stop()
    {
        if (_thread) {
            _break_loop = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(o_send_timeout_ms));
            _thread->join();
            delete _thread;
            _thread = nullptr;
        }

        _srt->stop();

        for (auto itr: _peers) {
            delete itr.second;
        }
        _peers.clear();
    }

    sockaddr_storage get_listen() const
    {
        return _srt->getbindaddr();
    }

    void srt_on_connect(ant::Srt_connection_id const& conn_id, sockaddr_storage const &to_addr) override
    {
        LOG(ant::Log::EInfo, ant::Log::EAnt, "srt_test: established outgoing connection(%d) to %s\n",
            conn_id, ant::print_sockaddr(to_addr).c_str());

        {
            std::lock_guard<std::mutex> lock(_peers_mt);
            _peers[conn_id] = new Peer;
        }

        if (o_hwm != -1 && o_lwm != -1)
            _srt->set_buffer(conn_id, -1, o_hwm, o_lwm);
    }

    void srt_on_connect_error(ant::Srt_connection_id const &conn_id, sockaddr_storage const &to_addr,
                              std::string const &errcode) override
    {
        LOG(ant::Log::EWarning, ant::Log::EAnt, "srt_test: cannot connect!\n")
    }

    void srt_on_accept(ant::Srt_connection_id const &conn_id, sockaddr_storage const &from_addr) override
    {
        LOG(ant::Log::EInfo, ant::Log::EAnt, "srt_test: established incoming connection(%d) from %s\n",
            conn_id, ant::print_sockaddr(from_addr).c_str());

        {
            std::lock_guard<std::mutex> lock(_peers_mt);
            _peers[conn_id] = new Peer;
        }

        if (o_hwm != -1 && o_lwm != -1)
            _srt->set_buffer(conn_id, -1, o_hwm, o_lwm);

        std::vector<uint8_t> buffer;
        std::vector<uint8_t> out_buffer = encode_packet(buffer, HANDSHAKE);
        _srt->send(conn_id, std::move(out_buffer));
    }


    int parse_packet(std::vector<uint8_t> const& data,
            std::vector<uint8_t>& in_command, int& in_cmd_id,
            std::vector<uint8_t>& in_ntp, std::vector<uint8_t>& in_data, int& remaining) {

        unsigned char const *p = data.data();
        size_t eat_bytes;

        remaining = 0;

        int rc;
        int data_len = data.size();
        rc = ant::bencode_parse_byte_string(p, data_len, &eat_bytes, in_command);
        assert(rc == 0);
        in_command.push_back(0);
        p += eat_bytes;
        data_len -= eat_bytes;
        rc = ant::bencode_parse_integer(p, data_len, &eat_bytes, &in_cmd_id);
        assert(rc == 0);
        p += eat_bytes;
        data_len -= eat_bytes;
        rc = ant::bencode_parse_byte_string(p, data_len, &eat_bytes, in_ntp);
        assert(rc == 0);
        assert(in_ntp.size() == 8);
        p += eat_bytes;
        data_len -= eat_bytes;
        if (data_len) {
            rc = ant::bencode_parse_byte_string(p, data_len, &eat_bytes, in_data);
            data_len -= eat_bytes;
        }

        if(rc == 0) {
            remaining = data_len;
        }

        return rc;
    }

    void srt_on_recv(ant::Srt_connection_id const &conn_id, std::vector<uint8_t> data) override
    {
        std::lock_guard<std::mutex> lock(_peers_mt);

        LOG(ant::Log::EDebug, ant::Log::EAnt, "new packet: %d\n", data.size());

        auto itr = _peers.find(conn_id);
        if (itr != _peers.end()) {
            Peer *peer = itr->second;
            peer->_cur_stat.recv_packs++;
            peer->_cur_stat.recv_bytes += data.size();

            //todo: deadlock happens if send data back from server
/*            if (o_listen && o_echo) {
                std::copy(peer->rbuf.end(), data.begin(), data.end());
            } */

            peer->rbuf.insert( peer->rbuf.end(), data.begin(), data.end() );

            // Message's structure is <byte string>COMMAND<integer>ID<byte string[8]>NTP_TIMESTAMP<byte string>[DATA]
            std::vector<uint8_t> in_command;
            int in_cmd_id;
            std::vector<uint8_t> in_ntp;
            std::vector<uint8_t> in_data;
            int remaining;
            int res = parse_packet(peer->rbuf, in_command, in_cmd_id, in_ntp, in_data, remaining);

            if(res==EAGAIN)
            // message is not full
                return;

            assert(res == 0);
            peer->rbuf.erase(peer->rbuf.begin(),peer->rbuf.end()-remaining);

            LOG(ant::Log::EInfo, ant::Log::EAnt,
                    "command: %s size: %d seq: %d\n",
                    in_command.data(), in_data.size(), in_cmd_id);

            if(0 == memcmp( in_command.data(), DATA_REQUEST, sizeof(DATA_REQUEST)-1)) {
                assert(in_ntp.size()==8);
                std::vector<uint8_t>  ack = encode_packet(std::vector<uint8_t>(), DATA_REPLY, in_cmd_id, in_ntp.data());
                _srt->send(conn_id, std::move(ack));
            }

            if(0 == memcmp( in_command.data(), DATA_REPLY, sizeof(DATA_REPLY)-1)) {
                assert(in_ntp.size()==8);
                struct timeval tv, in_tv;
                gettimeofday(&tv, 0);
                ntp2tv(in_ntp.data(), &in_tv);
                int32_t diff_usec = tv.tv_usec - in_tv.tv_usec;
                int32_t diff_sec = tv.tv_sec - in_tv.tv_sec;
                float rtt = (float)(diff_sec * 1e6 + diff_usec) / 1e6;
                LOG(ant::Log::EInfo, ant::Log::EAnt,
                    "PACKET TRIP seq: %d rtt: %f sec\n", in_cmd_id, rtt);
                assert(rtt < 2.0);

            }
        }
    }

    void srt_on_lwm(ant::Srt_connection_id const &conn_id) override
    {
        std::lock_guard<std::mutex> lock(_peers_mt);

        auto itr = _peers.find(conn_id);
        if (itr != _peers.end()) {
            Peer *peer = itr->second;
            if (peer->_congestion) {
                peer->_congestion = false;
                LOG(ant::Log::EWarning, ant::Log::EAnt, "peer(): LWM\n", itr->first)
            }
        }
    }

    void srt_on_break(ant::Srt_connection_id const &conn_id) override
    {
        std::lock_guard<std::mutex> lock(_peers_mt);

        auto itr = _peers.find(conn_id);
        if (itr != _peers.end()) {
            delete itr->second;
            _peers.erase(itr);
        }
    }

private:

    int cmd_id;

    std::vector<uint8_t> encode_packet(std::vector<uint8_t> const& buffer, const char * command, int id = 0, uint8_t* ntp_ptr = 0) {


        uint8_t ntp[8];
        if(ntp_ptr == 0) {
            struct timeval tv;
            gettimeofday(&tv, 0);
            tv2ntp(&tv, ntp);
        } else {
            memcpy(ntp, ntp_ptr, 8);
        }

        if(id == 0) {
            if (++cmd_id == INT_MAX)
                cmd_id = 0;
            id = cmd_id;
        }

        std::vector<uint8_t> out_vec;

        // Message's structure is <byte string>COMMAND<integer>ID<byte string[8]>NTP_TIMESTAMP<byte string>[DATA]
        if(buffer.size()>0)
            ant::bencode_encode_byte_string(out_vec, buffer.data(), buffer.size());
        ant::bencode_encode_byte_string(out_vec, ntp, sizeof(ntp));
        ant::bencode_encode_integer(out_vec, id);
        ant::bencode_encode_byte_string(out_vec, (unsigned char const *) command, strlen(command));

        return out_vec;
    }

    void thread_proc()
    {
        // prepare sending buffer
        std::vector<uint8_t> buffer(o_bufsize);
        unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
        std::shuffle(buffer.begin(), buffer.end(), std::default_random_engine(seed));

        _last_tick = std::chrono::steady_clock::now();

        while (!_break_loop) {
            std::this_thread::sleep_for(std::chrono::milliseconds(o_send_timeout_ms));

            std::lock_guard<std::mutex> lock(_peers_mt);

            auto now = std::chrono::steady_clock::now();
            auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(now - _last_tick);

            for (auto itr = _peers.begin(); itr != _peers.end(); ++itr) {
                auto peer = itr->second;

                if (!o_listen) {
                    peer->sbuf = encode_packet(buffer, DATA_REQUEST);
                } else if (o_listen && o_echo) {
                    //todo: deadlock happens if send data back from server
                    peer->sbuf = peer->rbuf;
                }

                if (!peer->_congestion && peer->sbuf.size()) {
                    auto buf = peer->sbuf;

                    LOG(ant::Log::EInfo, ant::Log::EAnt, "send command: DATA size: %d seq: %d\n", buffer.size(), cmd_id);

                    int rc = _srt->send(itr->first, std::move(buf));
                    peer->_cur_stat.sent_packs++;
                    peer->_cur_stat.sent_bytes += peer->sbuf.size();
                    peer->sbuf.clear();
                    if (rc == ant::Srt::ESendHWM && !peer->_congestion) {
                        peer->_congestion = true;
                        LOG(ant::Log::EWarning, ant::Log::EAnt, "peer(): HWM\n", itr->first)
                    }
                }
            }

            _5_sec_interval += dt.count();
            _30_sec_interval += dt.count();

            EVERY_5_SEC(show_5_sec_statistics());
            EVERY_30_SEC(show_30_sec_statistics());

            _last_tick = now;

            EVERY_5_SEC(_5_sec_interval = 0)
            EVERY_30_SEC(_30_sec_interval = 0)
        }
    }

protected:
    void show_5_sec_statistics()
    {
        for (auto itr: _peers) {
            auto peer = itr.second;
            //std::string str_addr = ant::print_sockaddr(peer->_addr);

            peer->send_stat.add_val(peer->_cur_stat.sent_bytes);
            peer->recv_stat.add_val(peer->_cur_stat.recv_bytes);

            float tx_rate = peer->_cur_stat.sent_bytes/_5_sec_interval/1000.0*8;
            float rx_rate = peer->_cur_stat.recv_bytes/_5_sec_interval/1000.0*8;

            LOG(ant::Log::EInfo, ant::Log::EAnt,
                "connection(%d): APP statistics: TS:%d"
                " send %04u pkt/%06u bytes with rate %.04f mbps,"
                " recv %04u pkt/%06u bytes with rate %.04f mbps\n",
                itr.first, _5_sec_interval,
                peer->_cur_stat.sent_packs, peer->_cur_stat.sent_bytes, tx_rate,
                peer->_cur_stat.recv_packs, peer->_cur_stat.recv_bytes, rx_rate);

            peer->_sum_stat += peer->_cur_stat;
            memset(&peer->_cur_stat, 0, sizeof peer->_cur_stat);

            SRT_TRACEBSTATS last_stat;
            //sockaddr_storage src_addr;// = _net->getbindaddr();

            srt_bistats(itr.first, &last_stat, 1, 1);
            //srt_getpeername(itr.first, (struct sockaddr *) &src_addr, &addr_len);

            //std::string peer_addr = print_sockaddr(src_addr);

            int pktUnackedSent = 0;
            int opt_len = sizeof pktUnackedSent;
            srt_getsockflag(itr.first, SRTO_SNDDATA, &pktUnackedSent, &opt_len);

            LOG(ant::Log::EInfo, ant::Log::EAnt,
                "connection(%d): SRT statistics: TS:%d"
                " send %04u pkt/%06u bytes with rate %.04f mbps, retransmit %03d pkt, non-acked %03d pkt,"
                " recv %04u pkt/%06u bytes with rate %.04f mbps, lost %03d pkt,"
                " rtt %.1f ms, estimate bw: %.04f mbps\n",
                itr.first, _5_sec_interval,
                last_stat.pktSent, last_stat.byteSent, last_stat.mbpsSendRate, last_stat.pktRetrans, pktUnackedSent,
                last_stat.pktRecv, last_stat.byteRecv, last_stat.mbpsRecvRate, last_stat.pktRcvLoss,
                last_stat.msRTT, last_stat.mbpsBandwidth)
        }
    }

    void show_30_sec_statistics()
    {
        for (auto itr: _peers) {
            auto peer = itr.second;
            //std::string str_addr = ant::print_sockaddr(itr.second->_addr);

            {
                printf("srt_test: connection(%d): %s: ", itr.first, "average send mbps: ");
                int i = peer->send_stat.intervals_count() < 5 ? peer->send_stat.intervals_count()-1 : 4;
                for (; i >= 0; --i) {
                    printf("%.4f, ", peer->send_stat.ave_val(-i) / 5 / 1000000.0 * 8);
                }
                printf("\n");
            }
            {
                printf("srt_test: connection(%d): %s: ", itr.first, "average recv mbps: ");
                int i = peer->recv_stat.intervals_count() < 5 ? peer->recv_stat.intervals_count()-1 : 4;
                for (; i >= 0; --i) {
                    printf("%.4f, ", peer->recv_stat.ave_val(-i)/5/1000000.0*8);
                }
                printf("\n");
            }
        }
    }
};



int main(int argc, char* argv[])
{
	while(true) {
		char c = getopt(argc, argv, "hvlrecxs:b:t:i:T:H:L:");
		if (c == -1) break;
		switch(c) {
			case 'h':
				usage(argv[0]);
				break;
			case 'v':
				o_debug++;
				break;
			case 'l':
				o_listen = true;
				break;
            case 'e':
                o_echo = true;
                break;
            case 'r':
                o_rendezvous_mode = true;
                break;
			case 's':
				if (optarg)
					o_local_ant_port = std::stoi(optarg);
					o_local_srt_port = o_local_ant_port + 1;
				break;
			case 'b':
				if (optarg)
					o_bufsize = std::stoi(optarg);
				break;
			case 't':
				if (optarg)
					o_send_timeout_ms = std::stoi(optarg);
				break;
			case 'i':
				if (optarg)
					o_inter_timeout_ms = std::stoi(optarg);
				break;
			case 'T':
				if (optarg)
					o_timeout = std::stoi(optarg);
				break;
            case 'H':
                o_hwm = std::stoi(optarg);
                break;
            case 'L':
                o_lwm = std::stoi(optarg);
                break;
			default:
				throw std::runtime_error("Unhandled argument\n");
		}
	}

	for (int i = optind; i < argc; i++)
		o_remote_address.push_back(argv[i]);

    ant::Log::set(log);
    switch (o_debug) {
        case 0:
            ant::Log::enable_log_name(ant::Log::ESrt, ant::Log::EError);
            srt_setloglevel(srt_logging::LogLevel::error);
            ant::Log::enable_log_name(ant::Log::EAnt, ant::Log::EError);
            break;
        case 1:
            ant::Log::enable_log_name(ant::Log::ESrt, ant::Log::EWarning);
            srt_setloglevel(srt_logging::LogLevel::warning);
            ant::Log::enable_log_name(ant::Log::EAnt, ant::Log::EWarning);
            break;
        case 2:
            ant::Log::enable_log_name(ant::Log::ESrt, ant::Log::EInfo);
            srt_setloglevel(srt_logging::LogLevel::note);
            ant::Log::enable_log_name(ant::Log::EAnt, ant::Log::EInfo);
            break;
        default:
            ant::Log::enable_log_name(ant::Log::ESrt, ant::Log::EDebug);
            //srt_setloglevel(srt_logging::LogLevel::debug);
            ant::Log::enable_log_name(ant::Log::EAnt, ant::Log::EDebug);
            break;
    }
	ant::Log::enable_log_name(ant::Log::EDht, ant::Log::EInfo);
	ant::Log::enable_log_name(ant::Log::ENet, ant::Log::EInfo);

//    srt_setlogflags( 0
//                     | SRT_LOGF_DISABLE_TIME
//                     | SRT_LOGF_DISABLE_THREADNAME
//    );
//    srt_setloghandler(nullptr, srt_log_handler);


    LOG(ant::Log::EInfo, ant::Log::EAnt, "Starting...\n");

	ant::Network::ptr net(new ant::Network(0));

    // choose appropriate interface and port
    sockaddr_storage bind_interface;
    ant::Network::find_interface("router.bittorrent.com", 6881, bind_interface);
    ant::set_port(bind_interface, o_local_ant_port);
    LOG(ant::Log::EDebug, ant::Log::ENet, "appropriate local interface is %s, network port %d, srt port\n",
        ant::sockaddr_storage_to_host_name(bind_interface).c_str(), o_local_ant_port, o_local_srt_port);

    if (net->start(bind_interface)) {
        std::cerr << "Network can't start" << std::endl;
        exit(1);
    }

	std::string ip;
	uint16_t port = DEFAULT_PORT;

#ifdef ANT_UNIT_TESTS
	// test comparator
	struct sockaddr_storage laddr;
	struct sockaddr_storage raddr;
	memset(&laddr, 0, sizeof(laddr));
	memset(&raddr, 0, sizeof(raddr));
	laddr.ss_family = AF_INET;
	raddr.ss_family = AF_INET;
	((struct sockaddr_in *) (&laddr)) ->sin_port = htons(62202);
	((struct sockaddr_in *) (&laddr)) ->sin_addr.s_addr = inet_addr("192.168.1.142");

	((struct sockaddr_in *) (&raddr)) ->sin_port = htons(50032);
	((struct sockaddr_in *) (&raddr)) ->sin_addr.s_addr = inet_addr("192.168.1.142");

	assert(false == ant::sock_addr_less(&laddr, &laddr));
	assert(true == ant::sock_addr_less(&raddr, &laddr));
	assert(false == ant::sock_addr_less(&laddr, &raddr));

	((struct sockaddr_in *) (&raddr)) ->sin_port = htons(50032);
	((struct sockaddr_in *) (&raddr)) ->sin_addr.s_addr = inet_addr("192.168.1.141");
	assert(true == ant::sock_addr_less(&raddr, &laddr));
	assert(false == ant::sock_addr_less(&laddr, &raddr));
#endif
    Srt_test *app = new Srt_test(net);

    ant::Srt_connecting_cb connect_callback;

	std::this_thread::sleep_for(std::chrono::seconds(1));
	// TODO этот старт стоит разделить, он разный для конектора и ассептора
	// это вызывет проблемы при мультиконекте так как тогда нам нужно передать столько callback сколько конектов
	app->start(net->getbindaddr(), o_local_srt_port, o_remote_address, connect_callback);

	for (int i = 1; i < o_timeout; ++i) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	std::cout << "Stopping..." << std::endl;
	app->stop();
	delete app;

    std::this_thread::sleep_for(std::chrono::seconds(1));
}
