#pragma once
#include "ItemsMng.h"
#include <vmsys.h>
#include <memory>
#include <thread>
#include <asio/ip/tcp.hpp>
#include <asio/ip/udp.hpp>

#include <vmsock_fix.h>

class App;

namespace MREngine {
    class TcpSock : public std::enable_shared_from_this<TcpSock> {
        asio::ip::tcp::socket socket;
        asio::ip::tcp::resolver resolver;
        int phandle;
        vm_sock_callback_t callback;
        bool is_read_waiting = false;
        bool is_write_waiting = false;

        void push_event(int event);
    public:
        int handle;
        TcpSock(asio::io_context& ctx, vm_sock_callback_t c, int phandle);

        void connect(const std::string& host, const std::string& port);

        int read(void* buffer, size_t length);
        int write(void* buffer, size_t length);

        void wait_for_read();
        void wait_for_write();

        void close();
    };

    class UdpSock : public std::enable_shared_from_this<UdpSock> {
        asio::ip::udp::socket socket;
        int phandle;
        vm_sock_callback_t callback;
        bool is_read_waiting = false;
        bool is_write_waiting = false;

        void push_event(int event);
    public:
        int handle;
        UdpSock(asio::io_context& ctx, vm_sock_callback_t c, int phandle);

        bool create(int port, int loopback);

        int sendto(const void* buf, size_t len, const vm_sockaddr_struct* addr);
        int recvfrom(void* buf, size_t len, vm_sockaddr_struct* addr);

        void wait_for_read();
        void wait_for_write();

        void close();
    };

    class AppSock {
    public:
        ItemsMng<std::shared_ptr<TcpSock>> tcps;
        ItemsMng<std::shared_ptr<UdpSock>> udps;
    };

    class NetworkManager {
        asio::io_context io_ctx;

        asio::executor_work_guard<asio::io_context::executor_type> work_guard;

        std::thread asio_thread;

    public:
        NetworkManager();

        ~NetworkManager();

        asio::io_context& get_context();
    };
}

MREngine::AppSock& get_current_app_sock();


struct sock_event { //tcp and udp
    vm_sock_callback_t callback;
    int phandle;
    int handle;
    int event;
};

struct dns_event {
    vm_dns_callback_t callback;
    int phandle;
    vm_soc_dns_result result;
};
