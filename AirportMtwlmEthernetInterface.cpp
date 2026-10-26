//
//  AirportMtwlmEthernetInterface.cpp
//  AirportMtwlm-Sonoma
//
//  Created by qcwap on 2023/6/27.
//  Copyright © 2023 钟先耀. All rights reserved.
//

#include "AirportMtwlmEthernetInterface.hpp"

#include <sys/_if_ether.h>
#include <net80211/ieee80211_var.h>

#define super IOEthernetInterface
OSDefineMetaClassAndStructors(AirportMtwlmEthernetInterface, IOEthernetInterface);

bool AirportMtwlmEthernetInterface::
initWithSkywalkInterfaceAndProvider(IONetworkController *controller, IO80211SkywalkInterface *interface)
{
    bool ret = super::init(controller);
    if (ret)
        this->interface = interface;
    this->isAttach = false;
    return ret;
}

IOReturn AirportMtwlmEthernetInterface::
attachToDataLinkLayer( IOOptionBits options, void *parameter )
{
    XYLog("%s\n", __FUNCTION__);
    char infName[IFNAMSIZ];
    IOReturn ret = super::attachToDataLinkLayer(options, parameter);
    if (ret == kIOReturnSuccess && interface) {
        UInt8 builtIn = 0;
        IOEthernetAddress addr;
        interface->setProperty("built-in", OSData::withBytes(&builtIn, sizeof(builtIn)));
        snprintf(infName, sizeof(infName), "%s%u", ifnet_name(getIfnet()), ifnet_unit(getIfnet()));
        interface->setProperty("IOInterfaceName", OSString::withCString(infName));
        interface->setProperty(kIOInterfaceUnit, OSNumber::withNumber(ifnet_unit(getIfnet()), 8));
        interface->setProperty(kIOInterfaceNamePrefix, OSString::withCString(ifnet_name(getIfnet())));
        if (OSDynamicCast(IOEthernetController, getController())->getHardwareAddress(&addr) == kIOReturnSuccess)
            setProperty(kIOMACAddress,  (void *) &addr,
                        kIOEthernetAddressSize);
        interface->registerService();
        interface->prepareBSDInterface(getIfnet(), 0);
//        ret = bpf_attach(getIfnet(), DLT_RAW, 0x48, &AirportMtwlmEthernetInterface::bpfOutputPacket, &AirportMtwlmEthernetInterface::bpfTap);
    }
    isAttach = true;
    return ret;
}

void AirportMtwlmEthernetInterface::
detachFromDataLinkLayer(IOOptionBits options, void *parameter)
{
    super::detachFromDataLinkLayer(options, parameter);
    isAttach = false;
}

/**
 Add another hack to fake that the provider is IOSkywalkNetworkInterface, to avoid skywalkfamily instance cast panic.
 */
IOService *AirportMtwlmEthernetInterface::
getProvider() const
{
    return isAttach ? this->interface : super::getProvider();
}

errno_t AirportMtwlmEthernetInterface::
bpfOutputPacket(ifnet_t interface, u_int32_t data_link_type, mbuf_t packet)
{
    XYLog("%s data_link_type: %d\n", __FUNCTION__, data_link_type);
    AirportMtwlmEthernetInterface *networkInterface = (AirportMtwlmEthernetInterface *)ifnet_softc(interface);
    return networkInterface->enqueueOutputPacket(packet);
}

errno_t AirportMtwlmEthernetInterface::
bpfTap(ifnet_t interface, u_int32_t data_link_type, bpf_tap_mode direction)
{
    XYLog("%s data_link_type: %d direction: %d\n", __FUNCTION__, data_link_type, direction);
    return 0;
}

bool AirportMtwlmEthernetInterface::
setLinkState(IO80211LinkState state)
{
    if (state == kIO80211NetworkLinkUp) {
        ifnet_set_flags(getIfnet(), ifnet_flags(getIfnet()) | (IFF_UP | IFF_RUNNING), (IFF_UP | IFF_RUNNING));
    } else {
        ifnet_set_flags(getIfnet(), ifnet_flags(getIfnet()) & ~(IFF_UP | IFF_RUNNING), 0);
    }
    return true;
}

UInt32 AirportMtwlmEthernetInterface::
inputPacket(mbuf_t packet, UInt32 length, IOOptionBits options, void *param)
{
    /*
     * EAPOL frames are handled on the 802.11 path until the RSN
     * handshake finishes, so there is nothing special to do here.
     */
    return IOEthernetInterface::inputPacket(packet, length, options, param);
}
