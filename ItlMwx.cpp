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
* MediaTek mwx(4) HAL for AirportMtwlm.
*/

#include "ItlMwx.hpp"

#include <net80211/ieee80211_var.h>

extern ItlMwx *g_mwx_service;

#undef super
#define super ItlHalService
OSDefineMetaClassAndStructors(ItlMwx, ItlHalService)

bool ItlMwx::
attach(IOPCIDevice *device)
{
    /*
     * On OpenBSD the softc comes from a zeroed autoconf allocation; here
     * it is a plain member of an OSObject.  Zero it so every field starts
     * deterministic (notably sc_alpha2, which is used as a C string).
     */
    bzero(&com, sizeof(com));

    pci.pa_pc = 0;
    pci.pa_dmat = 0;
    pci.pa_tag = device;
    pci.workloop = getMainWorkLoop();

    /* interrupt: single MSI vector, delivered on the main workloop. */
    com.sc_ih = IOFilterInterruptEventSource::filterInterruptEventSource(this,
        &ItlMwx::mwx_intr_action, &ItlMwx::intrFilter, device, 0);
    if (com.sc_ih == NULL ||
        pci.workloop->addEventSource((IOFilterInterruptEventSource *)com.sc_ih)
        != kIOReturnSuccess) {
        IOLog("AirportMtwlm: %s: can't establish interrupt\n", __FUNCTION__);
        /* releaseAll() disables, releases and drops the source. */
        releaseAll();
        return false;
    }

    /*
     * IOInterruptController leaves the vector hard-disabled after
     * registerInterrupt(); enable() is what actually unmasks the MSI.
     * Firmware download runs inside mwx_attach() below and waits for MCU
     * replies on the workloop, so without this every command times out
     * (and the FW download ring can fill and spin forever).
     */
    ((IOFilterInterruptEventSource *)com.sc_ih)->enable();

    g_mwx_service = this;
    if (!mwx_attach(&com, &pci, getController())) {
        g_mwx_service = NULL;
        detach(device);		/* also releases the interrupt source */
        return false;
    }
    com_attached = true;
    return true;
}

void ItlMwx::
detach(IOPCIDevice *device)
{
    /* Stop delivering interrupts before the ring state goes away. */
    if (com.sc_ih != NULL)
        ((IOFilterInterruptEventSource *)com.sc_ih)->disable();
    if (com_attached)
        mwx_detach(&com);
    com_attached = false;
    releaseAll();
}

void ItlMwx::
free()
{
    super::free();
}

void ItlMwx::
releaseAll()
{
    IOFilterInterruptEventSource *intr =
        (IOFilterInterruptEventSource *)com.sc_ih;
    CTimeout *resetTo = com.sc_reset_to;

    if (resetTo != NULL) {
        timeout_del(&com.sc_reset_to);
        timeout_free(&com.sc_reset_to);
        com.sc_reset_to = NULL;
    }
    if (intr != NULL) {
        if (pci.workloop != NULL)
            pci.workloop->removeEventSource(intr);
        intr->disable();
        intr->release();
        com.sc_ih = NULL;
    }
    if (com.sc_nswq != NULL) {
        com.sc_nswq = NULL;
    }
}

IOReturn ItlMwx::
enable(IONetworkInterface *netif)
{
    struct _ifnet *ifp = &com.sc_ic.ic_ac.ac_if;

    if (com.sc_ih != NULL)
        ((IOFilterInterruptEventSource *)com.sc_ih)->enable();
    if (ifp->if_flags & IFF_UP) {
        return kIOReturnSuccess;
    }
    ifp->if_flags |= IFF_UP;
    mwx_activate(&com, DVACT_WAKEUP);
    return kIOReturnSuccess;
}

IOReturn ItlMwx::
disable(IONetworkInterface *netif)
{
    struct _ifnet *ifp = &com.sc_ic.ic_ac.ac_if;

    if (!(ifp->if_flags & IFF_UP)) {
        XYLog("%s already in deactivating state\n", __FUNCTION__);
        return kIOReturnSuccess;
    }
    ifp->if_flags &= ~IFF_UP;
    mwx_activate(&com, DVACT_QUIESCE);
    if (com.sc_ih != NULL)
        ((IOFilterInterruptEventSource *)com.sc_ih)->disable();
    return kIOReturnSuccess;
}

struct ieee80211com *ItlMwx::
get80211Controller()
{
    return &com.sc_ic;
}

ItlDriverInfo *ItlMwx::
getDriverInfo()
{
    return this;
}

ItlDriverController *ItlMwx::
getDriverController()
{
    return this;
}

struct _ifnet *ItlMwx::
getIfp()
{
    return &com.sc_ic.ic_ac.ac_if;
}

struct mwx_softc *ItlMwx::
getSoft()
{
    return &com;
}

IOEthernetInterface *ItlMwx::
getNetworkInterface()
{
    struct _ifnet *ifp = &com.sc_ic.ic_ac.ac_if;
    return ifp->iface;
}

const char *ItlMwx::
getFirmwareVersion()
{
    switch (com.sc_hwtype) {
    case MWX_HW_MT7920:
        return "mt7961_1a";
    case MWX_HW_MT7922:
        return "mt7922_1";
    case MWX_HW_MT7925:
        return "mt7925_1_1";
    default:
        return "mt7961_1";
    }
}

int16_t ItlMwx::
getBSSNoise()
{
    /* mwx does not report noise. */
    return 0;
}

bool ItlMwx::
is5GBandSupport()
{
    return !!(com.sc_capa.has_5ghz);
}

int ItlMwx::
getTxNSS()
{
    /*
     * sc_capa.num_streams is filled by the firmware capability query, so
     * it is still zero if the ioctl runs before the first init.  Report
     * at least one chain, and never more than the 2x2 these parts have.
     */
    int nss = com.sc_capa.num_streams;

    if (nss < 1)
        return 1;
    return nss > 2 ? 2 : nss;
}

const char *ItlMwx::
getFirmwareName()
{
    switch (com.sc_hwtype) {
    case MWX_HW_MT7920:
        return "MT7920";
    case MWX_HW_MT7922:
        return "MT7922";
    case MWX_HW_MT7925:
        return "MT7925";
    default:
        return "MT7921";
    }
}

UInt32 ItlMwx::
supportedFeatures()
{
    return 0;
}

const char *ItlMwx::
getFirmwareCountryCode()
{
    return com.sc_alpha2;
}

uint32_t ItlMwx::
getTxQueueSize()
{
    /* Primary TX ring depth, configured by mwx_dma_alloc(). */
    return com.sc_txq.mq_count;
}

void ItlMwx::
clearScanningFlags()
{
    atomic_clearbits_int(&com.sc_flags, MWX_FLAG_SCANNING | MWX_FLAG_BGSCAN);
}

IOReturn ItlMwx::
setMulticastList(IOEthernetAddress *addr, int count)
{
    /* The firmware does not implement multicast filtering. */
    return kIOReturnSuccess;
}

int ItlMwx::
mwxSleepNsec(void *ident, int priority, const char *wmesg, uint64_t timo)
{
    return tsleep_nsec(ident, priority, wmesg, timo);
}

bool ItlMwx::
intrFilter(OSObject *object, IOFilterInterruptEventSource *src)
{
    ItlMwx *self = (ItlMwx *)object;

    /*
     * Defer to the workloop; mwx_intr() acks and services the WFDMA
     * status register itself.  Keep a count of the vectors that got
     * here so a stalled handler can be told apart from a lost MSI.
     */
    if (self != NULL)
        self->com.sc_intr_filter++;

    return true;
}

void ItlMwx::
mwx_intr_action(OSObject *owner, IOInterruptEventSource *, int)
{
    ItlMwx *self = (ItlMwx *)owner;

    if (self != NULL)
        mwx_intr(&self->com);
}

bool ItlMwx::
mwx_match(IOPCIDevice *device)
{
    return ::mwx_match(device);
}
