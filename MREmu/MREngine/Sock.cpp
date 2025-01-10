#include <SFML/Network.hpp>
#include "../Memory.h"
#include "../Bridge.h"
#include <cstring>
//#include "../Cpu.h"
#include "Sock.h"
//#include <vmsock.h>
#include "../App.h"

void MREngine::AppSock::update(App* app){ 
    for (int i = 0; i < tcps.size(); ++i)
        if (tcps.is_active(i)) {
            auto& tcp = tcps[i];

            if (!tcp.is_connected && tcp.soc->getRemotePort()) {
                tcp.is_connected = true;
                app->run(tcp.callback, i, VM_TCP_EVT_CONNECTED);
            }

            if (tcp.is_connected) {
                if (tcp.receive_tmp_buf_pos != SOCK_TMP_BUF_SIZE) {
                    size_t recived = 0;
                    tcp.soc->setBlocking(false);
                    auto res = tcp.soc->receive(tcp.receive_tmp_buf + tcp.receive_tmp_buf_pos, 
                        SOCK_TMP_BUF_SIZE - tcp.receive_tmp_buf_pos, recived);

                    tcp.receive_tmp_buf_pos += recived;

                    if(recived)
                        app->run(tcp.callback, i, VM_TCP_EVT_CAN_READ);
                }
            }

           /* switch (res)
            {
            case sf::Socket::Done:
                break;
            case sf::Socket::NotReady:
                break;
            case sf::Socket::Partial:
                break;
            case sf::Socket::Disconnected:
                if (!tcp.is_disconnected) {
                    Bridge::run_cpu(tcp.callback, 2, i, VM_TCP_EVT_PIPE_BROKEN);
                }
                tcp.is_disconnected = true;
                break;
            case sf::Socket::Error:
                break;
            default:
                break;
            }*/
            /**/
        }
}

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
        return VM_E_SOC_ERROR;

    sf::IpAddress ip(host);
    uint32_t iip = ip.toInteger();

    if(iip==0)
        return VM_E_SOC_ERROR;

    //memcpy(result->address, &iip, 4);
    for (int i = 0; i < 4; ++i)
        ((unsigned char*)result->address)[3-i] = ((unsigned char*)&iip)[i];
    result->num = 1;
    result->error_cause = VM_E_SOC_SUCCESS;

    return VM_E_SOC_SUCCESS;
}

VMINT vm_tcp_connect(const char* host, const VMINT port, const VMINT apn,
    void (*callback)(VMINT handle, VMINT event)) {

    MREngine::tcp_el tcp = { std::make_shared<sf::TcpSocket>(), callback };

    tcp.soc->setBlocking(true);
    auto res = tcp.soc->connect(host, port);

    switch (res){
    case sf::Socket::Disconnected:
    case sf::Socket::Error:
        return -1;
        break;
    }

    //std::size_t dummy;
    //auto res1 = tcp.soc->receive(&dummy, 0, dummy);

    MREngine::AppSock& app_sock = get_current_app_sock();
    return app_sock.tcps.push(tcp);
}

void vm_tcp_close(VMINT handle) {
    MREngine::AppSock& app_sock = get_current_app_sock();

    if (app_sock.tcps.is_active(handle))
        app_sock.tcps[handle].soc->disconnect();

    app_sock.tcps.remove(handle);
}

VMINT vm_tcp_read(VMINT handle, void* buf, VMINT len) {
    MREngine::AppSock& app_sock = get_current_app_sock();

    if (!app_sock.tcps.is_active(handle))
        return -1;

    auto& tcp = app_sock.tcps[handle];

    size_t from_buf = std::min<size_t>(len, tcp.receive_tmp_buf_pos);
    if (from_buf) {
        memcpy(buf, tcp.receive_tmp_buf, from_buf);

        if (from_buf != tcp.receive_tmp_buf_pos)
            memmove(tcp.receive_tmp_buf, tcp.receive_tmp_buf + from_buf, tcp.receive_tmp_buf_pos - from_buf);

        tcp.receive_tmp_buf_pos -= from_buf;
    }

    size_t recived = 0;
    auto res = tcp.soc->receive((char*)buf + from_buf, len - from_buf, recived);

    return recived + from_buf;
}

VMINT vm_tcp_write(VMINT handle, void* buf, VMINT len) {
    MREngine::AppSock& app_sock = get_current_app_sock();

    if (!app_sock.tcps.is_active(handle))
        return -1;

    auto& tcp = app_sock.tcps[handle];

    size_t writed = 0;
    tcp.soc->setBlocking(true);
    auto res = tcp.soc->send(buf, len, writed);
    tcp.soc->setBlocking(false);

    return writed;
}
