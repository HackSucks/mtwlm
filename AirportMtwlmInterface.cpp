//
//  AirportMtwlmInterface.cpp
//  AirportMtwlm
//
//  Created by qcwap on 2020/9/7.
//  Copyright © 2020 钟先耀. All rights reserved.
//

#include "AirportMtwlmInterface.hpp"

#define super IO80211Interface
OSDefineMetaClassAndStructors(AirportMtwlmInterface, IO80211Interface);

bool AirportMtwlmInterface::
init(IO80211Controller *controller, ItlHalService *halService)
{
    if (!super::init(controller))
        return false;
    this->fHalService = halService;
    return true;
}

UInt32 AirportMtwlmInterface::
inputPacket(mbuf_t packet, UInt32 length, IOOptionBits options, void *param)
{
    ether_header_t *eh;
    size_t len = mbuf_len(packet);
    
    eh = (ether_header_t *)mbuf_data(packet);
    if (len >= sizeof(ether_header_t) && eh->ether_type == htons(ETHERTYPE_PAE)) { // EAPOL packet
        return IO80211Interface::inputPacket(packet, (UInt32)len, 0, param);
    }
    return IOEthernetInterface::inputPacket(packet, length, options, param);
}
