/*
* Copyright (C) 2020  钟先耀
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

/*
* MediaTek mwx(4) HAL for AirportMtwlm, modelled on ItlIwm.
*/

#ifndef ItlMwx_hpp
#define ItlMwx_hpp

#include <compat.h>
#include <linux/kernel.h>

#include <IOKit/IOWorkLoop.h>
#include <IOKit/network/IOGatedOutputQueue.h>
#include <libkern/c++/OSString.h>
#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOLib.h>
#include <libkern/OSKextLib.h>
#include <libkern/c++/OSMetaClass.h>
#include <IOKit/IOFilterInterruptEventSource.h>

#include <HAL/ItlHalService.hpp>
#include <HAL/ItlDriverInfo.hpp>
#include <HAL/ItlDriverController.hpp>

#include "mwx_compat.h"

class ItlMwx : public ItlHalService, ItlDriverInfo, ItlDriverController {
    OSDeclareDefaultStructors(ItlMwx)

public:
    //kext
    void free() override;
    bool attach(IOPCIDevice *device) override;
    void detach(IOPCIDevice *device) override;
    IOReturn enable(IONetworkInterface *netif) override;
    IOReturn disable(IONetworkInterface *netif) override;
    struct ieee80211com *get80211Controller() override;

    static bool intrFilter(OSObject *object, IOFilterInterruptEventSource *src);

    void releaseAll();

    struct _ifnet *getIfp();
    struct mwx_softc *getSoft();
    IOEthernetInterface *getNetworkInterface();

    virtual ItlDriverInfo *getDriverInfo() override;
    virtual ItlDriverController *getDriverController() override;

    //driver info
    const char *getFirmwareVersion() override;
    int16_t getBSSNoise() override;
    bool is5GBandSupport() override;
    int getTxNSS() override;
    const char *getFirmwareName() override;
    UInt32 supportedFeatures() override;
    const char *getFirmwareCountryCode() override;
    uint32_t getTxQueueSize() override;

    //driver controller
    void clearScanningFlags() override;
    IOReturn setMulticastList(IOEthernetAddress *addr, int count) override;

    //glue
    int mwxSleepNsec(void *ident, int priority, const char *wmesg, uint64_t timo);

    static void mwx_intr_action(OSObject *owner, IOInterruptEventSource *, int);

    static bool mwx_match(IOPCIDevice *device);

private:
    struct mwx_softc	com;
    struct pci_attach_args	pci;
    bool		com_attached;
};

#endif /* ItlMwx_hpp */
