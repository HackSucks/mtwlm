/*
 * mwx_compat.h - macOS/IOKit compatibility layer for the MediaTek mwx(4)
 * driver.
 *
 * The driver body in if_mwx.c is kept as close as possible to the OpenBSD
 * original so that fixes can be shared in both directions.  Everything that
 * differs between the OpenBSD kernel and the itlwm userspace-ish kernel
 * environment lives here.
 *
 * Copyright (c) 2026 NVMe
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#ifndef _MWX_COMPAT_H_
#define _MWX_COMPAT_H_

/*
 * The OpenBSD driver addresses mbufs as `struct mbuf *` while the macOS
 * mbuf KPI uses the opaque `mbuf_t` (`struct __mbuf *`).  Define the
 * alias before any header is included so every declaration sees the
 * same struct tag.
 */
#define	mbuf	__mbuf

#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOTypes.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>

#include <kern/thread.h>
#include <kern/clock.h>

#include <compat.h>

#include <sys/kpi_mbuf.h>
#include <sys/pcireg.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_llc.h>
#include <net/if_media.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

/*
 * The receive path serialises net80211 against the workloop through the
 * command gate that AirportMtwlm creates for the HAL.
 */
extern IOCommandGate *_fCommandGate;

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include "if_mwxreg.h"
#include "if_mwxvar.h"

/*
 * ---------------------------------------------------------------------
 * Kernel services that OpenBSD provides and macOS does not.
 * ---------------------------------------------------------------------
 */

#define NBPFILTER	0

#ifndef ETHER_ADDR_LEN
#define ETHER_ADDR_LEN	6
#endif

/* OpenBSD bit helpers. */
#ifndef isset
#define isset(a, i)	(((const uint8_t *)(a))[(i) / NBBY] & \
			    (1 << ((i) % NBBY)))
#endif
#ifndef setbit
#define setbit(a, i)	(((uint8_t *)(a))[(i) / NBBY] |= \
			    (1 << ((i) % NBBY)))
#endif
#ifndef clrbit
#define clrbit(a, i)	(((uint8_t *)(a))[(i) / NBBY] &= \
			    ~(1 << ((i) % NBBY)))
#endif

#ifndef howmany
#define howmany(x, y)	(((x) + ((y) - 1)) / (y))
#endif

#ifndef MIN
#define MIN(a, b)	((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b)	((a) > (b) ? (a) : (b))
#endif

#ifndef nitems
#define nitems(_a)	(sizeof((_a)) / sizeof((_a)[0]))
#endif

#ifndef __packed
#define __packed	__attribute__((__packed__))
#endif

/* mwx calls printf() with a leading device name; route it to kprintf(). */
void	mwx_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#define	printf(...)	mwx_printf(__VA_ARGS__)

/* The OpenBSD driver derives its unit name from struct device. */
#define	DEVNAME(_s)	((_s)->sc_dev.dv_xname)

#define	M_DEVBUF		2
#define	M_WAIT			3
/* mwx only ever uses single-segment DMA below the 4G boundary. */
#define	BUS_DMA_READ		0
#define	BUS_DMA_WRITE		0
#define	KASSERT(_e)		((void)(_e))
#define	CTASSERT(_e)		static_assert((_e), #_e)
#define	DELAY			IODelay
#define	delay(_us)		IODelay(_us)
#define	INFSLP			UINT64_MAX
#define	hz			1000

/*
 * itl80211 names the interface struct `struct _ifnet`; alias it so the
 * OpenBSD driver body can keep using `struct ifnet`.
 */
#define	ifnet	_ifnet

/*
 * OpenBSD's free() takes pool type and size; the vendored allocator in
 * itl80211/sys/_malloc.h keeps its own length header, so the driver's
 * free(ptr, type, size) calls are mapped to mwx_free(ptr) instead.  A
 * macro cannot be used: it would break IOKit's `virtual void free()`.
 */
void	mwx_free(void *);

/*
 * The macOS net80211 port (and the Airport ioctl layer that sits on top
 * of it) expects ni_rssi to be a positive quality value relative to
 * ic_max_rssi, while the OpenBSD driver body works in dBm.  These are
 * only used by the __APPLE__ conversions in if_mwx.c.
 */
#define	MWX_MIN_DBM	(-100)
#define	MWX_MAX_DBM	(-33)	/* realistic guess, same as IWM_MAX_DBM */

/*
 * Sleeping is implemented by ItlHalService::tsleep_nsec() in itlwm; the
 * glue (mwx_glue.cpp) forwards to the current controller instance.
 * The driver calls mwx_tsleep_nsec() directly: defining a tsleep_nsec
 * macro would break ItlHalService's own declaration of that member.
 */
int	mwx_tsleep_nsec(void *, int, const char *, uint64_t);

#define	IC2IFP(_ic_)		(&(_ic_)->ic_if)

/*
 * Simple atomic bit operations.  The driver only uses them on its own
 * softc fields from task context, so plain read-modify-write is adequate
 * here; the firmware mailbox itself is serialised by its own locks.
 */
static inline void
atomic_setbits_int(volatile u_int *p, u_int v)
{
	*p |= v;
}

static inline void
atomic_clearbits_int(volatile u_int *p, u_int v)
{
	*p &= ~v;
}

#define	atomic_set_bit(_p, _b)		atomic_setbits_int((volatile u_int *)(_p), 1U << (_b))
#define	atomic_clear_bit(_p, _b)	atomic_clearbits_int((volatile u_int *)(_p), 1U << (_b))
#define	atomic_test_bit(_p, _b)		(*(volatile u_int *)(_p) & (1U << (_b)))

/*
 * ---------------------------------------------------------------------
 * net80211 differences between OpenBSD master and the net80211 copy
 * bundled with itlwm.
 *
 * itlwm's net80211 predates the channel-extension flags and the node
 * helper functions added upstream.  The mapping below keeps the driver
 * source identical to the OpenBSD version while using the newer
 * semantics (negotiated AP operation width, not our own capability) to
 * derive the channel context handed to the firmware.
 * ---------------------------------------------------------------------
 */

/* VHT operation channel-width encodings are numerically identical. */
#define	IEEE80211_VHTOP0_CHAN_WIDTH_HT		IEEE80211_VHT_CHANWIDTH_USE_HT
#define	IEEE80211_VHTOP0_CHAN_WIDTH_80		IEEE80211_VHT_CHANWIDTH_80MHZ
#define	IEEE80211_VHTOP0_CHAN_WIDTH_160		IEEE80211_VHT_CHANWIDTH_160MHZ
#define	IEEE80211_VHTOP0_CHAN_WIDTH_8080	IEEE80211_VHT_CHANWIDTH_80P80MHZ

#define	IEEE80211_VHTCAP_CHAN_WIDTH_MASK	IEEE80211_VHTCAP_SUPP_CHAN_WIDTH_MASK
#define	IEEE80211_VHTCAP_CHAN_WIDTH_SHIFT	2
/* These are pre-shift field values in itlwm's net80211. */
#define	IEEE80211_VHTCAP_CHAN_WIDTH_80		0
#define	IEEE80211_VHTCAP_CHAN_WIDTH_160		1	/* 1 << 2 == 4 */
#define	IEEE80211_VHTCAP_CHAN_WIDTH_160_8080	2	/* 2 << 2 == 8 */
#define	IEEE80211_VHTCAP_SGI80			IEEE80211_VHTCAP_SHORT_GI_80
#define	IEEE80211_VHTCAP_SGI160			IEEE80211_VHTCAP_SHORT_GI_160
#define	IEEE80211_VHTCAP_RX_ANT_PATTERN		IEEE80211_VHTCAP_RX_ANTENNA_PATTERN
#define	IEEE80211_VHTCAP_TX_ANT_PATTERN		IEEE80211_VHTCAP_TX_ANTENNA_PATTERN

/* A-MPDU length exponent lives at the same place in both versions. */
#define	IEEE80211_VHTCAP_MAX_AMPDU_LEN_MASK	0x03800000
#define	IEEE80211_VHTCAP_MAX_AMPDU_LEN_SHIFT	23
#define	IEEE80211_VHTCAP_MAX_AMPDU_LEN_8K	0
#define	IEEE80211_VHTCAP_MAX_AMPDU_LEN_16K	1
#define	IEEE80211_VHTCAP_MAX_AMPDU_LEN_32K	2
#define	IEEE80211_VHTCAP_MAX_AMPDU_LEN_64K	3

/* HT A-MPDU Parameters field (IEEE 802.11-2020 9.4.2.57.3). */
#define	IEEE80211_HT_AMPDU_PARM_FACTOR		0x03
#define	IEEE80211_HT_AMPDU_PARM_DENSITY		0x1c
#define	IEEE80211_HT_AMPDU_PARM_DENSITY_SHIFT	2

/* VHT MCS map helpers. */
#ifndef IEEE80211_VHT_MCS_FOR_SS_SHIFT
#define	IEEE80211_VHT_MCS_FOR_SS_SHIFT(n)	(2 * ((n) - 1))
#endif
#ifndef IEEE80211_VHT_MCS_0_9
#define	IEEE80211_VHT_MCS_0_9			2
#endif
#ifndef IEEE80211_VHT_MCS_SS_NOT_SUPP
#define	IEEE80211_VHT_MCS_SS_NOT_SUPP		3
#endif
#ifndef IEEE80211_VHT_NUM_SS
#define	IEEE80211_VHT_NUM_SS			8
#endif

/* Node field names. */
#define	ni_vht_chan_center_freq_idx0	ni_vht_chan1
#define	ni_vht_rxmcs			ni_vht_mcsinfo.rx_mcs_map
#define	ni_vht_txmcs			ni_vht_mcsinfo.tx_mcs_map

/* ieee80211com field names (itlwm carries the full MCS map instead). */
#define	ic_vht_rxmcs			ic_vht_rx_mcs_map
#define	ic_vht_txmcs			ic_vht_tx_mcs_map

/* PCI IDs for the MediaTek parts handled by this driver. */
#define	PCI_VENDOR_MEDIATEK		0x14c3
#define	PCI_PRODUCT_MEDIATEK_MT7921K	0x0608
#define	PCI_PRODUCT_MEDIATEK_RZ616	0x0616
#define	PCI_PRODUCT_MEDIATEK_RZ717	0x0717
#define	PCI_PRODUCT_MEDIATEK_MT7920	0x7920
#define	PCI_PRODUCT_MEDIATEK_MT7922	0x7922
#define	PCI_PRODUCT_MEDIATEK_MT7925	0x7925
#define	PCI_PRODUCT_MEDIATEK_MT7921	0x7961

/*
 * Derive the negotiated channel width from the AP's VHT operation IE,
 * bounded by what our own channel supports.  This mirrors
 * ieee80211_node_vht_channel_width() from current OpenBSD net80211.
 */
static inline uint8_t
mwx_node_vht_channel_width(struct ieee80211_node *ni)
{
	struct ieee80211_channel *c = ni->ni_chan;

	if (c == NULL || !ieee80211_node_supports_vht(ni))
		return IEEE80211_VHTOP0_CHAN_WIDTH_HT;

	switch (ni->ni_vht_chanwidth) {
	case IEEE80211_VHT_CHANWIDTH_160MHZ:
		if (c->ic_flags & IEEE80211_CHAN_VHT160)
			return IEEE80211_VHTOP0_CHAN_WIDTH_160;
		/* FALLTHROUGH */
	case IEEE80211_VHT_CHANWIDTH_80MHZ:
		if (c->ic_flags & IEEE80211_CHAN_VHT80)
			return IEEE80211_VHTOP0_CHAN_WIDTH_80;
		break;
	case IEEE80211_VHT_CHANWIDTH_80P80MHZ:
		/* 80+80 is not supported by these chips. */
		if (c->ic_flags & IEEE80211_CHAN_VHT160)
			return IEEE80211_VHTOP0_CHAN_WIDTH_160;
		if (c->ic_flags & IEEE80211_CHAN_VHT80)
			return IEEE80211_VHTOP0_CHAN_WIDTH_80;
		break;
	default:
		break;
	}
	return IEEE80211_VHTOP0_CHAN_WIDTH_HT;
}

/*
 * Same as ieee80211_node_ht_secondary_channel_offset() upstream: only
 * report a secondary channel when 40 MHz was actually negotiated.
 */
static inline uint8_t
mwx_node_ht_secondary_channel_offset(struct ieee80211_node *ni)
{
	struct ieee80211_channel *c = ni->ni_chan;

	if (c == NULL || !ieee80211_node_supports_ht(ni))
		return IEEE80211_HTOP0_SCO_SCN;
	if (!(c->ic_flags & IEEE80211_CHAN_HT40))
		return IEEE80211_HTOP0_SCO_SCN;
	if (!(ni->ni_htop0 & IEEE80211_HTOP0_CHW))
		return IEEE80211_HTOP0_SCO_SCN;
	return ni->ni_htop0 & IEEE80211_HTOP0_SCO_MASK;
}

#define	ieee80211_node_vht_channel_width(_ni)	mwx_node_vht_channel_width(_ni)
#define	ieee80211_node_ht_secondary_channel_offset(_ni) \
	mwx_node_ht_secondary_channel_offset(_ni)

/*
 * ---------------------------------------------------------------------
 * mbuf.  OpenBSD dereferences struct mbuf fields directly; the macOS
 * mbuf is opaque (mbuf_t is `struct __mbuf *`).  Alias the type name so
 * the driver keeps writing `struct mbuf *`, and replace the handful of
 * field accesses and OpenBSD helper functions that the itl80211 shim
 * does not already provide (compat.h gives m_freem, m_copydata,
 * m_free, M_DONTWAIT, MHLEN, M_EXT).  The `struct mbuf` alias itself
 * is defined at the top of this header, before any include.
 * ---------------------------------------------------------------------
 */

#define	m_adj(_m, _len)		mbuf_adj((_m), (_len))
#define	m_leadingspace(_m)	mbuf_leadingspace(_m)
#define	m_trailingspace(_m)	mbuf_trailingspace(_m)
#define	m_nextpkt(_m)		mbuf_nextpkt(_m)

mbuf_t	mwx_m_pullup(mbuf_t m, int len);
mbuf_t	mwx_m_prepend(mbuf_t m, int len, int how);
mbuf_t	mwx_m_gethdr(int how, int type);
mbuf_t	mwx_m_clget(mbuf_t m, int how, int len);
void	mwx_m_align(mbuf_t m, int len);
mbuf_t	mwx_mclgetl(mbuf_t m, int how, int len);

#define	m_pullup(_m, _len)		mwx_m_pullup((_m), (_len))
#define	m_prepend(_m, _len, _how)	mwx_m_prepend((_m), (_len), (_how))
#define	m_gethdr(_how, _type)		mwx_m_gethdr((_how), (_type))
#define	m_clget(_m, _how, _len)		mwx_m_clget((_m), (_how), (_len))
#define	m_align(_m, _len)		mwx_m_align((_m), (_len))
#define	MCLGETL(_m, _how, _len)		mwx_mclgetl((_m), (_how), (_len))


/*
 * ---------------------------------------------------------------------
 * DMA glue.  bus_dma_segment_t is an IOBufferMemoryDescriptor*, while
 * map->dm_segs entries are IOPhysicalSegment (whose field names differ
 * from OpenBSD's ds_addr/ds_len).  Raw loads go to the shim in
 * itl80211/compat.cpp, which tracks the last bus_dmamem_alloc().
 * ---------------------------------------------------------------------
 */
#define	ds_addr		location
#define	ds_len		length

static inline bool
mwx_dmamem_allocated(bus_dma_segment_t seg)
{
	return seg != NULL;
}

/*
 * itl80211/compat.h aliases bus_dmamap_load_mbuf to the two-argument
 * bus_dmamap_load(map, mbuf) helper.  The OpenBSD driver calls the
 * four-argument form, so redefine it here.  Do not fold it into
 * mwx_dmamap_load_raw(): that one assumes the buffer is inside the
 * most recent bus_dmamem_alloc() ring, which is never true for mbufs.
 */
#undef bus_dmamap_load_mbuf
#define	bus_dmamap_load_mbuf(_tag, _map, _mbuf, _flags) \
	bus_dmamap_load((_map), (mbuf_t)(_mbuf))

static inline void *
mallocarray(size_t n, size_t size, int type, int how)
{
	return malloc(n * size, type, how);
}

/* Present in itl80211 node.c but declared only there. */
void	ieee80211_node_switch_bss(struct ieee80211com *,
	    struct ieee80211_node *);

/* OpenBSD firmware loading, implemented in mwx_glue.cpp over the
 * embedded FwData blob table. */
int	loadfirmware(const char *, u_char **, size_t *);

/*
 * ---------------------------------------------------------------------
 * Driver entry points that the IOKit glue provides.
 * ---------------------------------------------------------------------
 */
bool	mwx_attach(struct mwx_softc *, struct pci_attach_args *,
	    IOEthernetController *);
int	mwx_match(IOPCIDevice *);
void	mwx_detach(struct mwx_softc *);
int	mwx_activate(struct mwx_softc *, int);
int	mwx_intr(void *);

#endif /* _MWX_COMPAT_H_ */
