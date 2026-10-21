// This file created becouse we have some problem with vmsock.h
#pragma once 
#include "vmsys.h"

#ifdef __cplusplus
extern "C" {
#endif

// General

typedef enum
{
    VM_E_SOC_SUCCESS            = 0,
    VM_E_SOC_ERROR              = -1,
    VM_E_SOC_WOULDBLOCK         = -2,
    VM_E_SOC_LIMIT_RESOURCE     = -3,
    VM_E_SOC_INVALID_SOCKET     = -4,
    VM_E_SOC_INVALID_ACCOUNT    = -5,
    VM_E_SOC_NAMETOOLONG        = -6,
    VM_E_SOC_ALREADY            = -7,
    VM_E_SOC_OPNOTSUPP          = -8,
    VM_E_SOC_CONNABORTED        = -9,
    VM_E_SOC_INVAL              = -10,
    VM_E_SOC_PIPE               = -11,
    VM_E_SOC_NOTCONN            = -12,
    VM_E_SOC_MSGSIZE            = -13,
    VM_E_SOC_BEARER_FAIL        = -14,
    VM_E_SOC_CONNRESET          = -15,
    VM_E_SOC_DHCP_ERROR         = -16,
    VM_E_SOC_IP_CHANGED         = -17,
    VM_E_SOC_ADDRINUSE          = -18,
    VM_E_SOC_CANCEL_ACT_BEARER  = -19 
} vm_soc_error_enum;


typedef void (*vm_sock_callback_t)(VMINT handle, VMINT event);

// TCP

#define VM_TCP_EVT_CONNECTED        1
#define VM_TCP_EVT_CAN_WRITE	    2
#define VM_TCP_EVT_CAN_READ		    3
#define VM_TCP_EVT_PIPE_BROKEN	    4
#define VM_TCP_EVT_HOST_NOT_FOUND	5
#define VM_TCP_EVT_PIPE_CLOSED	    6

#define VM_TCP_READ_EOF				(-1)


VMINT vm_tcp_connect(const char* host, const VMINT port, const VMINT apn, vm_sock_callback_t callback);
void vm_tcp_close(VMINT handle);
VMINT vm_tcp_read(VMINT handle, void* buf, VMINT len);
VMINT vm_tcp_write(VMINT handle, void* buf, VMINT len);

//UDP

#define VM_UDP_EVT_WRITE	    2
#define VM_UDP_EVT_READ		    3
#define VM_UDP_EVT_PIPE_BROKEN	4
#define VM_UDP_EVT_PIPE_CLOSED	6

#define VM_MAX_SOCK_ADDR_LEN    28

typedef struct
{
    VMINT16	addr_len;
    VMUINT16	port;
    VMUINT8	addr[VM_MAX_SOCK_ADDR_LEN];
} vm_sockaddr_struct;

VMINT vm_udp_create(VMINT port, VMINT apn, vm_sock_callback_t callback, VMINT loopback);
VMINT vm_udp_sendto(VMINT hdl, const void* buf, VMINT32 len, const vm_sockaddr_struct* addr);
VMINT vm_udp_recvfrom(VMINT hdl, void* buf, VMINT32 len, vm_sockaddr_struct* addr);
VMINT vm_udp_close(VMINT hdl);

// DNS
#define VM_SOC_DNS_MAX_ADDR 5

typedef struct
{
    VMUINT address[VM_SOC_DNS_MAX_ADDR];
    VMINT num;
    VMINT error_cause;
}vm_soc_dns_result;

typedef VMINT (*vm_dns_callback_t)(vm_soc_dns_result*);

VMINT vm_soc_get_host_by_name(VMINT apn, const VMCHAR* host, vm_soc_dns_result* result, vm_dns_callback_t callback);

// WIFI
VMINT vm_is_support_wifi(void);
VMINT vm_wifi_is_connected(void);


#ifdef __cplusplus
}
#endif