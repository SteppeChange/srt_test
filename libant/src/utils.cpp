
#include "utils.hpp"
#include <netdb.h>
#include <arpa/inet.h>


namespace ant {

#ifndef SHA1_DIGESTSIZE
#define SHA1_DIGESTSIZE 20
#endif



    byte hex_to_byte(unsigned char c) {
        byte val = 0;
        if ('0' <= c && c <= '9')
            val = c - '0';
        else if ('A' <= c && c <= 'F')
            val = 10 + (c - 'A');
        else
            assert(false);
        return val;
    }

	std::string print_sockaddr(const sockaddr_storage& sa)
    {
        char address[255]; // we need INET6_ADDRSTRLEN+2+1+5=54 bytes
        memset(&address, 0, sizeof(address));
        int port = 0;
        size_t end = 0;
        if (sa.ss_family == AF_INET6) {
            address[0]='[';
            const char * res = inet_ntop( AF_INET6, &(((struct sockaddr_in6 const *)&sa)->sin6_addr), address+1, sizeof(address)-1 );
            if(res==0) return "INADDR_NONE";
            address[strlen(address)]=']';
            port = ((struct sockaddr_in6 const *)&sa)->sin6_port;
        }
        if (sa.ss_family == AF_INET) {
            const char * res = inet_ntop( AF_INET, &(((struct sockaddr_in const *)&sa)->sin_addr), address, sizeof(address) );
            if(res==0) return "INADDR_NONE";
            port = ((struct sockaddr_in const *)&sa)->sin_port;
        }
        
        end = strlen(address);
        snprintf(address + end, sizeof(address) - end, ":%u", ntohs(port));
        return address;
    }
    
	std::string sockaddr_storage_to_host_name(const sockaddr_storage& sa)
	{
        if (sa.ss_family == AF_INET6) {
            char buffer[INET6_ADDRSTRLEN];
            int error = getnameinfo((struct sockaddr const*)&sa, sizeof(sockaddr_in6),buffer, sizeof(buffer), 0,0, NI_NUMERICHOST);
            if(error==0)
                return buffer;
        }

        if (sa.ss_family == AF_INET) {
            char buffer[INET_ADDRSTRLEN];
            int error = getnameinfo((struct sockaddr const*)&sa, sizeof(sockaddr_in),buffer, sizeof(buffer), 0,0, NI_NUMERICHOST);
            if(error==0)
                return buffer;
        }

        return "";
	}

	bool sock_addr_cmp(const struct sockaddr_storage * sa, const struct sockaddr_storage * sb) {
		return 0 == sock_addr_cmp_addr(sa, sb) && 0 == sock_addr_cmp_port(sa, sb);
	}

	bool sock_addr_less(const struct sockaddr_storage * sa, const struct sockaddr_storage * sb) {
		int res = sock_addr_cmp_addr(sa, sb);
		if(res<0)
			return true;
		if(res>0)
			return false;
		// res == 0
		res = sock_addr_cmp_port(sa, sb);
		if(res<0)
			return true;
		return false;
	}

	int sock_addr_cmp_addr(const struct sockaddr_storage * sa, const struct sockaddr_storage * sb)
	{
		if (sa->ss_family != sb->ss_family) {
			__uint8_t left = sa->ss_family;
			__uint8_t right = sb->ss_family;
			if(left<right)
				return -1;
			if(left==right)
				return 0;
			if(left>right)
				return 1;
		}

		if (sa->ss_family == AF_INET) {
			__uint32_t left = SOCK_ADDR_IN_ADDR(sa).s_addr;
			__uint32_t right = SOCK_ADDR_IN_ADDR(sb).s_addr;
			if(left<right)
				return -1;
			if(left==right)
				return 0;
			if(left>right)
				return 1;
		}
		else if (sa->ss_family == AF_INET6) {
			// returning zero if they all match or a value different from zero representing which is greater if they do not.
			// its not less but its good too
			return memcmp((char *) &(SOCK_ADDR_IN6_ADDR(sa)),
						   (char *) &(SOCK_ADDR_IN6_ADDR(sb)),
						   sizeof(SOCK_ADDR_IN6_ADDR(sa)));
		}

		return -1;
	}

	int sock_addr_cmp_port(const struct sockaddr_storage * sa, const struct sockaddr_storage * sb)
	{
		if (sa->ss_family != sb->ss_family)
			return sa->ss_family < sb->ss_family;

		if (sa->ss_family == AF_INET) {
			__uint32_t left = SOCK_ADDR_IN_PORT(sa);
			__uint32_t right = SOCK_ADDR_IN_PORT(sb);

			if (left < right)
				return -1;
			if (left == right)
				return 0;
			if (left > right)
				return 1;
		}
		else if (sa->ss_family == AF_INET6) {

			__uint32_t left = SOCK_ADDR_IN6_PORT(sa);
			__uint32_t right = SOCK_ADDR_IN6_PORT(sb);

			if (left < right)
				return -1;
			if (left == right)
				return 0;
			if (left > right)
				return 1;
		}

		return -1;
	}


	sockaddr_storage resolve_addr(std::string const& dest_ip, uint16_t dest_port)
	{
		int error = 0;
		sockaddr_storage destination;
		memset(&destination, 0, sizeof(destination));

		// initialize destination
		struct addrinfo hints, *res0 = nullptr;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = PF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;
		hints.ai_flags = AI_PASSIVE;
//		Chronometer<std::chrono::milliseconds> time_meter;
		error = getaddrinfo(dest_ip.c_str(), std::to_string(dest_port).c_str(), &hints, &res0);
//		time_meter.stop();
//		LOG(Log::EInfo, Log::ENet, "resolve \"%s\" for %u ms\n", dest_ip.c_str(), time_meter.count());
		if (error) {
//			LOG(Log::EError, Log::ENet, "tunnel: : syscall getaddrinfo failed: %s(%d) %s\n", gai_strerror(error), error, dest_ip.c_str());
			RUNTIME_ERROR("tunnel: : syscall getaddrinfo: %s\n", gai_strerror(error))
		}
		memcpy(&destination, res0->ai_addr, res0->ai_addrlen);

		freeaddrinfo(res0);

		return destination;
	}

}
