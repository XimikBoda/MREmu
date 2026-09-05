#include <SFML/Network.hpp>
#include "../Memory.h"
#include "../Bridge.h"
#include <cstring>
//#include "../Cpu.h"
#include "Sock.h"
//#include <vmsock.h>
#include "../App.h"
#include <spdlog/spdlog.h>

void MREngine::AppSock::update(App* app) { 
    auto cbs = dns_callbacks;
    dns_callbacks.clear();
    for (auto& req : cbs) {
        spdlog::info("[Sock] dispatching DNS callback for result {:p}", (void*)req.result);
        app->run(req.callback, req.result);
    }

    for (int i = 0; i < tcps.size(); ++i) {
        if (!tcps.is_active(i))
            continue;

        auto& tcp = tcps[i];

        if (!tcp.is_connected && tcp.soc->getRemotePort()) {
            tcp.is_connected = true;
            tcp.needs_write_evt = true;
            spdlog::info("[Sock] dispatching VM_TCP_EVT_CONNECTED on handle {}", i);
            app->run(tcp.callback, i, VM_TCP_EVT_CONNECTED);
            continue;
        }

        if (tcp.is_connected && !tcp.is_disconnected) {
            if (tcp.needs_write_evt) {
                tcp.needs_write_evt = false;
                spdlog::info("[Sock] dispatching VM_TCP_EVT_CAN_WRITE on handle {}", i);
                app->run(tcp.callback, i, VM_TCP_EVT_CAN_WRITE);
                if (!tcps.is_active(i) || tcp.is_disconnected)
                    continue;
            }

            if (tcp.receive_tmp_buf_pos != SOCK_TMP_BUF_SIZE) {
                size_t recived = 0;
                tcp.soc->setBlocking(false);
                auto res = tcp.soc->receive(tcp.receive_tmp_buf + tcp.receive_tmp_buf_pos, 
                    SOCK_TMP_BUF_SIZE - tcp.receive_tmp_buf_pos, recived);

                tcp.receive_tmp_buf_pos += recived;

                if (recived) {
                    spdlog::info("[Sock] received {} bytes on handle {}, dispatching VM_TCP_EVT_CAN_READ", recived, i);
                    app->run(tcp.callback, i, VM_TCP_EVT_CAN_READ);
                    if (!tcps.is_active(i) || tcp.is_disconnected)
                        continue;
                }

                if (res == sf::Socket::Disconnected) {
                    spdlog::info("[Sock] remote disconnected on handle {}, dispatching VM_TCP_EVT_PIPE_CLOSED", i);
                    tcp.is_disconnected = true;
                    app->run(tcp.callback, i, VM_TCP_EVT_PIPE_CLOSED);
                }
                else if (res == sf::Socket::Error) {
                    spdlog::info("[Sock] socket error on handle {}, dispatching VM_TCP_EVT_PIPE_BROKEN", i);
                    tcp.is_disconnected = true;
                    app->run(tcp.callback, i, VM_TCP_EVT_PIPE_BROKEN);
                }
            }
        }
    }
}

VMINT vm_is_support_wifi(void) {
	return 1;
}

VMINT vm_wifi_is_connected(void) {
	return 1;
}

VMINT vm_tcp_wifi_connected(void) {
	return 1;
}

VMINT vm_soc_get_last_error(void) {
	return VM_E_SOC_SUCCESS;
}

VMINT vm_soc_get_host_by_name(VMINT apn,
    const VMCHAR* host,
    vm_soc_dns_result* result,
    VMINT(*callback)(vm_soc_dns_result*)) 
{
    spdlog::info("[Sock] vm_soc_get_host_by_name: host={}, apn={}", host ? host : "null", apn);
    if (host == 0 || result == 0)
        return VM_E_SOC_ERROR;

    sf::IpAddress ip(host);
    uint32_t iip = ip.toInteger();

    if (iip == 0) {
        spdlog::warn("[Sock] vm_soc_get_host_by_name failed to resolve: {}", host);
        return VM_E_SOC_ERROR;
    }

    for (int i = 0; i < 4; ++i)
        ((unsigned char*)result->address)[3 - i] = ((unsigned char*)&iip)[i];
    result->num = 1;
    result->error_cause = VM_E_SOC_SUCCESS;

    spdlog::info("[Sock] vm_soc_get_host_by_name: resolved {} -> {}", host, ip.toString());

    if (callback) {
        MREngine::AppSock& app_sock = get_current_app_sock();
        app_sock.dns_callbacks.push_back({ result, callback });
    }

    return VM_E_SOC_SUCCESS;
}

static bool get_proxy_config(std::string& proxy_host, int& proxy_port) {
    const char* p = getenv("MREMU_PROXY");
    if (!p) p = getenv("ALL_PROXY");
    if (!p) p = getenv("all_proxy");
    if (!p) p = getenv("HTTP_PROXY");
    if (!p) p = getenv("http_proxy");
    if (!p || !*p) return false;

    std::string proxy_str = p;
    if (proxy_str.find("://") != std::string::npos)
        proxy_str = proxy_str.substr(proxy_str.find("://") + 3);

    auto colon = proxy_str.find(':');
    if (colon != std::string::npos) {
        proxy_host = proxy_str.substr(0, colon);
        proxy_port = std::atoi(proxy_str.substr(colon + 1).c_str());
        return proxy_port > 0;
    }
    return false;
}

VMINT vm_tcp_connect(const char* host, const VMINT port, const VMINT apn,
    void (*callback)(VMINT handle, VMINT event)) {
    if (!host || port <= 0)
        return -1;

    std::string target_host = host;
    int target_port = port;

    const char* custom_opera = getenv("MREMU_OPERA_SERVER");
    if (custom_opera && *custom_opera) {
        if (target_host.find("opera-mini") != std::string::npos || target_host.find("operamini") != std::string::npos) {
            std::string cstr = custom_opera;
            auto colon = cstr.find(':');
            if (colon != std::string::npos) {
                target_host = cstr.substr(0, colon);
                target_port = std::atoi(cstr.substr(colon + 1).c_str());
            } else {
                target_host = cstr;
            }
            spdlog::info("[Sock] Redirected Opera server {}:{} -> {}:{}", host, port, target_host, target_port);
        }
    }

    spdlog::info("[Sock] vm_tcp_connect: target={}:{}, apn={}", target_host, target_port, apn);

    std::string proxy_host;
    int proxy_port = 0;
    bool use_proxy = get_proxy_config(proxy_host, proxy_port);

    const char* connect_host = use_proxy ? proxy_host.c_str() : target_host.c_str();
    int connect_port = use_proxy ? proxy_port : target_port;

    MREngine::tcp_el tcp = { std::make_shared<sf::TcpSocket>(), callback };

    tcp.soc->setBlocking(true);
    auto res = tcp.soc->connect(connect_host, connect_port, sf::seconds(10));

    if (res != sf::Socket::Done) {
        tcp.soc->setBlocking(false);
        spdlog::warn("[Sock] vm_tcp_connect: failed to connect to {}:{} (res={})", connect_host, connect_port, (int)res);
        return -1;
    }

    if (use_proxy) {
        std::string req = fmt::format("CONNECT {}:{} HTTP/1.1\r\nHost: {}:{}\r\nProxy-Connection: Keep-Alive\r\n\r\n",
            target_host, target_port, target_host, target_port);
        size_t sent = 0;
        tcp.soc->send(req.c_str(), req.length(), sent);

        char resp_buf[1024];
        size_t received = 0;
        auto recv_res = tcp.soc->receive(resp_buf, sizeof(resp_buf) - 1, received);
        if (recv_res != sf::Socket::Done || received < 12) {
            tcp.soc->disconnect();
            tcp.soc->setBlocking(false);
            spdlog::warn("[Sock] Proxy CONNECT received invalid or empty response");
            return -1;
        }
        resp_buf[received] = '\0';
        if (strncmp(resp_buf, "HTTP/1.1 200", 12) != 0 && strncmp(resp_buf, "HTTP/1.0 200", 12) != 0) {
            spdlog::warn("[Sock] Proxy CONNECT to {}:{} failed: {}", target_host, target_port, resp_buf);
            tcp.soc->disconnect();
            tcp.soc->setBlocking(false);
            return -1;
        }
        spdlog::info("[Sock] Tunnel established to {}:{} via proxy {}:{}", target_host, target_port, proxy_host, proxy_port);
    }

    tcp.soc->setBlocking(false);

    MREngine::AppSock& app_sock = get_current_app_sock();
    VMINT handle = app_sock.tcps.push(tcp);
    spdlog::info("[Sock] vm_tcp_connect: successfully connected -> handle {}", handle);
    return handle;
}

void vm_tcp_close(VMINT handle) {
    MREngine::AppSock& app_sock = get_current_app_sock();

    if (app_sock.tcps.is_active(handle)) {
        app_sock.tcps[handle].soc->disconnect();
        app_sock.tcps[handle].is_disconnected = true;
    }

    app_sock.tcps.remove(handle);
}

VMINT vm_tcp_read(VMINT handle, void* buf, VMINT len) {
    MREngine::AppSock& app_sock = get_current_app_sock();

    if (!app_sock.tcps.is_active(handle) || !buf || len <= 0)
        return -1;

    auto& tcp = app_sock.tcps[handle];

    if (tcp.is_disconnected && tcp.receive_tmp_buf_pos == 0)
        return -1;

    size_t from_buf = std::min<size_t>((size_t)len, tcp.receive_tmp_buf_pos);
    if (from_buf) {
        memcpy(buf, tcp.receive_tmp_buf, from_buf);

        if (from_buf != tcp.receive_tmp_buf_pos)
            memmove(tcp.receive_tmp_buf, tcp.receive_tmp_buf + from_buf, tcp.receive_tmp_buf_pos - from_buf);

        tcp.receive_tmp_buf_pos -= from_buf;
    }

    size_t recived = 0;
    if ((size_t)len > from_buf) {
        tcp.soc->setBlocking(false);
        auto res = tcp.soc->receive((char*)buf + from_buf, (size_t)len - from_buf, recived);
        if (res == sf::Socket::Disconnected || res == sf::Socket::Error) {
            tcp.is_disconnected = true;
            if (from_buf == 0)
                return -1;
        }
    }

    VMINT total = (VMINT)(recived + from_buf);
    if (total > 0)
        spdlog::info("[Sock] vm_tcp_read: handle={}, req_len={}, read_len={}", handle, len, total);
    return total;
}

VMINT vm_tcp_write(VMINT handle, void* buf, VMINT len) {
    MREngine::AppSock& app_sock = get_current_app_sock();

    if (!app_sock.tcps.is_active(handle) || !buf || len <= 0)
        return -1;

    auto& tcp = app_sock.tcps[handle];

    if (tcp.is_disconnected)
        return -1;

    size_t writed = 0;
    tcp.soc->setBlocking(false);
    auto res = tcp.soc->send(buf, (size_t)len, writed);
    spdlog::info("[Sock] vm_tcp_write: handle={}, req_len={}, writed={}, res={}", handle, len, writed, (int)res);
    if (res == sf::Socket::Error || res == sf::Socket::Disconnected) {
        tcp.is_disconnected = true;
        return -1;
    }

    if (writed < (size_t)len)
        tcp.needs_write_evt = true;

    return (VMINT)writed;
}
