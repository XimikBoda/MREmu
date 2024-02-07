#include <SFML/Network.hpp>
#include "Sock.h"
//#include <vmsock.h>

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

    memcpy(result->address, &iip, 4);
    result->num = 1;
    result->error_cause = VM_E_SOC_SUCCESS;

    return VM_E_SOC_SUCCESS;
}