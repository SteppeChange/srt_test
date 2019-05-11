#pragma once

#include <stdexcept>
#include <cstddef>
#include <cassert>
#include <chrono>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstring>

#define SOCK_ADDR_IN_PTR(sa)	((struct sockaddr_in *)(sa))
#define SOCK_ADDR_IN_FAMILY(sa)	SOCK_ADDR_IN_PTR(sa)->sin_family
#define SOCK_ADDR_IN_PORT(sa)	SOCK_ADDR_IN_PTR(sa)->sin_port
#define SOCK_ADDR_IN_ADDR(sa)	SOCK_ADDR_IN_PTR(sa)->sin_addr

#define SOCK_ADDR_IN6_PTR(sa)	((struct sockaddr_in6 *)(sa))
#define SOCK_ADDR_IN6_FAMILY(sa) SOCK_ADDR_IN6_PTR(sa)->sin6_family
#define SOCK_ADDR_IN6_PORT(sa)	SOCK_ADDR_IN6_PTR(sa)->sin6_port
#define SOCK_ADDR_IN6_ADDR(sa)	SOCK_ADDR_IN6_PTR(sa)->sin6_addr

#ifndef __linux__
#define IN6ADDR_ANY_INIT \
	{{{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }}}
#endif

namespace ant {

    static in6_addr _in6addr_any = IN6ADDR_ANY_INIT;

	typedef unsigned char byte;

	byte hex_to_byte(unsigned char c);

	std::string print_sockaddr(const sockaddr_storage &sa);

	std::string sockaddr_storage_to_host_name(const sockaddr_storage &sa);

	bool sock_addr_cmp(const struct sockaddr_storage * sa, const struct sockaddr_storage * sb);
	bool sock_addr_less(const struct sockaddr_storage * sa, const struct sockaddr_storage * sb);
	// 0 equal, - less , + great
	int sock_addr_cmp_addr(const struct sockaddr_storage * sa, const struct sockaddr_storage * sb);
	int sock_addr_cmp_port(const struct sockaddr_storage * sa, const struct sockaddr_storage * sb);
	inline bool empty(const struct sockaddr_storage * s) {
		return s->ss_family == AF_UNSPEC;
	}

	struct sockaddr_storage_comparator {
		// operator less
		bool operator()(sockaddr_storage const& a, sockaddr_storage const& b) const {
			return sock_addr_less(&a, &b);
		}
	};


	inline void set_port(sockaddr_storage& addr, int port)
	{
		if (addr.ss_family == AF_INET) {
			SOCK_ADDR_IN_PORT(&addr) = htons(port);
		} else if (addr.ss_family == AF_INET6) {
			SOCK_ADDR_IN6_PORT(&addr) = htons(port);
		}
	}

    inline int get_port(sockaddr_storage& addr)
    {
        if (addr.ss_family == AF_INET) {
            return ntohs(SOCK_ADDR_IN_PORT(&addr));
        } else if (addr.ss_family == AF_INET6) {
            return ntohs(SOCK_ADDR_IN6_PORT(&addr));
        } else return 0;
    }

    inline sockaddr_storage invalid_host()
    {
        sockaddr_storage empty_socket;
        struct sockaddr_in sin;
        memset (&sin, 0, sizeof (sin));
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = inet_addr ("0.0.0.0");
        memset(&empty_socket, 0, sizeof(empty_socket));
        memcpy(&empty_socket, &sin, sizeof (sin));
        return empty_socket;
    }

    inline bool is_invalid_host(sockaddr_storage const& addr)
    {
        __uint32_t right = SOCK_ADDR_IN_ADDR(&addr).s_addr;
        return addr.ss_family == AF_INET && SOCK_ADDR_IN_PORT(&addr) == 0 && right == 0;
    }

    inline bool is_any_ip(sockaddr_storage const& addr)
    {
        if(addr.ss_family == AF_INET)
            return SOCK_ADDR_IN_ADDR(&addr).s_addr == INADDR_ANY;
        if(addr.ss_family == AF_INET6)
            return !std::memcmp(&_in6addr_any, (char const*) &(SOCK_ADDR_IN6_ADDR(&addr)), sizeof(in6_addr));
        return false;
    }

    sockaddr_storage resolve_addr(std::string const& dest_ip, uint16_t dest_port);

#define RUNTIME_ERROR(f_, ...)  {\
    char s[1024];\
    snprintf(s, 1024, (f_), __VA_ARGS__); \
    throw std::runtime_error(s); }

	template <typename Resolution>
	class Chronometer {
		std::chrono::steady_clock::time_point t1;
		std::chrono::steady_clock::time_point t2;

	public:
		Chronometer() {
			reset();
		}
		inline void stop() {
			t2 = std::chrono::steady_clock::now();
		}
		inline unsigned count() {
			return std::chrono::duration_cast<Resolution>(t2 - t1).count();
		}
		inline void reset() {
			t1 = std::chrono::steady_clock::now();
			t2 = t1;
		}
	};

}
