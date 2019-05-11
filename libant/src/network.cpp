#include <iostream>
#include <cassert>
#include <cstring>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <functional>
#include "network.h"
#include "logger.h"
#include "utils.hpp"
#include <iostream>
#include <random>
#include <ctime>
#ifdef ANT_UNIT_TESTS
# include <gtest/gtest.h>
#endif

#if ! defined __ANDROID__ && ! defined _WINDOWS
#    include <ifaddrs.h>
#endif

#ifdef ANT_UNIT_TESTS
namespace ant {

TEST(Network, ipAddrChecking)
{
    ant::Network _net(nullptr);

    ASSERT_EQ(sizeof(in_addr), 4);
    ASSERT_EQ(sizeof(in6_addr), 16);
    ASSERT_EQ(sizeof(sockaddr), 16);
    ASSERT_EQ(sizeof(sockaddr_in), 16);
    ASSERT_EQ(sizeof(sockaddr_in6), 28);
    ASSERT_EQ(sizeof(sockaddr_storage), 128);

    {
        sockaddr_storage sa;
        char straddr[INET_ADDRSTRLEN];
        memset(straddr, 0, INET_ADDRSTRLEN);
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(20202);

        inet_pton(AF_INET, "172.16.17.249", &(addr.sin_addr));
        inet_ntop(AF_INET, &addr.sin_addr, straddr, sizeof(straddr));
        std::cout << "sample IPv4 addr: " << straddr << std::endl;
        memcpy(&sa, &addr, sizeof(addr));
        ASSERT_FALSE(is_any_ip(sa));

        inet_pton(AF_INET, "127.0.0.1", &(addr.sin_addr));
        inet_ntop(AF_INET, &addr.sin_addr, straddr, sizeof(straddr));
        std::cout << "loopback IPv4 addr: " << straddr << std::endl;
        memcpy(&sa, &addr, sizeof(addr));
        ASSERT_FALSE(is_any_ip(sa));

        inet_pton(AF_INET, "0.0.0.0", &(addr.sin_addr));
        inet_ntop(AF_INET, &addr.sin_addr, straddr, sizeof(straddr));
        std::cout << "   any IPv4 addr: " << straddr << std::endl;
        memcpy(&sa, &addr, sizeof(addr));
        ASSERT_TRUE(is_any_ip(sa));
    }
    {
        sockaddr_storage sa;
        char straddr[INET6_ADDRSTRLEN];
        memset(straddr, 0, INET6_ADDRSTRLEN);
        struct sockaddr_in6 addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(30303);

        inet_pton(AF_INET6, "2001:720:1500:1::a100", &(addr.sin6_addr));
        inet_ntop(AF_INET6, &addr.sin6_addr, straddr, sizeof(straddr));
        std::cout << "sample IPv6 addr: " << straddr << std::endl;
        memcpy(&sa, &addr, sizeof(addr));
        ASSERT_FALSE(is_any_ip(sa));

        inet_pton(AF_INET6, "::1", &(addr.sin6_addr));
        inet_ntop(AF_INET6, &addr.sin6_addr, straddr, sizeof(straddr));
        std::cout << "loopback IPv6 addr: " << straddr << std::endl;
        memcpy(&sa, &addr, sizeof(addr));
        ASSERT_FALSE(is_any_ip(sa));

        inet_pton(AF_INET6, "::", &(addr.sin6_addr));
        inet_ntop(AF_INET6, &addr.sin6_addr, straddr, sizeof(straddr));
        std::cout << "   any IPv6 addr: " << straddr << std::endl;
        memcpy(&sa, &addr, sizeof(addr));
        ASSERT_TRUE(is_any_ip(sa));
    }

    {
        struct addrinfo hints, *res0 = nullptr;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = PF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;
        hints.ai_flags = AI_PASSIVE;
        int rc = getaddrinfo("google.com", "http", &hints, &res0);
        if (rc)
            fprintf(stderr, "syscall getaddrinfo failed: %s(%d)\n", gai_strerror(rc), rc);
        EXPECT_EQ(rc, 0);

        // converting sockaddr_in >> sockaddr_storage and sockaddr_in6 >> sockaddr_storage test
        int storage_len = 2 * sizeof(sockaddr_storage);
        char pres[storage_len];
        char *p = pres;
        for (int i = 0; i < storage_len; ++i)
            *p++ = i;
        {
            char *p = pres;
            printf("BEFORE:");
            for (int i = 0; i < storage_len; ++i) {
                if (i % 16 == 0) printf("\n[%02d]:", i / 16);
                printf(" 0x%02x", *p++ & 0xff);
            }
            printf("\n");
        }
        for (struct addrinfo *ai = res0; ai != nullptr; ai = ai->ai_next) {
            if (ai->ai_family == AF_INET) {
                sockaddr_in *v4 = (sockaddr_in *) ai->ai_addr;
                v4->sin_port = htons(80);
                *(sockaddr_storage *) pres = *(sockaddr_storage *) v4;
                ASSERT_FALSE(is_any_ip(*(sockaddr_storage *) pres));
            } else if (ai->ai_family == AF_INET6) {
                sockaddr_in6 *v6 = (sockaddr_in6 *) ai->ai_addr;
                v6->sin6_port = htons(80);
                *(sockaddr_storage *) pres = *(sockaddr_storage *) v6;
                ASSERT_FALSE(is_any_ip(*(sockaddr_storage *) pres));
            }
            std::cout << "resolve google.com as " << print_sockaddr(*(sockaddr_storage *) pres) << std::endl;
            {
                char *p = pres;
                printf("AFTER:");
                for (int i = 0; i < storage_len; ++i) {
                    if (i % 16 == 0) printf("\n[%02d]:", i / 16);
                    printf(" 0x%02x", *p++ & 0xff);
                }
                printf("\n");
            }
        }

        // cyclic test
        int storage_size = 100;
        sockaddr_storage res[storage_size];
        for (int i = 0; i < storage_size; ++i) {
            for (struct addrinfo *ai = res0; ai != nullptr; ai = ai->ai_next) {
                if (ai->ai_family == AF_INET) {
                    sockaddr_in *v4 = (sockaddr_in *) ai->ai_addr;
                    v4->sin_port = htons(80);
                    res[i] = *(sockaddr_storage *) v4;
                } else if (ai->ai_family == AF_INET6) {
                    sockaddr_in6 *v6 = (sockaddr_in6 *) ai->ai_addr;
                    v6->sin6_port = htons(80);
                    res[i] = *(sockaddr_storage *) v6;
                }
            }
        }
    }

    sockaddr_storage sa;
    memset(&sa, 0, sizeof(sa));
    int rc = _net.find_interface("router.bittorrent.com", 6881, sa);
    EXPECT_EQ(rc, 0);
    std::cout << " host name: " << sockaddr_storage_to_host_name(sa) << std::endl;
    std::cout << " bind addr: " << print_sockaddr(sa) << std::endl;

    int s1, s2;
    socklen_t addrlen = 0;
    if (sa.ss_family == AF_INET) {
        s1 = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
        s2 = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
        addrlen = sizeof(sockaddr_in);
    } else if (sa.ss_family == AF_INET6) {
        s1 = socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP);
        s2 = socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP);
        addrlen = sizeof(sockaddr_in6);
    }
    ASSERT_TRUE(addrlen);
    ASSERT_TRUE(s1 > 0 && s2 > 0);

    rc = bind(s1, (struct sockaddr *) &sa, addrlen);
    ASSERT_TRUE(rc == 0);

    const char *send_buffer = "Hello, world!";
    size_t len = strlen(send_buffer);
    rc = ::sendto(s2, send_buffer, len, 0, (struct sockaddr *) &sa, addrlen);
    if (rc < 0)
        fprintf(stderr, "syscall sendto failed: %s(%d)\n", strerror(errno), errno);
    std::cout << "   send to: " << print_sockaddr(sa) << " " << std::to_string(rc) << " bytes" << std::endl;
    ASSERT_EQ(rc, len);

    char recv_buffer[16];
    sockaddr_storage sa_from;
    socklen_t from_addrlen = sizeof(sa_from);
    rc = ::recvfrom(s1, recv_buffer, 16, 0, (struct sockaddr *) &sa_from, &from_addrlen);
    if (rc < 0)
        fprintf(stderr, "syscall recvfrom failed: %s(%d)\n", strerror(errno), errno);
    std::cout << " recv from: " << print_sockaddr(sa_from) << " " << std::to_string(rc) << " bytes" << std::endl;
    assert(rc == strlen(send_buffer));
    recv_buffer[rc] = '\0';
    ASSERT_EQ(strcmp(send_buffer, recv_buffer), 0);

    close(s1);
    close(s2);
}

}
#endif

ant::Network::Network(Net_events* a_events)
    : is_break_loop(false)
	, net_thread(nullptr)
	, _events(a_events)
    , _net_error(0)
    , _sock(-1)
{
    net_thread_pipe = {-1, -1};
}

ant::Network::~Network()
{
    if (net_thread)
        stop();

	LOGS(Log::EInfo, Log::ENet, "\n network released \n");
}

int ant::Network::start(sockaddr_storage const& net_interface) noexcept
{
    if (_sock != -1) {
        LOGS(Log::EDebug, Log::ENet, "already started!\n");
        return 0;
    }

    _net_error = 0;

    assert(sizeof(int) == 4);
    if (pipe(reinterpret_cast<int *>(&net_thread_pipe))) {
        _net_error = errno;
        LOG(Log::EError, Log::ENet, "syscall pipe failed: %s(%d)\n", strerror(errno), errno);
        return _net_error;
    }

#ifndef __linux__
    int flags = fcntl(net_thread_pipe.wfd, F_GETFL, 0);
    if (fcntl(net_thread_pipe.wfd, F_SETFL, flags | F_SETNOSIGPIPE)) {
        _net_error = errno;
        LOG(Log::EError, Log::ENet, "syscall fcntl failed: %s(%d)\n", strerror(errno), errno);
        return _net_error;
    }
#endif

    _bind_addr = net_interface;
    net_thread = new std::thread(&Network::thread_proc, this);
    return 0;
}

void ant::Network::do_listen(int socket)
{
	_srt_proxies.insert(socket);
}

void ant::Network::asynch_start()
{
    _sock = socket(_bind_addr.ss_family, SOCK_DGRAM, IPPROTO_UDP);
    if (_sock < 0) {
        _net_error = errno;
        LOG(Log::EError, Log::ENet, "syscall socket failed: %s(%d)\n", strerror(errno), errno)
        return;
    }

	_net_error = set_sock_opt();
	if (_net_error != 0) {
		LOG(Log::EError, Log::ENet, "set_sock_opt failed: %s(%d)\n", strerror(_net_error), _net_error);
		return;
	}

    bind(_bind_addr);
}

int ant::Network::set_sock_opt()
{
	int flags = fcntl(_sock, F_GETFL, 0);
	if (fcntl(_sock, F_SETFL, flags | O_NONBLOCK)) {
		_net_error = errno;
		LOG(Log::EError, Log::ENet, "syscall fcntl failed: %s(%d)\n", strerror(errno), errno);
		return _net_error;
	}

#ifndef __linux__
	if (fcntl(_sock, F_SETFL, flags | F_SETNOSIGPIPE)) {
		_net_error = errno;
		LOG(Log::EError, Log::ENet, "syscall fcntl failed: %s(%d)\n", strerror(errno), errno);
		return _net_error;
	}
#endif

	int opt = 1;
	if (setsockopt(_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
		_net_error = errno;
		LOG(Log::EError, Log::ENet, "syscall setsockopt failed: %s(%d)\n", strerror(errno), errno);
		return _net_error;
	}

#ifdef __linux__
	if (setsockopt(_sock, SOL_IP, IP_RECVERR, &opt, sizeof(opt)) != 0) {
        int net_error = errno;
        LOG(Log::EError, Log::ENet, "syscall setsockopt failed: %s(%d)\n", strerror(errno), errno);
        return net_error;
    }
#endif

	opt = 4096;
	socklen_t opt_size = sizeof(opt);
	::getsockopt(_sock, SOL_SOCKET, SO_RCVBUF, &opt, &opt_size);
	LOG(Log::EInfo, Log::ENet, "socket(%d) RCVBUF is %d bytes\n", _sock, opt)
	in_buf.reserve(opt);

	::getsockopt(_sock, SOL_SOCKET, SO_SNDBUF, &opt, &opt_size);
	LOG(Log::EInfo, Log::ENet, "socket(%d) SNDBUF is %d bytes\n", _sock, opt)

	return 0;
}


/*
 * local port logic
 * Нужна возможность передавать порт сверху игнорируя алгоритмы, это нужно бут и фри нодам для открытия портов на фаерволе
 * Нужно разрешение конфликтов портов. Это нужно для запуска перископа и пирамида одновременно. Двух нод на эмуляторе. Двух ant_test ....
 * Нужно восстанавливать порт при чтении из кеша.
 * Решение конфликта - был пирамид и ушел в фон. Перископ занял его порт. Пирамид вышел в форегроунд и не смог повиснуть на старом порту. Надо чистить кеш и запускаться с нуля.
 * Желательно не скрывать ошибки. Если сверху передают порт и мы не можем на нем повиснуть - выдаем ошибку.
 * bind_port == 0 - means that we need automatically get port
 * returns 0 if success or system error code
*/
int ant::Network::bind(sockaddr_storage& bind_ip_port)
{
	socklen_t addrlen = 0;

	LOG(Log::EInfo, Log::ENet, "libant(%d) try bind to %s\n", _sock, print_sockaddr(bind_ip_port).c_str());

	if (bind_ip_port.ss_family == AF_INET) {
		LOG(Log::EInfo, Log::ENet, "IPv4 network detected\n")
		addrlen =  sizeof(sockaddr_in);
	} else if (bind_ip_port.ss_family == AF_INET6) {
		LOG(Log::EInfo, Log::ENet, "IPv6 network detected\n")
		addrlen =  sizeof(sockaddr_in6);
	}

	std::mt19937 gen(time(0));
	std::uniform_int_distribution<> uid(6000, 8999);
	int new_port = 	uid(gen);

	int attempts = 0;
	int max_attempts;

    // high level logic requires exactly this port (for firewall rules ....)
    max_attempts = 1;

	while (++attempts <= max_attempts) {
        _net_error = 0;
		int res = ::bind(_sock, (sockaddr *) &bind_ip_port, addrlen);
		_net_error = res==0 ? 0 : errno;

		if (_net_error == 0) {
			if (getsockname(_sock, (struct sockaddr *) &_bind_addr, &addrlen)) {
				LOG(Log::EError, Log::ENet, "syscall getsockname failed: %s(%d)\n", strerror(errno), errno);
				return 1;
			}

			LOG(Log::EInfo, Log::ENet, "libant(%d) bound to local %s\n", _sock, print_sockaddr(_bind_addr).c_str());
			return 0;
		}

		if (_net_error == EADDRINUSE) {
			LOG(Log::EWarning, Log::ENet, "attempt %d(%d), syscall bind(%s) failed: %s(%d)\n",
                attempts, max_attempts, print_sockaddr(bind_ip_port).c_str(), strerror(_net_error), _net_error);
			set_port(bind_ip_port, ++new_port);
			continue;
		}

		LOG(Log::EWarning, Log::ENet, "syscall bind(%s) failed: %s(%d)\n",
            print_sockaddr(bind_ip_port).c_str(), strerror(_net_error), _net_error);
		return _net_error;
	}

    _net_error = EADDRINUSE;
	return _net_error;
}

void ant::Network::do_asynch(std::function<void()> f) noexcept
{
	if (is_break_loop)
		return;
    net_queue.push(f);
	ssize_t	rc = write(net_thread_pipe.wfd, "f", 1);
	// On error, -1 is returned, and errno is set appropriately.
	if (rc == -1)
		LOG(Log::EError, Log::ENet, "syscall write to pipe failed: %s(%d)\n", strerror(errno), errno);
}

void ant::Network::stop() noexcept
{
	is_break_loop = true;
    if (net_thread && net_thread->joinable()) {
        if (net_thread_pipe.wfd != -1)
            write(net_thread_pipe.wfd, "s", 1);
        net_thread->join();
        delete net_thread;
        net_thread = nullptr;
    }
}

int ant::Network::find_interface(std::string target_fqdn, uint16_t target_port, sockaddr_storage& src_addr)
{
    int error = 0;
    ifaddrs *ifaces = nullptr;
    if (getifaddrs(&ifaces)) {
        error = errno;
        LOG(Log::EError, Log::ENet, "syscall getifaddrs failed: %s(%d)\n", strerror(errno), errno);
        return error;
    }
    for (ifaddrs *iface = ifaces; iface; iface = iface->ifa_next) {
        if (!(iface->ifa_addr->sa_family == AF_INET || iface->ifa_addr->sa_family == AF_INET6))
            continue;
        std::string addr = sockaddr_storage_to_host_name(*(struct sockaddr_storage const*)(iface->ifa_addr));
        LOG(Log::EInfo, Log::ENet, "%s%s: %s\n",
            iface->ifa_addr->sa_family==AF_INET ? "ipv4/" : "ipv6/", iface->ifa_name, addr.c_str());
    }
    
    int test_sock;

    struct addrinfo hints, *res0 = nullptr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_PASSIVE;

    Chronometer<std::chrono::milliseconds> time_meter;
    error = getaddrinfo(target_fqdn.c_str(), std::to_string(target_port).c_str(), &hints, &res0);
    time_meter.stop();
    LOG(Log::EInfo, Log::ENet, "resolve \"%s\" for %u ms\n", target_fqdn.c_str(), time_meter.count());
    if (error) {
        LOG(Log::EError, Log::ENet, "find_interface: syscall getaddrinfo failed: %s(%d) %s\n",
            gai_strerror(error), error, target_fqdn.c_str());
        return error;
    }

    do {
        test_sock = socket(res0->ai_family, SOCK_DGRAM, IPPROTO_UDP);
        if (test_sock < 0) {
            error = errno;
            LOG(Log::EError, Log::ENet, "syscall socket failed: %s\n", strerror(errno), errno);
            break;
        }

        if (connect(test_sock, res0->ai_addr, res0->ai_addrlen)) {
            error = errno;
            LOG(Log::EError, Log::ENet, "syscall connect failed: %s(%d)\n", strerror(errno), errno);
            break;
        }

        socklen_t len = 0;
        if (res0->ai_family == AF_INET) {
            len = sizeof(sockaddr_in);
        } else if (res0->ai_family == AF_INET6) {
            len = sizeof(sockaddr_in6);
        }

        if (getsockname(test_sock, (struct sockaddr *) &src_addr, &len)) {
            error = errno;
            LOG(Log::EError, Log::ENet, "syscall getsockname failed: %s(%d)\n", strerror(errno), errno);
            break;
        }
    } while (false);

    freeaddrinfo(res0);
    close(test_sock);

    return error;
}

bool ant::Network::resolve_to_ipv4(std::string target_fqdn, unsigned short target_port, sockaddr_storage& ipv4_addr)
{
	std::mt19937 gen(time(0));

	std::vector<sockaddr_storage> list_peers;

	struct addrinfo hints, *res0 = nullptr;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC; // AF_INET
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_PASSIVE;  // Если node не равно NULL, то флаг AI_PASSIVE игнорируется.

	Chronometer<std::chrono::milliseconds> time_meter;
	int error = getaddrinfo(target_fqdn.c_str(), 0, &hints, &res0);
	time_meter.stop();
	LOG(Log::EDebug, Log::ENet, "resolve \"%s\" for %u ms\n", target_fqdn.c_str(), time_meter.count());

	if (error) {
		LOG(Log::EError, Log::ENet, "resolve_to_ipv4: syscall getaddrinfo failed: %s(%d) fqdn:%s\n",
		    gai_strerror(error), error, target_fqdn.c_str());
		return false;
	}
	for (struct addrinfo *i = res0; i != nullptr; i = i->ai_next) {
		if (i->ai_family == AF_INET) {
			sockaddr_in *v4 = (sockaddr_in *) i->ai_addr;
			v4->sin_port = htons(target_port);
			sockaddr_storage ipv4_peer;
			ipv4_peer = *((sockaddr_storage*)v4);
			list_peers.push_back(ipv4_peer);
            LOG(Log::EInfo, Log::EAnt, "resolving ipv4 %s:%d candidate %s\n"
                , target_fqdn.c_str()
                , target_port
                , print_sockaddr(ipv4_peer).c_str());
		}
	}

    if (list_peers.size()==0) {
        LOG(Log::EWarning, Log::EAnt, "resolving ipv4 ip failed! %s:%d\n", target_fqdn.c_str(), target_port);
        return false;
    }
    
	std::uniform_int_distribution<> uid(0, list_peers.size()-1);
	ipv4_addr = list_peers[uid(gen)];

	LOG(Log::EDebug, Log::EAnt, "resolving ip %s -> %s\n",
		target_fqdn.c_str(), print_sockaddr(ipv4_addr).c_str());

	return true;
}

int ant::Network::check_srt(fd_set const& sd_set)
{
	for(const auto &srt_proxy_socket: _srt_proxies) {
		if(FD_ISSET(srt_proxy_socket, &sd_set))
			return srt_proxy_socket;
	}
	return 0;
}

void ant::Network::thread_proc()
{
    fd_set rd_fds, err_fds;
    std::chrono::steady_clock::time_point last_now = std::chrono::steady_clock::now();

    LOGS(Log::EInfo, Log::ENet, "NET thread is running\n")

	asynch_start();

	if (_events) {
        if (!_net_error)
            _events->network_start();
        else
            _events->on_network_lost();
    }

	auto start_handling_time = std::chrono::steady_clock::now();

    while (!is_break_loop) {
        FD_ZERO(&rd_fds);
        FD_ZERO(&err_fds);
        
        FD_SET(net_thread_pipe.rfd, &rd_fds);
        int nfds = net_thread_pipe.rfd;
        if (_sock > 0) {
            FD_SET(_sock, &rd_fds);
            FD_SET(_sock, &err_fds);
            nfds = std::max(nfds, _sock);
        }
        for(const auto &srt_proxy_socket: _srt_proxies) {
			FD_SET(srt_proxy_socket, &rd_fds);
			FD_SET(srt_proxy_socket, &err_fds);
			nfds = std::max(nfds, srt_proxy_socket);
		}

		//auto stop_handling_time = std::chrono::steady_clock::now();
		//int select_handling = std::chrono::duration_cast<std::chrono::milliseconds>(stop_handling_time - start_handling_time).count();
		//LOG(Log::EDebug, Log::ENet, "handling select delay: %d ms\n", select_handling);
        struct timeval tv = { .tv_sec = 0, .tv_usec = 250000 };
        int rc = select(nfds+1, &rd_fds, nullptr, &err_fds, &tv);
        start_handling_time = std::chrono::steady_clock::now();
        int delay = std::chrono::duration_cast<std::chrono::milliseconds>(start_handling_time - last_now).count();

		if (is_break_loop)
			continue;

        if (rc > 0) {
			LOG(Log::EDebug, Log::ENet, "handling read\n")

			int srt_sock = check_srt(rd_fds);
			if (srt_sock) {
				LOGS(Log::EDebug, Log::ENet, "SRT socket read\n")
			}

			if (_sock > 0 && FD_ISSET(_sock, &rd_fds)) {
				LOGS(Log::EDebug, Log::ENet, "socket read\n")
			}

            if (_sock > 0 && FD_ISSET(_sock, &err_fds)) {
                LOGS(Log::EError, Log::ENet, "error on socket\n")
                if (_events)
                	_events->handle_icmp(_sock);
            }

			if (FD_ISSET(net_thread_pipe.rfd, &rd_fds)) {
				LOGS(Log::EDebug, Log::ENet, "command read\n")
				char cmd[255]; ::read(net_thread_pipe.rfd, &cmd, sizeof(cmd)); // clear pipe
				to_call_async_commands();
			}

        } else if (rc < 0) { // select returns -1
            _net_error = errno;

            LOG(Log::EError, Log::ENet, "syscall select failed: %s(%d)\n", strerror(errno), errno)
            LOGS(Log::EError, Log::ENet, "network failed and requires a restart!\n")

            close(_sock);
			_sock = -1;
            if (_events)
            	_events->on_network_lost();
        }

        if (delay >= 500) {
            last_now = start_handling_time;

            if (_events) {
				LOGS(Log::EDebug, Log::ENet, "tick\n")
				_events->tick();
			}
        }

    } //  while(!is_break_loop)

    if (_events)
        _events->network_stop();

	if (_sock > 0)
		close(_sock);
	_sock = 0;

	close(net_thread_pipe.wfd);
    close(net_thread_pipe.rfd);
    net_thread_pipe = {-1, -1};

	net_queue.clear(); // queue parameters can hold smart pointers
	_events = nullptr;

    LOGS(Log::EInfo, Log::ENet, "thread stopped\n")
}


void ant::Network::to_call_async_commands()
{
	std::function<void()> f;
	while (!is_break_loop && net_queue.try_pop(f)) {
		try {
			f();
		} catch (std::exception const &e) {
			LOG(Log::EError, Log::ENet, "catch exception into function call: %s\n", e.what());
		}
	}
}
