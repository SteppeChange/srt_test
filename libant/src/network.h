#pragma once

#include <vector>
#include <utility>
#include <cstdint>
#include <thread>
#include <sys/socket.h>
#include "multithread_queue.h"
#include <functional>
#include <set>
#include <srt.h>

namespace ant {

    enum Protocol {
        EUndefined = 0,
        EKademlia = 1,
        EUtp = 2,
		ESrt = 3,
        EUnrecognized = 4
    };

    class Udp_sender
    {
    public:
        typedef std::shared_ptr<Udp_sender> ptr;
        virtual sockaddr_storage const& getbindaddr() const = 0;
    };

	class Net_events
	{
	public:
		virtual void on_network_lost() = 0;
		virtual void tick() = 0;
		virtual void network_start() = 0;
		virtual void network_stop() = 0;
		virtual void recvfrom(Protocol proto, const uint8_t *buffer, size_t buffer_len, const sockaddr_storage *from, socklen_t addr_len, int recv_socket, int ant_socket) = 0;
		virtual void on_network_error(const sockaddr_storage *from, int errcode) = 0;
		virtual void handle_icmp(int sock) = 0;
	};

    class Network : public Udp_sender
    {
    public:
        typedef std::shared_ptr<Network> ptr;

        Network(Net_events* an_events);
        virtual ~Network();

        int start(sockaddr_storage const& net_interface) noexcept;
        void stop() noexcept;

        void do_asynch(std::function<void()> f) noexcept;

        sockaddr_storage const& getbindaddr() const override {
            return _bind_addr;
		}
        // it finds appropriate local interface for target interaction
        static int find_interface(std::string target_fqdn, uint16_t target_port, sockaddr_storage& local_addr);
		static bool resolve_to_ipv4(std::string target_fqdn, uint16_t target_port, sockaddr_storage& ipv4_addr);

		void listen(int socket) {
			do_asynch(std::bind(&ant::Network::do_listen, this, socket));
		}

    protected:
		void do_listen(int socket);
		void asynch_start();
        void thread_proc();
		int check_srt(fd_set const& sd_set);
        // return 0 if success or system error code
        int bind(sockaddr_storage& bind_ip_port);
		int set_sock_opt();

	protected:
        bool is_break_loop;
        multithread_queue<std::function<void()>> net_queue;
        std::thread *net_thread;
		Net_events* _events;
        int _net_error;

	private:
		void to_call_async_commands();

    private:
        // pipe is used for internal communications with loop thread
        struct Pipe {
            int rfd;
            int wfd;
        } __attribute__ ((aligned (4))) net_thread_pipe;

        // external IP socket
        int _sock;
        // external IP address
        sockaddr_storage _bind_addr;
        // receiving buffer
        std::vector<uint8_t> in_buf;
		//
        std::set<int> _srt_proxies;
    };

} // namespace ant
