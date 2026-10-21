#include "../Network.h"
#include "../AppContext.h"
#include "Sock.h"
#include "SockImpl.h"
#include <cstring>
#include <asio/connect.hpp>

//// TCP

void MREngine::TcpSock::push_event(int event) {
    using namespace Event;
    AppContext::push_event(network{ phandle, sock{callback, handle, event} });
}

MREngine::TcpSock::TcpSock(asio::io_context& ctx, void (*c)(int, int), int app_handle) 
    : socket(ctx), resolver(ctx), callback(c), phandle(app_handle){}

void MREngine::TcpSock::connect(const std::string& host, const std::string& port) {
    auto self = shared_from_this();

    resolver.async_resolve(host, port,
        [self, this](const asio::error_code& ec, asio::ip::tcp::resolver::results_type results) {
            if (ec == asio::error::operation_aborted) return;

            if (ec) {
                push_event(VM_TCP_EVT_HOST_NOT_FOUND);
                return;
            }

            asio::async_connect(socket, results,
                [self, this](const asio::error_code& ec, const asio::ip::tcp::endpoint&) {
                    if (ec == asio::error::operation_aborted) return;

                    if (!ec) {
                        asio::error_code ec;
                        socket.non_blocking(true, ec);

                        push_event(VM_TCP_EVT_CONNECTED);

                        wait_for_read();
                        wait_for_write();
                    }
                    else 
                        push_event(VM_TCP_EVT_PIPE_BROKEN);
                });
        });
}

int MREngine::TcpSock::read(void* buffer, size_t length) {
    asio::error_code ec;
    size_t bytes_read = socket.read_some(asio::buffer(buffer, length), ec);

    if (ec == asio::error::would_block) {
        wait_for_read();
        return 0;
    }

    if (ec) return -1;

    if (bytes_read < length)
        wait_for_read();

    return bytes_read;
}

int MREngine::TcpSock::write(void* buffer, size_t length) {
    asio::error_code ec;
    size_t bytes_write = socket.write_some(asio::buffer(buffer, length), ec);

    if (ec == asio::error::would_block) { 
        wait_for_write();
        return 0; 
    }

    if (ec) return -1;

    if (bytes_write < length)
        wait_for_write();

    return bytes_write;
}

void MREngine::TcpSock::wait_for_read() {
    if (is_read_waiting) return;
    is_read_waiting = true;

    auto self = shared_from_this();
    socket.async_wait(asio::socket_base::wait_read,
        [self, this](const asio::error_code& ec) {
            is_read_waiting = false;

            if (ec == asio::error::operation_aborted) return;

            if (!ec) 
                push_event(VM_TCP_EVT_CAN_READ);
            else if (ec == asio::error::eof) 
                push_event(VM_TCP_EVT_PIPE_CLOSED);
            else 
                push_event(VM_TCP_EVT_PIPE_BROKEN);
        });
}

void MREngine::TcpSock::wait_for_write() {
    if (is_write_waiting) return;
    is_write_waiting = true;

    auto self = shared_from_this();
    socket.async_wait(asio::socket_base::wait_write,
        [self, this](const asio::error_code& ec) {
            is_write_waiting = false;

            if (ec == asio::error::operation_aborted) return;

            if (!ec) 
                push_event(VM_TCP_EVT_CAN_WRITE);
            else
                push_event(VM_TCP_EVT_PIPE_BROKEN);
        });
}

void MREngine::TcpSock::close() {
    auto self = shared_from_this();
    asio::post(socket.get_executor(), [self, this]() {
        asio::error_code ec;
        socket.close(ec);
        });
}

//// UDP

static asio::ip::udp::endpoint mre_to_asio_endpoint(const vm_sockaddr_struct* mre_addr) {
    asio::ip::address_v4::bytes_type bytes;
    std::memcpy(bytes.data(), mre_addr->addr, 4);

    return asio::ip::udp::endpoint(asio::ip::address_v4(bytes), mre_addr->port);
}

static void asio_to_mre_endpoint(const asio::ip::udp::endpoint& asio_ep, vm_sockaddr_struct* mre_addr) {
    auto bytes = asio_ep.address().to_v4().to_bytes();
    std::memcpy(mre_addr->addr, bytes.data(), 4);
    mre_addr->port = asio_ep.port();
    mre_addr->addr_len = 4;
}

void MREngine::UdpSock::push_event(int event) {
    using namespace Event;
    AppContext::push_event(network{ phandle, sock{callback, handle, event} });
}

MREngine::UdpSock::UdpSock(asio::io_context& ctx, void (*c)(int, int), int app_handle)
    : socket(ctx), callback(c), phandle(app_handle) {
}

bool MREngine::UdpSock::create(int port, int loopback) {
    asio::error_code ec;

    socket.open(asio::ip::udp::v4(), ec);
    if (ec) return false;

    socket.set_option(asio::socket_base::reuse_address(true), ec);

    auto addr = loopback ? asio::ip::address_v4::loopback() : asio::ip::address_v4::any();

    socket.bind(asio::ip::udp::endpoint(addr, port), ec);
    if (ec) return false;

    socket.non_blocking(true, ec);
    if (ec) return false;

    wait_for_read();
    wait_for_write();

    return true;
}

int MREngine::UdpSock::sendto(const void* buf, size_t len, const vm_sockaddr_struct* addr) {
    asio::error_code ec;
    auto asio_ep = mre_to_asio_endpoint(addr);

    size_t bytes_written = socket.send_to(asio::buffer(buf, len), asio_ep, 0, ec);

    if (ec == asio::error::would_block) {
        wait_for_write();
        return 0;
    }

    if (ec) return -1; 

    if (bytes_written < len) {
        wait_for_write();
    }

    return bytes_written;
}

int MREngine::UdpSock::recvfrom(void* buf, size_t len, vm_sockaddr_struct* addr) {
    asio::error_code ec;
    asio::ip::udp::endpoint sender_ep;

    size_t bytes_read = socket.receive_from(asio::buffer(buf, len), sender_ep, 0, ec);

    if (ec == asio::error::would_block) {
        wait_for_read();
        return 0;
    }

    if (ec) return -1;

    if (addr) {
        asio_to_mre_endpoint(sender_ep, addr);
    }

    wait_for_read();

    return bytes_read;
}

void MREngine::UdpSock::wait_for_read() {
    if (is_read_waiting) return;
    is_read_waiting = true;

    auto self = shared_from_this();
    socket.async_wait(asio::socket_base::wait_read,
        [self, this](const asio::error_code& ec) {
            is_read_waiting = false;

            if (ec == asio::error::operation_aborted) return;

            if (!ec)
                push_event(VM_UDP_EVT_READ);
            else if (ec == asio::error::eof)
                push_event(VM_UDP_EVT_PIPE_CLOSED);
            else
                push_event(VM_UDP_EVT_PIPE_BROKEN);
        });
}

void MREngine::UdpSock::wait_for_write() {
    if (is_write_waiting) return;
    is_write_waiting = true;

    auto self = shared_from_this();
    socket.async_wait(asio::socket_base::wait_write,
        [self, this](const asio::error_code& ec) {
            is_write_waiting = false;

            if (ec == asio::error::operation_aborted) return;

            if (!ec)
                push_event(VM_UDP_EVT_WRITE);
            else
                push_event(VM_UDP_EVT_PIPE_BROKEN);
        });
}

void MREngine::UdpSock::close() {
    auto self = shared_from_this();
    asio::post(socket.get_executor(), [self, this]() {
        asio::error_code ec;
        socket.close(ec);
        });
}

static void fill_dns_result(vm_soc_dns_result* result, const std::vector<asio::ip::address_v4>& ips, VMINT error_cause) {
    result->error_cause = error_cause;

    if (error_cause == VM_E_SOC_SUCCESS && !ips.empty()) {
        result->num = std::min<int>(VM_SOC_DNS_MAX_ADDR, ips.size());

        for (int i = 0; i < result->num; ++i) {
            auto bytes = ips[i].to_bytes();
            std::memcpy(&result->address[i], bytes.data(), 4);
        }
    }
    else {
        result->num = 0;
    }
}


// MRE API

VMINT vm_is_support_wifi(void) {
	return 1;
}

VMINT vm_wifi_is_connected(void) {
	return 1;
}

VMINT vm_soc_get_host_by_name(VMINT apn,
    const VMCHAR* host,
    vm_soc_dns_result* result,
    VMINT(*callback)(vm_soc_dns_result*)) 
{
    if (host == 0 || result == 0)
        return VM_E_SOC_INVAL;

    std::string host_str(host);
    asio::error_code ec;

    asio::ip::address_v4 direct_ip = asio::ip::make_address_v4(host_str, ec);
    if (!ec) {
        fill_dns_result(result, { direct_ip }, VM_E_SOC_SUCCESS);
        return VM_E_SOC_SUCCESS;
    }

    int phandle = AppContext::get_executing_app_phandle();
    
    auto resolver = std::make_shared<asio::ip::tcp::resolver>(Network::getPtr()->get_context());

    resolver->async_resolve(host_str, "",
        [phandle, callback, resolver](const asio::error_code& ec, asio::ip::tcp::resolver::results_type results) {
            if (ec == asio::error::operation_aborted) return;

            Event::dns event_data;
            event_data.callback = callback;

            if (!ec) {
                std::vector<asio::ip::address_v4> ips;
                for (auto& res : results)
                    if (res.endpoint().address().is_v4())
                        ips.push_back(res.endpoint().address().to_v4());

                fill_dns_result(&event_data.result, ips, VM_E_SOC_SUCCESS);
            }
            else
                fill_dns_result(&event_data.result, {}, VM_E_SOC_ERROR);

            using namespace Event;
            AppContext::push_event(network{ phandle, event_data });
        });

    return VM_E_SOC_WOULDBLOCK;
}

VMINT vm_tcp_connect(const char* host, const VMINT port, const VMINT apn,
    void (*callback)(VMINT handle, VMINT event)) 
{
    std::shared_ptr<MREngine::TcpSock> tcp = std::make_shared<MREngine::TcpSock>(Network::getPtr()->get_context(), callback, AppContext::get_executing_app_phandle());
    
    MREngine::AppSock& app_sock = AppContext::get_executing_app_sock();
    int tcp_id = app_sock.tcps.push(tcp);
    
    tcp->handle = tcp_id;
    tcp->connect(host, std::to_string(port));

    return tcp_id;
}

void vm_tcp_close(VMINT handle) {
    MREngine::AppSock& app_sock = AppContext::get_executing_app_sock();

    if (app_sock.tcps.is_active(handle))
        app_sock.tcps[handle]->close();

    app_sock.tcps.remove(handle);
}

VMINT vm_tcp_read(VMINT handle, void* buf, VMINT len) {
    MREngine::AppSock& app_sock = AppContext::get_executing_app_sock();

    if (!app_sock.tcps.is_active(handle))
        return -1;

    auto& tcp = app_sock.tcps[handle];

    return tcp->read(buf, len);
}

VMINT vm_tcp_write(VMINT handle, void* buf, VMINT len) {
    MREngine::AppSock& app_sock = AppContext::get_executing_app_sock();

    if (!app_sock.tcps.is_active(handle))
        return -1;

    auto& tcp = app_sock.tcps[handle];

    return tcp->write(buf, len);
}

VMINT vm_udp_create(VMINT port, VMINT apn, void (*callback)(VMINT hdl, VMINT event), VMINT loopback) {
    std::shared_ptr<MREngine::UdpSock> udp = std::make_shared<MREngine::UdpSock>(Network::getPtr()->get_context(), callback, AppContext::get_executing_app_phandle());

    if (!udp->create(port, loopback)) {
        return -1;
    }

    MREngine::AppSock& app_sock = AppContext::get_executing_app_sock();
    int udp_id = app_sock.udps.push(udp);

    udp->handle = udp_id;

    return udp_id;
}

VMINT vm_udp_sendto(VMINT hdl, const void* buf, VMINT32 len, const vm_sockaddr_struct* addr) {
    MREngine::AppSock& app_sock = AppContext::get_executing_app_sock();

    if (!app_sock.udps.is_active(hdl))
        return -1;

    auto& udp = app_sock.udps[hdl];

    return udp->sendto(buf, len, addr);
}

VMINT vm_udp_recvfrom(VMINT hdl, void* buf, VMINT32 len, vm_sockaddr_struct* addr) {
    MREngine::AppSock& app_sock = AppContext::get_executing_app_sock();

    if (!app_sock.udps.is_active(hdl))
        return -1;

    auto& udp = app_sock.udps[hdl];

    return udp->recvfrom(buf, len, addr);
}

VMINT vm_udp_close(VMINT hdl) {
    MREngine::AppSock& app_sock = AppContext::get_executing_app_sock();

    if (app_sock.udps.is_active(hdl))
        app_sock.udps[hdl]->close();

    app_sock.udps.remove(hdl);
    return 0;
}
