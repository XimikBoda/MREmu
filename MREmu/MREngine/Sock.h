#pragma once
#include "ItemsMng.h"
#include <vmsock_fix.h>
#include <memory>
#include <variant>

namespace MREngine {
    class TcpSock;
    class UdpSock;

    class AppSock {
    public:
        ItemsMng<std::shared_ptr<TcpSock>> tcps;
        ItemsMng<std::shared_ptr<UdpSock>> udps;
    };    
}

namespace Event {
    struct sock { //tcp and udp
        vm_sock_callback_t callback;
        int handle;
        int event;
    };

    struct dns {
        vm_dns_callback_t callback;
        vm_soc_dns_result result;
    };

    struct network {
        int phandle;
        std::variant<sock, dns> data;
    };
}