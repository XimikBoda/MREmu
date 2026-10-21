#pragma once
#include "ItemsMng.h"
#include <vmsock_fix.h>
#include <memory>

namespace MREngine {
    class TcpSock;
    class UdpSock;

    class AppSock {
    public:
        ItemsMng<std::shared_ptr<TcpSock>> tcps;
        ItemsMng<std::shared_ptr<UdpSock>> udps;
    };    
}

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
