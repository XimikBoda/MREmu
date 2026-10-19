#pragma once
#include "ItemsMng.h"
#include <SFML/Network/TcpSocket.hpp>
#include <memory>
#include <vmsys.h>
#include <asio.hpp>

class App;

#define VM_MAX_SOCK_ADDR_LEN 28

typedef struct
{
    VMINT16	addr_len; /* address length */
    VMUINT16	port; /* port number */
    VMUINT8	addr[VM_MAX_SOCK_ADDR_LEN];
} vm_sockaddr_struct;

typedef struct
{
    VMINT       apn_info_id;
    VMINT       proxy_port;
    VMCHAR      operator_id[8];
    VMCHAR      operator_name[64];
    VMCHAR      proxy_ip[16];
    VMCHAR      apn_proxy[32];
    VMCHAR      apn_no_proxy[32];
    VMCHAR      user_name[16];
    VMCHAR      pass_word[16];
} vm_apn_info_ext;

namespace MREngine {
    class TcpSock : public std::enable_shared_from_this<TcpSock> {
        asio::ip::tcp::socket socket;
        asio::ip::tcp::resolver resolver;
        int phandle;
        void (*callback)(VMINT handle, VMINT event);
        bool is_read_waiting = false;
        bool is_write_waiting = false;

        void push_event(int event);
    public:
        int handle;
        TcpSock(asio::io_context& ctx, void (*c)(int, int), int phandle);

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
        void (*callback)(VMINT handle, VMINT event);
        bool is_read_waiting = false;
        bool is_write_waiting = false;

        void push_event(int event);
    public:
        int handle;
        UdpSock(asio::io_context& ctx, void (*c)(int, int), int phandle);

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

#define VM_SOC_DNS_MAX_ADDR 5

typedef enum
{
    VM_E_SOC_SUCCESS = 0,     /* success */
    VM_E_SOC_ERROR = -1,    /* error */
    VM_E_SOC_WOULDBLOCK = -2,    /* not done yet */
    VM_E_SOC_LIMIT_RESOURCE = -3,    /* limited resource */
    VM_E_SOC_INVALID_SOCKET = -4,    /* invalid socket */
    VM_E_SOC_INVALID_ACCOUNT = -5,    /* invalid account id */
    VM_E_SOC_NAMETOOLONG = -6,    /* address too long */
    VM_E_SOC_ALREADY = -7,    /* operation already in progress */
    VM_E_SOC_OPNOTSUPP = -8,    /* operation not support */
    VM_E_SOC_CONNABORTED = -9,    /* Software caused connection abort */
    VM_E_SOC_INVAL = -10,   /* invalid argument */
    VM_E_SOC_PIPE = -11,   /* broken pipe */
    VM_E_SOC_NOTCONN = -12,   /* socket is not connected */
    VM_E_SOC_MSGSIZE = -13,   /* msg is too long */
    VM_E_SOC_BEARER_FAIL = -14,   /* bearer is broken */
    VM_E_SOC_CONNRESET = -15,   /* TCP half-write close, i.e., FINED */
    VM_E_SOC_DHCP_ERROR = -16,   /* DHCP error */
    VM_E_SOC_IP_CHANGED = -17,   /* IP has changed */
    VM_E_SOC_ADDRINUSE = -18,   /* address already in use */
    VM_E_SOC_CANCEL_ACT_BEARER = -19    /* cancel the activation of bearer */
} vm_soc_error_enum;

typedef struct
{
	VMUINT address[VM_SOC_DNS_MAX_ADDR];
	VMINT num;
	VMINT error_cause;
}vm_soc_dns_result;

#define VM_TCP_EVT_CONNECTED	1
#define VM_TCP_EVT_CAN_WRITE	2
#define VM_TCP_EVT_CAN_READ		3
#define VM_TCP_EVT_PIPE_BROKEN	4
#define VM_TCP_EVT_HOST_NOT_FOUND	5
#define VM_TCP_EVT_PIPE_CLOSED	6

#define VM_UDP_EVT_WRITE	2
#define VM_UDP_EVT_READ		3
#define VM_UDP_EVT_PIPE_BROKEN	4
#define VM_UDP_EVT_PIPE_CLOSED	6

extern "C" {
	VMINT vm_is_support_wifi(void); // Becouse we have some problem with vmsock.h
	VMINT vm_wifi_is_connected(void);

	VMINT vm_soc_get_host_by_name(VMINT apn, const VMCHAR* host, vm_soc_dns_result* result, VMINT(*callback)(vm_soc_dns_result*));

    VMINT vm_tcp_connect(const char* host, const VMINT port, const VMINT apn, void (*callback)(VMINT handle, VMINT event));
    void vm_tcp_close(VMINT handle);
    VMINT vm_tcp_read(VMINT handle, void* buf, VMINT len);
    VMINT vm_tcp_write(VMINT handle, void* buf, VMINT len);

    VMINT vm_udp_create(VMINT port, VMINT apn, void (*callback)(VMINT hdl, VMINT event), VMINT loopback);
    VMINT vm_udp_sendto(VMINT hdl, const void* buf, VMINT32 len, const vm_sockaddr_struct* addr);
    VMINT vm_udp_recvfrom(VMINT hdl, void* buf, VMINT32 len, vm_sockaddr_struct* addr);
    VMINT vm_udp_close(VMINT hdl);
    VMINT vm_get_default_apn_info(vm_apn_info_ext* apn_info);
    VMINT vm_get_apn_info(vm_apn_info_ext* current_apn_info);
}

struct sock_event { //tcp and udp
    void (*callback)(VMINT handle, VMINT event);
    int phandle;
    int handle;
    int event;
};

struct dns_event {
    VMINT(*callback)(vm_soc_dns_result*);
    int phandle;
    vm_soc_dns_result result;
};
