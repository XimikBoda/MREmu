#pragma once
#include "ItemsMng.h"
#include <SFML/Network/TcpSocket.hpp>
#include <memory>
#include <vmsys.h>

#define SOCK_TMP_BUF_SIZE 1024

namespace MREngine {
    struct tcp_el {
        std::shared_ptr<sf::TcpSocket> soc;
        uint32_t callback;
        bool is_connected = false;
        bool is_disconnected = false;
        uint8_t receive_tmp_buf[SOCK_TMP_BUF_SIZE];
        size_t receive_tmp_buf_pos = 0;
    };

    class AppSock {
    public:
        ItemsMng<tcp_el> tcps;
        void update();
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
	VMINT error_cause; /* vm_ps_cause_enum */
}vm_soc_dns_result;

#define VM_TCP_EVT_CONNECTED	1
#define VM_TCP_EVT_CAN_WRITE	2
#define VM_TCP_EVT_CAN_READ		3
#define VM_TCP_EVT_PIPE_BROKEN	4
#define VM_TCP_EVT_HOST_NOT_FOUND	5
#define VM_TCP_EVT_PIPE_CLOSED	6

extern "C" {
	VMINT vm_is_support_wifi(void); // Becouse we have some problem with vmsock.h
	VMINT vm_wifi_is_connected(void);


	VMINT vm_soc_get_host_by_name(VMINT apn,
		const VMCHAR* host,
		vm_soc_dns_result* result,
		VMINT(*callback)(vm_soc_dns_result*));

    VMINT vm_tcp_connect(const char* host, const VMINT port, const VMINT apn,
        void (*callback)(VMINT handle, VMINT event));

    void vm_tcp_close(VMINT handle);

    VMINT vm_tcp_read(VMINT handle, void* buf, VMINT len);

    VMINT vm_tcp_write(VMINT handle, void* buf, VMINT len);
}