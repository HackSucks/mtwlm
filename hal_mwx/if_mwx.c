/*	$OpenBSD: if_mwx.c,v 1.47 2026/10/17 16:12:45 deraadt Exp $ */
/*
 * Copyright (c) 2022 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2021 MediaTek Inc.
 * Copyright (c) 2021 Lorenzo Bianconi <lorenzo.bianconi83@gmail.com>
 * Copyright (c) 2017 Stefan Sperling <stsp@openbsd.org>
 * Copyright (c) 2016 Felix Fietkau <nbd@nbd.name>
 * Copyright (c) 2007-2010 Damien Bergamini <damien.bergamini@free.fr>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "mwx_compat.h"

static const struct pci_matchid mwx_devices[] = {
	{ PCI_VENDOR_MEDIATEK, PCI_PRODUCT_MEDIATEK_MT7920 },
	{ PCI_VENDOR_MEDIATEK, PCI_PRODUCT_MEDIATEK_MT7921 },
	{ PCI_VENDOR_MEDIATEK, PCI_PRODUCT_MEDIATEK_MT7921K },
	{ PCI_VENDOR_MEDIATEK, PCI_PRODUCT_MEDIATEK_MT7922 },
	{ PCI_VENDOR_MEDIATEK, PCI_PRODUCT_MEDIATEK_RZ616 },
	{ PCI_VENDOR_MEDIATEK, PCI_PRODUCT_MEDIATEK_MT7925 },
	{ PCI_VENDOR_MEDIATEK, PCI_PRODUCT_MEDIATEK_RZ717 },
};

#define MWX_DEBUG	1

#define	MT7920_ROM_PATCH	"mwx-mt7961_patch_mcu_1a_2_hdr"
#define	MT7920_FIRMWARE_WM	"mwx-mt7961_ram_code_1a"
#define	MT7921_ROM_PATCH	"mwx-mt7961_patch_mcu_1_2_hdr"
#define	MT7921_FIRMWARE_WM	"mwx-mt7961_ram_code_1"
#define	MT7922_ROM_PATCH	"mwx-mt7922_patch_mcu_1_1_hdr"
#define	MT7922_FIRMWARE_WM	"mwx-mt7922_ram_code_1"
#define	MT7925_ROM_PATCH	"mwx-mt7925_patch_mcu_1_1_hdr"
#define	MT7925_FIRMWARE_WM	"mwx-mt7925_ram_code_1_1"

struct mwx_connac_reg_map {
	uint32_t	phys;
	uint32_t	maps;
	uint32_t	size;
};

static const struct mwx_connac_reg_map mt7921_fixed_map[] = {
	{ 0x820d0000, 0x30000, 0x10000 }, /* WF_LMAC_TOP (WF_WTBLON) */
	{ 0x820ed000, 0x24800, 0x00800 }, /* WF_LMAC_TOP BN0 (WF_MIB) */
	{ 0x820e4000, 0x21000, 0x00400 }, /* WF_LMAC_TOP BN0 (WF_TMAC) */
	{ 0x820e7000, 0x21e00, 0x00200 }, /* WF_LMAC_TOP BN0 (WF_DMA) */
	{ 0x820eb000, 0x24200, 0x00400 }, /* WF_LMAC_TOP BN0 (WF_LPON) */
	{ 0x820e2000, 0x20800, 0x00400 }, /* WF_LMAC_TOP BN0 (WF_AGG) */
	{ 0x820e3000, 0x20c00, 0x00400 }, /* WF_LMAC_TOP BN0 (WF_ARB) */
	{ 0x820e5000, 0x21400, 0x00800 }, /* WF_LMAC_TOP BN0 (WF_RMAC) */
	{ 0x00400000, 0x80000, 0x10000 }, /* WF_MCU_SYSRAM */
	{ 0x00410000, 0x90000, 0x10000 }, /* WF_MCU_SYSRAM (conf register) */
	{ 0x40000000, 0x70000, 0x10000 }, /* WF_UMAC_SYSRAM */
	{ 0x54000000, 0x02000, 0x01000 }, /* WFDMA PCIE0 MCU DMA0 */
	{ 0x55000000, 0x03000, 0x01000 }, /* WFDMA PCIE0 MCU DMA1 */
	{ 0x58000000, 0x06000, 0x01000 }, /* WFDMA PCIE1 MCU DMA0 (MEM_DMA) */
	{ 0x59000000, 0x07000, 0x01000 }, /* WFDMA PCIE1 MCU DMA1 */
	{ 0x7c000000, 0xf0000, 0x10000 }, /* CONN_INFRA */
	{ 0x7c020000, 0xd0000, 0x10000 }, /* CONN_INFRA, WFDMA */
	{ 0x7c060000, 0xe0000, 0x10000 }, /* CONN_INFRA, conn_host_csr_top */
	{ 0x80020000, 0xb0000, 0x10000 }, /* WF_TOP_MISC_OFF */
	{ 0x81020000, 0xc0000, 0x10000 }, /* WF_TOP_MISC_ON */
	{ 0x820c0000, 0x08000, 0x04000 }, /* WF_UMAC_TOP (PLE) */
	{ 0x820c8000, 0x0c000, 0x02000 }, /* WF_UMAC_TOP (PSE) */
	{ 0x820cc000, 0x0e000, 0x01000 }, /* WF_UMAC_TOP (PP) */
	{ 0x820cd000, 0x0f000, 0x01000 }, /* WF_MDP_TOP */
	{ 0x74030000, 0x10000, 0x10000 }, /* PCIE_MAC_IREG */
	{ 0x820ce000, 0x21c00, 0x00200 }, /* WF_LMAC_TOP (WF_SEC) */
	{ 0x820cf000, 0x22000, 0x01000 }, /* WF_LMAC_TOP (WF_PF) */
	{ 0x820e0000, 0x20000, 0x00400 }, /* WF_LMAC_TOP BN0 (WF_CFG) */
	{ 0x820e1000, 0x20400, 0x00200 }, /* WF_LMAC_TOP BN0 (WF_TRB) */
	{ 0x820e9000, 0x23400, 0x00200 }, /* WF_LMAC_TOP BN0 (WF_WTBLOFF) */
	{ 0x820ea000, 0x24000, 0x00200 }, /* WF_LMAC_TOP BN0 (WF_ETBF) */
	{ 0x820ec000, 0x24600, 0x00200 }, /* WF_LMAC_TOP BN0 (WF_INT) */
	{ 0x820f0000, 0xa0000, 0x00400 }, /* WF_LMAC_TOP BN1 (WF_CFG) */
	{ 0x820f1000, 0xa0600, 0x00200 }, /* WF_LMAC_TOP BN1 (WF_TRB) */
	{ 0x820f2000, 0xa0800, 0x00400 }, /* WF_LMAC_TOP BN1 (WF_AGG) */
	{ 0x820f3000, 0xa0c00, 0x00400 }, /* WF_LMAC_TOP BN1 (WF_ARB) */
	{ 0x820f4000, 0xa1000, 0x00400 }, /* WF_LMAC_TOP BN1 (WF_TMAC) */
	{ 0x820f5000, 0xa1400, 0x00800 }, /* WF_LMAC_TOP BN1 (WF_RMAC) */
	{ 0x820f7000, 0xa1e00, 0x00200 }, /* WF_LMAC_TOP BN1 (WF_DMA) */
	{ 0x820f9000, 0xa3400, 0x00200 }, /* WF_LMAC_TOP BN1 (WF_WTBLOFF) */
	{ 0x820fa000, 0xa4000, 0x00200 }, /* WF_LMAC_TOP BN1 (WF_ETBF) */
	{ 0x820fb000, 0xa4200, 0x00400 }, /* WF_LMAC_TOP BN1 (WF_LPON) */
	{ 0x820fc000, 0xa4600, 0x00200 }, /* WF_LMAC_TOP BN1 (WF_INT) */
	{ 0x820fd000, 0xa4800, 0x00800 }, /* WF_LMAC_TOP BN1 (WF_MIB) */
};

static const struct mwx_connac_reg_map mt7925_fixed_map[] = {
	{ 0x830c0000, 0x000000, 0x0001000 }, /* WF_MCU_BUS_CR_REMAP */
	{ 0x54000000, 0x002000, 0x0001000 }, /* WFDMA PCIE0 MCU DMA0 */
	{ 0x55000000, 0x003000, 0x0001000 }, /* WFDMA PCIE0 MCU DMA1 */
	{ 0x56000000, 0x004000, 0x0001000 }, /* WFDMA reserved */
	{ 0x57000000, 0x005000, 0x0001000 }, /* WFDMA MCU wrap CR */
	{ 0x58000000, 0x006000, 0x0001000 }, /* WFDMA PCIE1 MCU DMA0 */
	{ 0x59000000, 0x007000, 0x0001000 }, /* WFDMA PCIE1 MCU DMA1 */
	{ 0x820c0000, 0x008000, 0x0004000 }, /* WF_UMAC_TOP (PLE) */
	{ 0x820c8000, 0x00c000, 0x0002000 }, /* WF_UMAC_TOP (PSE) */
	{ 0x820cc000, 0x00e000, 0x0002000 }, /* WF_UMAC_TOP (PP) */
	{ 0x74030000, 0x010000, 0x0001000 }, /* PCIe MAC */
	{ 0x820e0000, 0x020000, 0x0000400 }, /* WF_LMAC_TOP BN0 (WF_CFG) */
	{ 0x820e1000, 0x020400, 0x0000200 }, /* WF_LMAC_TOP BN0 (WF_TRB) */
	{ 0x820e2000, 0x020800, 0x0000400 }, /* WF_LMAC_TOP BN0 (WF_AGG) */
	{ 0x820e3000, 0x020c00, 0x0000400 }, /* WF_LMAC_TOP BN0 (WF_ARB) */
	{ 0x820e4000, 0x021000, 0x0000400 }, /* WF_LMAC_TOP BN0 (WF_TMAC) */
	{ 0x820e5000, 0x021400, 0x0000800 }, /* WF_LMAC_TOP BN0 (WF_RMAC) */
	{ 0x820ce000, 0x021c00, 0x0000200 }, /* WF_LMAC_TOP (WF_SEC) */
	{ 0x820e7000, 0x021e00, 0x0000200 }, /* WF_LMAC_TOP BN0 (WF_DMA) */
	{ 0x820cf000, 0x022000, 0x0001000 }, /* WF_LMAC_TOP (WF_PF) */
	{ 0x820e9000, 0x023400, 0x0000200 }, /* WF_LMAC_TOP BN0 (WF_WTBLOFF) */
	{ 0x820ea000, 0x024000, 0x0000200 }, /* WF_LMAC_TOP BN0 (WF_ETBF) */
	{ 0x820eb000, 0x024200, 0x0000400 }, /* WF_LMAC_TOP BN0 (WF_LPON) */
	{ 0x820ec000, 0x024600, 0x0000200 }, /* WF_LMAC_TOP BN0 (WF_INT) */
	{ 0x820ed000, 0x024800, 0x0000800 }, /* WF_LMAC_TOP BN0 (WF_MIB) */
	{ 0x820ca000, 0x026000, 0x0002000 }, /* WF_LMAC_TOP BN0 (WF_MUCOP) */
	{ 0x820d0000, 0x030000, 0x0010000 }, /* WF_LMAC_TOP (WF_WTBLON) */
	{ 0x40000000, 0x070000, 0x0010000 }, /* WF_UMAC_SYSRAM */
	{ 0x00400000, 0x080000, 0x0010000 }, /* WF_MCU_SYSRAM */
	{ 0x00410000, 0x090000, 0x0010000 }, /* WF_MCU_SYSRAM (config reg) */
	{ 0x820f0000, 0x0a0000, 0x0000400 }, /* WF_LMAC_TOP BN1 (WF_CFG) */
	{ 0x820f1000, 0x0a0600, 0x0000200 }, /* WF_LMAC_TOP BN1 (WF_TRB) */
	{ 0x820f2000, 0x0a0800, 0x0000400 }, /* WF_LMAC_TOP BN1 (WF_AGG) */
	{ 0x820f3000, 0x0a0c00, 0x0000400 }, /* WF_LMAC_TOP BN1 (WF_ARB) */
	{ 0x820f4000, 0x0a1000, 0x0000400 }, /* WF_LMAC_TOP BN1 (WF_TMAC) */
	{ 0x820f5000, 0x0a1400, 0x0000800 }, /* WF_LMAC_TOP BN1 (WF_RMAC) */
	{ 0x820f7000, 0x0a1e00, 0x0000200 }, /* WF_LMAC_TOP BN1 (WF_DMA) */
	{ 0x820f9000, 0x0a3400, 0x0000200 }, /* WF_LMAC_TOP BN1 (WF_WTBLOFF) */
	{ 0x820fa000, 0x0a4000, 0x0000200 }, /* WF_LMAC_TOP BN1 (WF_ETBF) */
	{ 0x820fb000, 0x0a4200, 0x0000400 }, /* WF_LMAC_TOP BN1 (WF_LPON) */
	{ 0x820fc000, 0x0a4600, 0x0000200 }, /* WF_LMAC_TOP BN1 (WF_INT) */
	{ 0x820fd000, 0x0a4800, 0x0000800 }, /* WF_LMAC_TOP BN1 (WF_MIB) */
	{ 0x820c4000, 0x0a8000, 0x0004000 }, /* WF_LMAC_TOP BN1 (WF_MUCOP) */
	{ 0x820b0000, 0x0ae000, 0x0001000 }, /* [APB2] WFSYS_ON */
	{ 0x80020000, 0x0b0000, 0x0010000 }, /* WF_TOP_MISC_OFF */
	{ 0x81020000, 0x0c0000, 0x0010000 }, /* WF_TOP_MISC_ON */
	{ 0x7c020000, 0x0d0000, 0x0010000 }, /* CONN_INFRA, wfdma */
	{ 0x7c060000, 0x0e0000, 0x0010000 }, /* CONN_INFRA, conn_host_csr_top */
	{ 0x7c000000, 0x0f0000, 0x0010000 }, /* CONN_INFRA */
	{ 0x70020000, 0x1f0000, 0x0010000 }, /* Reserved for CBTOP */
	{ 0x7c500000, 0x060000, 0x2000000 }, /* remap */
};

#if NBPFILTER > 0
struct mwx_rx_radiotap_header {
	struct ieee80211_radiotap_header	wr_ihdr;
	uint64_t				wr_tsft;
	uint8_t					wr_flags;
	uint8_t					wr_rate;
	uint16_t				wr_chan_freq;
	uint16_t				wr_chan_flags;
	int8_t					wr_dbm_antsignal;
	int8_t					wr_dbm_antnoise;
} __packed;

#define	MWX_RX_RADIOTAP_PRESENT				\
	((1 << IEEE80211_RADIOTAP_TSFT) |		\
	(1 << IEEE80211_RADIOTAP_FLAGS) |		\
	(1 << IEEE80211_RADIOTAP_RATE) |		\
	(1 << IEEE80211_RADIOTAP_CHANNEL) |		\
	(1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL) |	\
	(1 << IEEE80211_RADIOTAP_DBM_ANTNOISE))

struct mwx_tx_radiotap_header {
	struct ieee80211_radiotap_header	wt_ihdr;
	uint8_t					wt_flags;
	uint8_t					wt_rate;
	uint16_t				wt_chan_freq;
	uint16_t				wt_chan_flags;
} __packed;

#define	MWX_TX_RADIOTAP_PRESENT			\
	((1 << IEEE80211_RADIOTAP_FLAGS) |	\
	(1 << IEEE80211_RADIOTAP_RATE) |	\
	(1 << IEEE80211_RADIOTAP_CHANNEL))

#endif

struct mwx_txwi {
	struct mt76_txwi		*mt_desc;
	struct mbuf			*mt_mbuf;
	struct ieee80211_node		*mt_ni;
	bus_dmamap_t			mt_map;
	LIST_ENTRY(mwx_txwi)		mt_entry;
	u_int32_t			mt_addr;
	u_int				mt_idx;
	int				mt_busy;
};

#include "if_mwxvar.h"

const uint8_t mwx_channels_2ghz[] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
};

#define MWX_NUM_2GHZ_CHANNELS   nitems(mwx_channels_2ghz)

const uint8_t mwx_channels_5ghz[] = {
	36, 40, 44, 48, 52, 56, 60, 64,
	100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144,
	149, 153, 157, 161, 165, 169, 173
};

#define MWX_NUM_5GHZ_CHANNELS   nitems(mwx_channels_5ghz)

const uint8_t mwx_channels_6ghz[] = {
	/* UNII-5 */
	1, 5, 9, 13, 17, 21, 25, 29, 33, 37, 41, 45, 49, 53, 57,
	61, 65, 69, 73, 77, 81, 85, 89, 93,
	/* UNII-6 */
	97, 101, 105, 109, 113, 117,
	/* UNII-7 */
	121, 125, 129, 133, 137, 141, 145, 149, 153, 157, 161, 165, 169,
	173, 177, 181, 185,
	/* UNII-8 */
	189, 193, 197, 201, 205, 209, 213, 217, 221, 225, 229, 233
};

const struct mwx_rate {
	uint16_t	rate;
	uint16_t	hw_value;
} mt76_rates[] = {
	{	2,	(MT_PHY_TYPE_CCK << 8) | 0 },
	{	4,	(MT_PHY_TYPE_CCK << 8) | 1 },
	{	11,	(MT_PHY_TYPE_CCK << 8) | 2 },
	{	22,	(MT_PHY_TYPE_CCK << 8) | 3 },
	{	12,	(MT_PHY_TYPE_OFDM << 8) | 11 },
	{	18,	(MT_PHY_TYPE_OFDM << 8) | 15 },
	{	24,	(MT_PHY_TYPE_OFDM << 8) | 10 },
	{	36,	(MT_PHY_TYPE_OFDM << 8) | 14 },
	{	48,	(MT_PHY_TYPE_OFDM << 8) | 9 },
	{	72,	(MT_PHY_TYPE_OFDM << 8) | 13 },
	{	96,	(MT_PHY_TYPE_OFDM << 8) | 8 },
	{	108,	(MT_PHY_TYPE_OFDM << 8) | 12 },
};

#define MWX_NUM_6GHZ_CHANNELS   nitems(mwx_channels_6ghz)

#define	DEVNAME(s)	((s)->sc_dev.dv_xname)
#define	DEVDEBUG(x)	((x)->sc_ic.ic_if.if_flags & IFF_DEBUG)

#ifdef MWX_DEBUG
#define	DPRINTF(x...)		printf(x)
#else
#define	DPRINTF(x...)
#endif

struct mbuf	*ieee80211_get_probe_req(struct ieee80211com *,
		    struct ieee80211_node *);

int		mwx_init(struct ifnet *);
void		mwx_stop(struct ifnet *);
void		mwx_watchdog(struct ifnet *);
void		mwx_start(struct ifnet *);
int		mwx_ioctl(struct ifnet *, u_long, caddr_t);

struct ieee80211_node *mwx_node_alloc(struct ieee80211com *);
int		mwx_media_change(struct ifnet *);
#if NBPFILTER > 0
void		mwx_radiotap_attach(struct mwx_softc *);
#endif

int		mwx_newstate(struct ieee80211com *, enum ieee80211_state, int);
void		mwx_newstate_task(void *);
static uint64_t	mwx_uptime_ms(void);
static int	mwx_scan_check_stale(struct mwx_softc *, uint64_t);
int		mwx_scan(struct mwx_softc *);
int		mwx_bgscan(struct ieee80211com *);
void		mwx_bgscan_done(struct ieee80211com *,
		    struct ieee80211_node_switch_bss_arg *, size_t);
void		mwx_bgscan_done_task(void *);
void		mwx_bmiss_task(void *);
int		mwx_set_key(struct ieee80211com *, struct ieee80211_node *,
		    struct ieee80211_key *);
void		mwx_delete_key(struct ieee80211com *, struct ieee80211_node *,
		    struct ieee80211_key *);

int		mwx_tx(struct mwx_softc *, struct mbuf *,
		    struct ieee80211_node *);
void		mwx_rx(struct mwx_softc *, struct mbuf *, struct mbuf_list *);
int		mwx_intr(void *);
int		mwx_preinit(struct mwx_softc *);
#ifdef __APPLE__
int		mwx_match(IOPCIDevice *);
bool		mwx_attach(struct mwx_softc *, struct pci_attach_args *,
		    IOEthernetController *);
void		mwx_detach(struct mwx_softc *);
int		mwx_activate(struct mwx_softc *, int);
#else
void		mwx_attach_hook(struct device *);
int		mwx_match(struct device *, void *, void *);
void		mwx_attach(struct device *, struct device *, void *);
int		mwx_activate(struct device *, int);
#endif

void		mwx_reset(struct mwx_softc *);
void		mwx_reset_timeo(void *);
void		mwx_reset_task(void *);
int		mwx_txwi_alloc(struct mwx_softc *, int);
void		mwx_txwi_free(struct mwx_softc *);
struct mwx_txwi	*mwx_txwi_get(struct mwx_softc *);
void		mwx_txwi_put(struct mwx_softc *, struct mwx_txwi *);
int		mwx_txwi_enqueue(struct mwx_softc *, struct mwx_txwi *,
		    struct mbuf *, struct ieee80211_node *);
int		mwx_queue_alloc(struct mwx_softc *, struct mwx_queue *, int,
		    uint32_t);
void		mwx_queue_free(struct mwx_softc *, struct mwx_queue *);
void		mwx_queue_reset(struct mwx_softc *, struct mwx_queue *);
int		mwx_buf_fill(struct mwx_softc *, struct mwx_queue_data *,
		    struct mwx_desc *);
int		mwx_queue_fill(struct mwx_softc *, struct mwx_queue *);
int		mwx_dma_alloc(struct mwx_softc *);
int		mwx_dma_reset(struct mwx_softc *, int);
void		mwx_dma_free(struct mwx_softc *);
int		mwx_dma_tx_enqueue(struct mwx_softc *, struct mwx_queue *,
		    struct mbuf *);
void		mwx_dma_txwi_enqueue(struct mwx_softc *, struct mwx_queue *,
		    struct mwx_txwi *);
void		mwx_dma_tx_cleanup(struct mwx_softc *, struct mwx_queue *);
void		mwx_dma_tx_done(struct mwx_softc *);
void		mwx_dma_rx_process(struct mwx_softc *, struct mbuf_list *,
		    int);
void		mwx_dma_rx_dequeue(struct mwx_softc *, struct mwx_queue *,
		    struct mbuf_list *);
void		mwx_dma_rx_done(struct mwx_softc *, struct mwx_queue *);

struct mbuf	*mwx_mcu_alloc_msg(size_t);
void		mwx_mcu_set_len(struct mbuf *, void *);
static void	mwx_mcu_dump_state(struct mwx_softc *, const char *);
int		mwx_mcu_send_mbuf(struct mwx_softc *, uint32_t, struct mbuf *,
		    int *);
int		mwx_mcu_send_msg(struct mwx_softc *, uint32_t, void *, size_t,
		    int *);
int		mwx_mcu_send_wait(struct mwx_softc *, uint32_t, void *, size_t);
int		mwx_mcu_send_mbuf_wait(struct mwx_softc *, uint32_t,
		    struct mbuf *);
int		mwx_mcu_send_mbuf_wait_status(struct mwx_softc *, uint32_t,
		    struct mbuf *, uint32_t *, int);
void		mwx_mcu_rx_event(struct mwx_softc *, struct mbuf *);
void		mwx_mcu_rx_unsolicited_event(struct mwx_softc *,
		    struct mwx_mcu_rxd *, struct mbuf *);
static void	mwx_dma_service(struct mwx_softc *);
static void	mwx_dma_rx_deliver(struct mwx_softc *);
static void	mwx_dma_poll(struct mwx_softc *);
static void	mwx_ring_lock(struct mwx_softc *);
static void	mwx_ring_unlock(struct mwx_softc *);
static void	mwx_tx_lock(struct mwx_softc *);
static void	mwx_tx_unlock(struct mwx_softc *);
static int	mwx_mcu_wait_slot(struct mwx_softc *, int, int);
int		mwx_mcu_wait_resp_int(struct mwx_softc *, uint32_t, int,
		    uint32_t *);
int		mwx_mcu_wait_resp_int_ms(struct mwx_softc *, uint32_t, int,
		    uint32_t *, int);
int		mwx_mcu_wait_resp_msg(struct mwx_softc *, uint32_t, int,
		    struct mbuf **);

int		mwx_dma_disable(struct mwx_softc *, int);
void		mwx_dma_enable(struct mwx_softc *);
int		mwx_mcu_fw_pmctrl(struct mwx_softc *);
int		mwx_mcu_drv_pmctrl(struct mwx_softc *);
int		mwx_wfsys_reset(struct mwx_softc *);
uint32_t	mwx_reg_addr(struct mwx_softc *, uint32_t);
int		mwx_init_hardware(struct mwx_softc *);
int		mwx_load_firmware(struct mwx_softc *);
int		mwx_mac_wtbl_update(struct mwx_softc *, uint32_t);
void		mt7925_mac_set_fixed_rate_table(struct mwx_softc *, uint8_t,
		    uint16_t);
void		mt7925_mac_init_basic_rates(struct mwx_softc *);
void		mwx_mac_init_band(struct mwx_softc *, uint32_t);
void		mwx_mac_init(struct mwx_softc *);
int		mwx_mcu_patch_sem_ctrl(struct mwx_softc *, int);
int		mwx_mcu_init_download(struct mwx_softc *, uint32_t,
		    uint32_t, uint32_t);
int		mwx_mcu_send_firmware(struct mwx_softc *, int,
		    u_char *, size_t, size_t);
int		mwx_mcu_start_patch(struct mwx_softc *);
int		mwx_mcu_start_firmware(struct mwx_softc *, uint32_t,
		    uint32_t);
int		mt7921_mcu_get_nic_capability(struct mwx_softc *);
int		mt7925_mcu_get_nic_capability(struct mwx_softc *);
int		mt7921_mcu_fw_log_2_host(struct mwx_softc *, uint8_t);
int		mt7925_mcu_fw_log_2_host(struct mwx_softc *, uint8_t);
int		mt7921_mcu_set_eeprom(struct mwx_softc *);
int		mt7925_mcu_set_eeprom(struct mwx_softc *);
int		mt7921_mcu_set_rts_thresh(struct mwx_softc *, uint32_t,
		    uint8_t);
int		mt7925_mcu_set_rts_thresh(struct mwx_softc *, uint32_t);
int		mwx_mcu_set_deep_sleep(struct mwx_softc *, int);
void		mt7921_mcu_low_power_event(struct mwx_softc *, struct mbuf *);
void		mt7921_mcu_tx_done_event(struct mwx_softc *, struct mbuf *);
void		mwx_end_scan_task(void *);
void		mt7921_mcu_scan_event(struct mwx_softc *, struct mbuf *);
void		mt7925_mcu_scan_event(struct mwx_softc *, struct mbuf *);
int		mt7921_mcu_hw_scan(struct mwx_softc *, int);
int		mt7925_mcu_hw_scan(struct mwx_softc *, int);
int		mt7921_mcu_hw_scan_cancel(struct mwx_softc *);
int		mt7925_mcu_hw_scan_cancel(struct mwx_softc *);
int		mt7921_mcu_set_mac_enable(struct mwx_softc *, int, int);
int		mt7921_mcu_set_channel_domain(struct mwx_softc *);
int		mt7925_mcu_set_channel_domain(struct mwx_softc *);
int		mt7921_mcu_set_chan_info(struct mwx_softc *, int);
void		mt7921_mcu_build_sku(struct mwx_softc *, int, int8_t *);
int		mt7921_mcu_rate_txpower_band(struct mwx_softc *, int,
		    const uint8_t *, int, int);
int		mt7921_mcu_set_rate_txpower(struct mwx_softc *);
void		mwx_mac_reset_counters(struct mwx_softc *);
void		mt7921_mac_set_timing(struct mwx_softc *);
int		mwx_mcu_uni_add_dev(struct mwx_softc *, struct mwx_vif *,
		    struct mwx_node *, int);
int		mwx_mcu_uni_add_bss(struct mwx_softc *, struct mwx_vif *,
		    struct ieee80211_node *, struct mwx_node *, int);
int		mwx_mcu_set_sniffer(struct mwx_softc *, int);
int		mt7925_mcu_config_sniffer(struct mwx_softc *);
int		mt7921_mcu_set_beacon_filter(struct mwx_softc *, int);
int		mt7921_mcu_set_bss_pm(struct mwx_softc *, int);
int		mt7921_mcu_set_tx(struct mwx_softc *, struct mwx_vif *);
int		mt7925_mcu_set_tx(struct mwx_softc *, struct mwx_vif *);
int		mwx_mcu_set_hif_suspend(struct mwx_softc *, int);
int		mt7921_mac_fill_rx(struct mwx_softc *, struct mbuf *,
		    struct ieee80211_rxinfo *);
int		mt7925_mac_fill_rx(struct mwx_softc *, struct mbuf *,
		    struct ieee80211_rxinfo *);
uint32_t	mt7921_mac_tx_rate_val(struct mwx_softc *);
void		mt7921_mac_write_txwi_80211(struct mwx_softc *, struct mbuf *,
		    struct ieee80211_node *, struct mt76_txwi *);
void		mt7921_mac_write_txwi(struct mwx_softc *, struct mbuf *,
		    struct ieee80211_node *, struct mt76_txwi *);
void		mt7925_mac_write_txwi_80211(struct mwx_softc *, struct mbuf *,
		    struct ieee80211_node *, struct mt76_txwi *);
void		mt7925_mac_write_txwi(struct mwx_softc *, struct mbuf *,
		    struct ieee80211_node *, struct mt76_txwi *);
void		mt7921_mac_tx_free(struct mwx_softc *, struct mbuf *);
void		mt7925_mac_tx_free(struct mwx_softc *, struct mbuf *);
int		mt7921_set_channel(struct mwx_softc *);
int		mt7925_set_channel(struct mwx_softc *);
int		mt7925_mcu_add_bss_info(struct mwx_softc *,
		    struct ieee80211_node *, int);

uint8_t		 mt7921_get_phy_mode_v2(struct mwx_softc *,
		    struct ieee80211_node *);
struct mbuf	*mwx_alloc_sta_req_tlv(int);
caddr_t		mwx_append_len(struct mbuf *, int);
void		*mwx_append_tlv(struct mbuf *, uint16_t *, int, int);
void		 mt7921_mcu_add_basic_tlv(struct mbuf *, uint16_t *,
		    struct mwx_softc *, struct ieee80211_node *, int, int);
void		 mt7921_mcu_add_sta_tlv(struct mbuf *, uint16_t *,
		    struct mwx_softc *, struct ieee80211_node *, int, int);
int		 mt7921_mcu_wtbl_generic_tlv(struct mbuf *, uint16_t *,
		    struct mwx_softc *, struct ieee80211_node *);
int		 mt7921_mcu_wtbl_hdr_trans_tlv(struct mbuf *, uint16_t *,
		    struct mwx_softc *, struct ieee80211_node *);
int		 mt7921_mcu_wtbl_ht_tlv(struct mbuf *, uint16_t *,
		    struct mwx_softc *, struct ieee80211_node *);
int		 mt7921_mac_sta_update(struct mwx_softc *,
		    struct ieee80211_node *, int, int);
int		 mt7925_mac_sta_update(struct mwx_softc *,
		    struct ieee80211_node *, int, int);
void		 mt7921_mcu_add_key_tlv(struct mbuf *, uint16_t *,
		    struct ieee80211_key *, int);
void		 mt7925_mcu_add_key_tlv(struct mbuf *, uint16_t *,
		    struct ieee80211_key *, struct ieee80211_node *,
		    uint16_t, int);
int		 mwx_mcu_sta_key_update(struct mwx_softc *,
		    struct ieee80211_node *, struct ieee80211_key *);
void		 mwx_mcu_sta_key_delete(struct mwx_softc *,
		    struct ieee80211_node *, struct ieee80211_key *);

static inline uint32_t
mwx_read(struct mwx_softc *sc, uint32_t reg)
{
	reg = mwx_reg_addr(sc, reg);
	return bus_space_read_4(sc->sc_st, sc->sc_memh, reg);
}

static inline void
mwx_write(struct mwx_softc *sc, uint32_t reg, uint32_t val)
{
	reg = mwx_reg_addr(sc, reg);
	bus_space_write_4(sc->sc_st, sc->sc_memh, reg, val);
}

static inline void
mwx_barrier(struct mwx_softc *sc)
{
	bus_space_barrier(sc->sc_st, sc->sc_memh, 0, sc->sc_mems,
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);
}

static inline uint32_t
mwx_rmw(struct mwx_softc *sc, uint32_t reg, uint32_t val, uint32_t mask)
{
	reg = mwx_reg_addr(sc, reg);
	val |= bus_space_read_4(sc->sc_st, sc->sc_memh, reg) & ~mask;
	bus_space_write_4(sc->sc_st, sc->sc_memh, reg, val);
	return val;
}

static inline uint32_t
mwx_set(struct mwx_softc *sc, uint32_t reg, uint32_t bits)
{
	return mwx_rmw(sc, reg, bits, 0);
}

static inline uint32_t
mwx_clear(struct mwx_softc *sc, uint32_t reg, uint32_t bits)
{
	return mwx_rmw(sc, reg, 0, bits);
}

static inline uint32_t
mt7921_map_reg_l1(struct mwx_softc *sc, uint32_t reg)
{
	uint32_t offset = MWX_HIF_REG_OFFSET(reg);
	uint32_t base = MWX_HIF_REG_BASE(reg) << MT7921_HIF_REMAP_L1_SHIFT;

	mwx_rmw(sc, MT7921_HIF_REMAP_L1, base, MT7921_HIF_REMAP_L1_MASK);
	mwx_barrier(sc);

	return MT7921_HIF_REMAP_BASE_L1 + offset;
}

static inline uint32_t
mt7925_map_reg_l1(struct mwx_softc *sc, uint32_t reg)
{
	uint32_t offset = MWX_HIF_REG_OFFSET(reg);
	uint32_t base = MWX_HIF_REG_BASE(reg) << MT7925_HIF_REMAP_L1_SHIFT;

	mwx_rmw(sc, MT7925_HIF_REMAP_L1, base, MT7925_HIF_REMAP_L1_MASK);
	mwx_barrier(sc);

	return MT7925_HIF_REMAP_BASE_L1 + offset;
}

static inline u_int
mwx_dma_outstanding(struct mwx_queue *q)
{
	return (q->mq_prod + q->mq_count - q->mq_cons) % q->mq_count;
}

static inline int
mwx_dma_available_slots(struct mwx_queue *q)
{
	u_int used = mwx_dma_outstanding(q);

	/*
	 * mq_prod is written by the producer and mq_cons by the consumer
	 * only, so this is a single-producer/single-consumer ring and the
	 * free count is exact without a shared counter.  A separate
	 * "queued" counter cannot be used here: the producer and the
	 * consumer run on different threads, so the increment and the
	 * decrement would race and the ring would drift out of sync.
	 */
	if (used >= q->mq_count - 1)
		return 0;
	return q->mq_count - 1 - used;
}

/*
 * Poll for timeout milliseconds or until register reg read the value val
 * after applying the mask to the value read. Returns 0 on success ETIMEDOUT
 * on failure.
 */
int
mwx_poll(struct mwx_softc *sc, uint32_t reg, uint32_t val, uint32_t mask,
    int timeout)
{
	uint32_t cur;

	reg = mwx_reg_addr(sc, reg);
	timeout *= 10;
	do {
		cur = bus_space_read_4(sc->sc_st, sc->sc_memh, reg) & mask;
		if (cur == val)
			return 0;
		delay(100);
	} while (timeout-- > 0);

	DPRINTF("%s: poll timeout reg %x val %x mask %x cur %x\n",
	    DEVNAME(sc), reg, val, mask, cur);
	return ETIMEDOUT;
}

/*
 * ifp specific functions
 */
int
mwx_init(struct ifnet *ifp)
{
	struct mwx_softc *sc = (struct mwx_softc *)ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct mwx_node *mn;
	int rv, tries;

	if (!sc->sc_fw_loaded) {
		rv = mwx_preinit(sc);
		if (rv)
			return rv;
	}

	DPRINTF("%s: init\n", DEVNAME(sc));
	mwx_mcu_set_deep_sleep(sc, 0);

	if (sc->sc_hwtype != MWX_HW_MT7925) {
		rv = mt7921_mcu_set_mac_enable(sc, 0, 1);
		if (rv)
			return rv;
	}

	if (sc->sc_hwtype == MWX_HW_MT7925)
		rv = mt7925_mcu_set_channel_domain(sc);
	else
		rv = mt7921_mcu_set_channel_domain(sc);
	if (rv)
		return rv;

#if 0
	/* XXX no channel available yet */
	rv = mt7921_mcu_set_chan_info(sc, MCU_EXT_CMD_SET_RX_PATH);
	if (rv)
		return rv;
#endif

	if (sc->sc_hwtype != MWX_HW_MT7925) {
		rv = mt7921_mcu_set_rate_txpower(sc);
		if (rv)
			return rv;
	}

	mwx_mac_reset_counters(sc);

	mn = &sc->sc_vif.vif_mn;

	mwx_mac_wtbl_update(sc, mn->wcid);
	rv = mwx_mcu_uni_add_dev(sc, &sc->sc_vif, mn, 1);
	if (rv)
		return rv;

	if (sc->sc_hwtype == MWX_HW_MT7925)
		rv = mt7925_mcu_set_tx(sc, &sc->sc_vif);
	else
		rv = mt7921_mcu_set_tx(sc, &sc->sc_vif);
	if (rv)
		return rv;

	if (ic->ic_opmode == IEEE80211_M_MONITOR) {
		if (sc->sc_hwtype != MWX_HW_MT7925) {
			rv = mt7921_mcu_set_chan_info(sc,
			    MCU_EXT_CMD_SET_RX_PATH);
			if (rv)
				return rv;
			rv = mt7921_set_channel(sc);
			if (rv)
				return rv;
		}

		rv = mwx_mcu_set_sniffer(sc, 1);
		if (rv)
			return rv;

		if (sc->sc_hwtype == MWX_HW_MT7925) {
			rv = mt7925_mcu_config_sniffer(sc);
		} else {
			rv = mt7921_mcu_set_beacon_filter(sc, 0);
		}
		if (rv)
			return rv;

		mwx_set(sc, MT_DMA_DCR0(0), MT_DMA_DCR0_RXD_G5_EN);
	} else {
		// Linux MT7925 does not send SNIFFER during normal STA bring-up
		// XXX mwx_mcu_set_sniffer(sc, 0);
		mwx_clear(sc, MT_DMA_DCR0(0), MT_DMA_DCR0_RXD_G5_EN);
	}

	ifp->if_flags |= IFF_RUNNING;
	ifq_clr_oactive(&ifp->if_snd);

	if (ic->ic_opmode == IEEE80211_M_MONITOR) {
		ic->ic_bss->ni_chan = ic->ic_ibss_chan;
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
		return 0;
	}

	ieee80211_begin_scan(ifp);

	/*
	 * ieee80211_begin_scan() ends up scheduling mwx_newstate_task().
	 * Wait until the transition to SCAN state has completed.
	 *
	 * Bounded: the wait inherits the workloop (we are inside enable()),
	 * so if the state machine never gets to run this would otherwise
	 * spin once a second forever and hang the boot.
	 */
	for (tries = 0; ic->ic_state != IEEE80211_S_SCAN; tries++) {
		if (tries >= 20) {
			printf("%s: scan state never reached (state %d)\n",
			    DEVNAME(sc), ic->ic_state);
			mwx_stop(ifp);
			return ETIMEDOUT;
		}
		rv = mwx_tsleep_nsec(&ic->ic_state, PCATCH, "mwxinit",
		    SEC_TO_NSEC(1));
		if (rv) {
			mwx_stop(ifp);
			return rv;
		}
	}

	return 0;
}

void
mwx_stop(struct ifnet *ifp)
{
	struct mwx_softc *sc = (struct mwx_softc *)ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int s = splnet();
	int was_running = (ifp->if_flags & IFF_RUNNING) != 0;

	DPRINTF("%s: stop\n", DEVNAME(sc));

	/* Cancel scheduled tasks and let any stale tasks finish up. */
	task_del(sc->sc_nswq, &sc->sc_newstate_task);
	task_del(sc->sc_nswq, &sc->sc_bgscan_done_task);
	task_del(sc->sc_nswq, &sc->sc_scan_task);
	task_del(sc->sc_nswq, &sc->sc_bmiss_task);
	task_del(sc->sc_nswq, &sc->sc_reset_task);
	timeout_del(&sc->sc_reset_to);

	/* XXX need a barrier here */

	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_RUNNING;
	ifq_clr_oactive(&ifp->if_snd);


	/* reset soft state */
	if (was_running) {
		struct mwx_node *mn;

		mwx_free(sc->sc_bgscan_arg);
		sc->sc_bgscan_arg = NULL;
		sc->sc_bgscan_arg_size = 0;
		if (sc->sc_flags & MWX_FLAG_SCANNING) {
			if (sc->sc_hwtype == MWX_HW_MT7925)
				mt7925_mcu_hw_scan_cancel(sc);
			else
				mt7921_mcu_hw_scan_cancel(sc);
		}

		/*
		 * The scan bookkeeping is ours, not the firmware's, and it
		 * must not survive an interface restart.  A leftover
		 * IEEE80211_F_BGSCAN makes net80211 refuse every later
		 * background and cache scan, and a leftover scanning flag
		 * makes the state machine think a scan is still running.
		 */
		sc->sc_flags &= ~(MWX_FLAG_SCANNING | MWX_FLAG_BGSCAN);
		ic->ic_flags &= ~IEEE80211_F_BGSCAN;
		sc->sc_scan_start = 0;

		mn = &sc->sc_vif.vif_mn;
		mwx_mcu_uni_add_dev(sc, &sc->sc_vif, mn, 0);
		mwx_mcu_set_deep_sleep(sc, 1);
		if (sc->sc_hwtype != MWX_HW_MT7925) {
			if (sc->sc_fw_loaded && was_running)
				mt7921_mcu_set_mac_enable(sc, 0, 0);
		}

		/* XXX anything more ??? */
		/* check out mt7921e_mac_reset, mt7921e_unregister_device and
		   mt7921_pci_suspend
		 */
	}

	sc->sc_newstate(ic, IEEE80211_S_INIT, -1);
	sc->sc_ns_state = IEEE80211_S_INIT;
	ifp->if_timer = 0;

	splx(s);
}

void
mwx_watchdog(struct ifnet *ifp)
{
	struct mwx_softc *sc = (struct mwx_softc *)ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	ifp->if_timer = 0;

	/*
	 * Completions are normally taken from the interrupt, but they must
	 * not depend on it.  If the MSI for a completion is lost its status
	 * bit stays set, and while it is set the PCIe block raises no
	 * further edge for that source: nothing would ever drain that ring
	 * again.  This timer runs off its own clock, so it is the one path
	 * that always makes progress.  The poll is a handful of register
	 * reads when no descriptor has completed.
	 */
	mwx_dma_poll(sc);

	/*
	 * The scan-done event is unsolicited and can be lost.  If that
	 * happens while associated, nothing else is guaranteed to run:
	 * airportd's next cache scan is refused (it must not take the radio
	 * off-channel), the firmware's beacon-loss event may be lost too,
	 * and net80211 has no timer for a scan it believes is still in
	 * progress.  Notice it here and let the beacon-miss task put the
	 * radio back on the BSS channel.  The check is two comparisons when
	 * no scan is pending.
	 */
	if (mwx_scan_check_stale(sc, mwx_uptime_ms()) &&
	    ic->ic_state == IEEE80211_S_RUN)
		task_add(sc->sc_nswq, &sc->sc_bmiss_task);

	ieee80211_watchdog(ifp);
}

void
mwx_start(struct ifnet *ifp)
{
	struct mwx_softc *sc = (struct mwx_softc *)ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct ether_header *eh;
	struct mbuf *m;

	if (!(ifp->if_flags & IFF_RUNNING) || ifq_is_oactive(&ifp->if_snd))
		return;

	/*
	 * Cheap recovery point for a completion whose interrupt was not
	 * delivered: the transmit path is driven for every outgoing
	 * frame, so a status bit left pending by a lost MSI (which also
	 * suppresses further edges) is cleared here and the rings drained
	 * before more work is queued.
	 */
	mwx_dma_poll(sc);

	for (;;) {
		/* XXX TODO handle oactive
			ifq_set_oactive(&ifp->if_snd);
		*/

		/* need to send management frames even if we're not RUNning */
		m = mq_dequeue(&ic->ic_mgtq);
		if (m) {
			ni = (struct ieee80211_node *)mbuf_pkthdr_rcvif(m);
			goto sendit;
		}

		if (ic->ic_state != IEEE80211_S_RUN ||
		    (ic->ic_xflags & IEEE80211_F_TX_MGMT_ONLY))
			break;

		m = ifq_dequeue(&ifp->if_snd);
		if (!m)
			break;
		if (mbuf_len(m) < sizeof (*eh) &&
		    (m = m_pullup(m, sizeof (*eh))) == NULL) {
			ifp->if_oerrors++;
			continue;
		}
#if NBPFILTER > 0
		if (ifp->if_bpf != NULL)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif
		if ((m = ieee80211_encap(ifp, m, &ni)) == NULL) {
			ifp->if_oerrors++;
			continue;
		}

 sendit:
#if NBPFILTER > 0
		if (ic->ic_rawbpf != NULL)
			bpf_mtap(ic->ic_rawbpf, m, BPF_DIRECTION_OUT);
#endif
		{
			int txrv;

			txrv = mwx_tx(sc, m, ni);
			if (txrv != 0) {
				DPRINTF("%s: mwx_tx failed: %d\n",
				    DEVNAME(sc), txrv);
				ieee80211_release_node(ic, ni);
				ifp->if_oerrors++;
				continue;
			}
		}

		if (ifp->if_flags & IFF_UP)
			ifp->if_timer = 1;
	}
}

int
mwx_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	int s, err = 0;

	s = splnet();
	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		/* FALLTHROUGH */
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_flags & IFF_RUNNING)) {
				mwx_stop(ifp);
				err = mwx_init(ifp);
			}
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				mwx_stop(ifp);
		}
		break;
	default:
		err = ieee80211_ioctl(ifp, cmd, data);
		break;
	}

	if (err == ENETRESET) {
		err = 0;
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING)) {
			mwx_stop(ifp);
			err = mwx_init(ifp);
		}
	}
	splx(s);
	return err;
}

int
mwx_media_change(struct ifnet *ifp)
{
	struct mwx_softc *sc = (struct mwx_softc *)ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int err;

	err = ieee80211_media_change(ifp);
	if (err != ENETRESET)
		return err;

	/* TODO lot more handling here */
	if (ic->ic_fixed_mcs != -1) {
		;
	} else if (ic->ic_fixed_rate != -1) {
		;
	}
	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
	    (IFF_UP | IFF_RUNNING)) {
		/* XXX could be a bit harsh */
		mwx_stop(ifp);
		err = mwx_init(ifp);
	}
	return err;
}

/*
 * net80211 specific functions.
 */

struct ieee80211_node *
mwx_node_alloc(struct ieee80211com *ic)
{
	/* XXX this is just wrong */
	static uint32_t wcid = 0;
	struct mwx_softc *sc = (struct mwx_softc *)ic->ic_softc;
	struct mwx_node *mn;

	mn = (struct mwx_node *)malloc(sizeof(struct mwx_node), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (mn == NULL)
		return NULL;
	mn->wcid = 1 + wcid++ % (MWX_WTBL_STA - 1);

	/* init WCID table entry */
	mwx_mac_wtbl_update(sc, mn->wcid);

	return &mn->ni;
}

void
mwx_newassoc(struct ieee80211com *ic, struct ieee80211_node *ni, int isnew)
{
	struct mwx_softc *sc = (struct mwx_softc *)ic->ic_softc;
	struct mwx_node *mn = (struct mwx_node *)ni;
	uint16_t wcid = 0;

	if (isnew && ni->ni_associd != 0) {
		/* only interested in true associations */
		wcid = IEEE80211_AID(ni->ni_associd);

	}
	DPRINTF("%s: new assoc isnew=%d addr=%s WCID=%d\n", DEVNAME(sc),
	    isnew, ether_sprintf(ni->ni_macaddr), mn->wcid);

	/* XXX TODO rate handling here */
}

#ifndef IEEE80211_STA_ONLY
void
mwx_node_leave(struct ieee80211com *ic, struct ieee80211_node *ni)
{
#if 0
	struct mwx_softc *sc = (struct mwx_softc *)ic->ic_softc;
	struct mwx_node *mn = (struct mwx_node *)ni;
	uint16_t wcid = mn->wcid;

	mwx_mac_wtbl_update(sc, wcid);
#endif
}
#endif

/*
 * Milliseconds of monotonic uptime.  Used to pace background scans; the
 * kernel stores uptime in absolute-time units, so convert it once here
 * instead of comparing raw ticks with different bases.
 */
static uint64_t
mwx_uptime_ms(void)
{
	uint64_t now, ns;

	clock_get_uptime(&now);
	absolutetime_to_nanoseconds(now, &ns);
	return ns / 1000000ULL;
}

/* Keep the radio on the associated channel this long after link-up. */
#define	MWX_BGSCAN_HOLD_MS		15000
/* Minimum spacing between two accepted background scans. */
#define	MWX_BGSCAN_MIN_INTERVAL_MS	20000

/*
 * Give up on a scan whose completion never arrived.
 *
 * A scan is flagged busy with MWX_FLAG_SCANNING until the firmware sends
 * the scan-done event, and both mwx_scan() and the S_SCAN transition
 * treat that flag as "a scan is already running, do not start another".
 * The scan-done event is as unsolicited as a beacon-loss event, so it can
 * be lost; without this check the interface would then refuse to scan
 * forever, i.e. it would stay in S_SCAN (or never leave RUN) with no scan
 * in flight and no association.  All channels of an active scan take a
 * few seconds on this hardware, so anything older than this is lost, not
 * slow.
 *
 * Returns non-zero when a scan was abandoned.  The caller is responsible
 * for the radio: net80211 will not select a channel for a scan it thinks
 * is still running, so when this is called in RUN the BSS channel has to
 * be programmed again (see mwx_bmiss_task()).
 */
#define	MWX_SCAN_STALE_MS		15000

static int
mwx_scan_check_stale(struct mwx_softc *sc, uint64_t now)
{
	struct ieee80211com *ic = &sc->sc_ic;

	if ((sc->sc_flags & MWX_FLAG_SCANNING) == 0)
		return 0;
	if (sc->sc_scan_start != 0 &&
	    now - sc->sc_scan_start < MWX_SCAN_STALE_MS)
		return 0;

	printf("%s: scan did not complete%s, giving up on it\n", DEVNAME(sc),
	    sc->sc_scan_start != 0 ? " within 15 s" : " (start time lost)");
	sc->sc_flags &= ~(MWX_FLAG_SCANNING | MWX_FLAG_BGSCAN);

	/*
	 * net80211 records a background scan with IEEE80211_F_BGSCAN when
	 * the driver reports success, and only the driver's completion can
	 * clear it.  Once that completion is lost the flag has to be
	 * cleared here, otherwise both ieee80211_begin_bgscan() and
	 * ieee80211_begin_cache_bgscan() refuse every later scan: the
	 * interface could never roam or rejoin again.
	 */
	ic->ic_flags &= ~IEEE80211_F_BGSCAN;

	return 1;
}

int
mwx_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct mwx_softc *sc = (struct mwx_softc *)ic->ic_softc;

	/*
	 * Prevent attempts to transition towards the same state, unless
	 * we are scanning in which case a SCAN -> SCAN transition
	 * triggers another scan iteration. And AUTH -> AUTH is needed
	 * to support band-steering.
	 */
	if (sc->sc_ns_state == nstate && nstate != IEEE80211_S_SCAN &&
	    nstate != IEEE80211_S_AUTH)
		return 0;

	if (ic->ic_state == IEEE80211_S_RUN) {
		/* cancel other tasks here */
	}

	sc->sc_ns_state = nstate;
	sc->sc_ns_arg = arg;

	task_add(sc->sc_nswq, &sc->sc_newstate_task);
	return 0;
}

void
mwx_newstate_task(void *ptr)
{
	struct mwx_softc *sc = (struct mwx_softc *)ptr;
	struct ieee80211com *ic = &sc->sc_ic;
	enum ieee80211_state ostate = ic->ic_state;
	enum ieee80211_state nstate = sc->sc_ns_state;
	int arg = sc->sc_ns_arg;
	int s;
	int rv = 0;

	s = splnet();

	switch (ostate) {
	case IEEE80211_S_RUN:
		if (nstate != ostate)
			mwx_mcu_set_deep_sleep(sc, 1);
		break;
	case IEEE80211_S_SCAN:
		if (nstate == ostate) {
			/*
			 * Another scan iteration.  Refuse only while a
			 * scan is really running; a completion that was
			 * never delivered must not park the interface
			 * here forever.
			 */
			mwx_scan_check_stale(sc, mwx_uptime_ms());
			if (sc->sc_flags & MWX_FLAG_SCANNING) {
				splx(s);
				return;
			}
		}
		break;
	default:
		break;
	}

	DPRINTF("%s: %s %d -> %d\n", DEVNAME(sc), __func__, ostate, nstate);

	switch (nstate) {
	case IEEE80211_S_INIT:
		break;
	case IEEE80211_S_SCAN:
		rv = mwx_scan(sc);
		if (rv)
			break;
		splx(s);
		return;
	case IEEE80211_S_AUTH:
		if (ostate != IEEE80211_S_AUTH) {
			if (sc->sc_hwtype == MWX_HW_MT7925)
				rv = mt7925_set_channel(sc);
			else
				rv = mt7921_set_channel(sc);
			if (rv)
				break;
			mwx_mcu_set_deep_sleep(sc, 0);
			if (sc->sc_hwtype == MWX_HW_MT7925)
				mt7925_mac_sta_update(sc, sc->sc_ic.ic_bss, 1,
				    1);
			else
				mt7921_mac_sta_update(sc, sc->sc_ic.ic_bss, 1,
				    1);
		}
		break;
	case IEEE80211_S_ASSOC:
		break;
	case IEEE80211_S_RUN:
		if (ic->ic_opmode == IEEE80211_M_MONITOR)
			break;

		if (sc->sc_hwtype == MWX_HW_MT7925)
			mt7925_mcu_hw_scan_cancel(sc); /* XXX */
		else
			mt7921_mcu_hw_scan_cancel(sc); /* XXX */
		mwx_mcu_set_deep_sleep(sc, 0);

		/*
		 * Association is complete.  Publish the BSSID, the beacon
		 * parameters and the real association ID to the firmware
		 * before net80211 starts the 4-way handshake and marks the
		 * port valid: it is the notification that turns the BSS
		 * the interface was added with (no BSSID, AID 0) into the
		 * associated BSS the firmware has to transmit and receive
		 * for.  See mwx_mcu_uni_add_bss().
		 */
		if (sc->sc_hwtype == MWX_HW_MT7925)
			rv = mt7925_mcu_add_bss_info(sc, ic->ic_bss, 1);
		else
			rv = mwx_mcu_uni_add_bss(sc, &sc->sc_vif, ic->ic_bss,
			    &sc->sc_vif.vif_mn, 1);
		if (rv != 0)
			break;
		if (sc->sc_hwtype == MWX_HW_MT7925)
			rv = mt7925_mac_sta_update(sc, ic->ic_bss, 1, 0);
		else
			rv = mt7921_mac_sta_update(sc, ic->ic_bss, 1, 0);
		if (rv != 0) {
			printf("%s: %s: association notification failed: %d\n",
			    DEVNAME(sc), __func__, rv);
			break;
		}

		if (sc->sc_hwtype == MWX_HW_MT7925)
			mt7925_mcu_set_rts_thresh(sc, 0x92b);
		else
			mt7921_mcu_set_rts_thresh(sc, 0x92b, 0);

		/*
		 * Give DHCP the radio until the lease is in.  See the
		 * pacing comment in mwx_bgscan().
		 */
		sc->sc_bgscan_last = 0;
		sc->sc_bgscan_hold = mwx_uptime_ms() + MWX_BGSCAN_HOLD_MS;
		break;
	}

	if (rv) {
		mwx_reset(sc);
	} else {
		sc->sc_newstate(ic, nstate, arg);
	}
	splx(s);
}

int
mwx_scan(struct mwx_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	int rv;

	mwx_scan_check_stale(sc, mwx_uptime_ms());

	if (sc->sc_flags & MWX_FLAG_BGSCAN) {
		if (sc->sc_hwtype == MWX_HW_MT7925)
			rv = mt7925_mcu_hw_scan_cancel(sc);
		else
			rv = mt7921_mcu_hw_scan_cancel(sc);
		if (rv) {
			printf("%s: could not abort background scan\n",
			    DEVNAME(sc));
			/*
			 * Do not leave the radio half-way into a scan
			 * because the cancel command failed.  The flags
			 * are ours, not the firmware's, and the scan
			 * request below re-programs the channel anyway.
			 */
			sc->sc_flags &= ~(MWX_FLAG_SCANNING | MWX_FLAG_BGSCAN);
		}
	}

	if (sc->sc_hwtype == MWX_HW_MT7925)
		rv = mt7925_mcu_hw_scan(sc, 0);
	else
		rv = mt7921_mcu_hw_scan(sc, 0);
	if (rv) {
		printf("%s: could not initiate scan\n", DEVNAME(sc));
		return rv;
	}

	/*
	 * The current mode might have been fixed during association.
	 * Ensure all channels get scanned.
	 */
	if (IFM_MODE(ic->ic_media.ifm_cur->ifm_media) != IFM_AUTO)
		ieee80211_setmode(ic, IEEE80211_MODE_AUTO);

	if (ifp->if_flags & IFF_DEBUG)
		printf("%s: %s -> %s\n", ifp->if_xname,
		    ieee80211_state_name[ic->ic_state],
		    ieee80211_state_name[IEEE80211_S_SCAN]);
	if ((sc->sc_flags & MWX_FLAG_BGSCAN) == 0) {
		ieee80211_set_link_state(ic, LINK_STATE_DOWN);
		ieee80211_node_cleanup(ic, ic->ic_bss);
	}
	ic->ic_state = IEEE80211_S_SCAN;
	wakeup(&ic->ic_state);

	return 0;
}

int
mwx_bgscan(struct ieee80211com *ic)
{
	struct mwx_softc *sc = (struct mwx_softc *)ic->ic_softc;
	uint64_t now = mwx_uptime_ms();
	int err;

	/*
	 * A cache scan (ieee80211_begin_cache_bgscan()) is airportd asking
	 * for an updated list of surrounding APs.  It sets
	 * IEEE80211_F_DISABLE_BG_AUTO_CONNECT just before calling us, which
	 * is how it tells the 802.11 layer not to roam onto the results --
	 * and is also how we tell this request apart from the RSSI-driven
	 * roaming scan, which needs the radio.
	 *
	 * Scanning here is not harmless: the firmware leaves the BSS for
	 * every channel of the list, so a request every few seconds keeps
	 * the radio away from the AP for most of the time and starves
	 * DHCP.  Worse, a cache scan that ends without net80211 switching
	 * BSS (that is the point of a cache scan) leaves the radio parked
	 * on the last scanned channel: net80211 stays in RUN, nothing
	 * re-selects the channel, the firmware stops hearing the AP and
	 * the interface looks connected while passing no traffic.  Serve
	 * the request from the node cache instead.  The caller still gets
	 * its (cached) results and a scan-done notification, and the flag
	 * is cleared again because no scan is pending to clear it later;
	 * leaving it set would disable roaming for good.
	 *
	 * This test has to come before the scan-in-progress test below:
	 * returning success without starting a scan would make
	 * ieee80211_begin_cache_bgscan() set IEEE80211_F_BGSCAN, and the
	 * in-flight scan would then finish as a cache scan and leave the
	 * radio off-channel.
	 */
	if (ic->ic_flags & IEEE80211_F_DISABLE_BG_AUTO_CONNECT) {
		ic->ic_flags &= ~IEEE80211_F_DISABLE_BG_AUTO_CONNECT;
		return EBUSY;
	}

	/*
	 * A scan is already running.
	 *
	 * Do not report success: net80211 would set IEEE80211_F_BGSCAN and
	 * wait for this driver to end a scan it never started.  The running
	 * scan will report its own completion.
	 */
	mwx_scan_check_stale(sc, now);
	if (sc->sc_flags & MWX_FLAG_SCANNING)
		return EBUSY;

	/*
	 * Pace the roaming scans that remain.
	 *
	 * A background scan is not an off-channel scan on this hardware:
	 * the firmware parks the MAC on each scanned channel for the whole
	 * dwell, so while it runs nothing is received or transmitted on the
	 * associated channel.  net80211 starts one whenever a beacon is
	 * missed (the RSSI timer), and a burst of those would keep the
	 * radio away from the AP just like the airportd cache scans did.
	 * Give the link a quiet window after association and space the
	 * scans out afterwards.
	 *
	 * Returning an error makes net80211 skip the scan (it only sets
	 * IEEE80211_F_BGSCAN when the driver reports success); the RSSI
	 * check runs again after the next beacon, so a refused scan is
	 * retried later instead of being lost.
	 */
	if (now < sc->sc_bgscan_hold ||
	    (sc->sc_bgscan_last != 0 &&
	     now - sc->sc_bgscan_last < MWX_BGSCAN_MIN_INTERVAL_MS))
		return EBUSY;

	if (sc->sc_hwtype == MWX_HW_MT7925)
		err = mt7925_mcu_hw_scan(sc, 1);
	else
		err = mt7921_mcu_hw_scan(sc, 1);
	if (err) {
		printf("%s: could not initiate scan\n", DEVNAME(sc));
		return err;
	}

	if (DEVDEBUG(sc))
		printf("%s: begin background scan\n", DEVNAME(sc));
	sc->sc_bgscan_last = now;
	sc->sc_flags |= MWX_FLAG_BGSCAN;
	return 0;
}

void
mwx_bgscan_done(struct ieee80211com *ic,
    struct ieee80211_node_switch_bss_arg *arg, size_t arg_size)
{
	struct mwx_softc *sc = (struct mwx_softc *)ic->ic_softc;

	mwx_free(sc->sc_bgscan_arg);
	sc->sc_bgscan_arg = arg;
	sc->sc_bgscan_arg_size = arg_size;
	task_add(sc->sc_nswq, &sc->sc_bgscan_done_task);
}

void
mwx_bgscan_done_task(void *arg)
{
	struct mwx_softc *sc = (struct mwx_softc *)arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = ic->ic_bss;
	int err = 0;
	int s = splnet();

	if (ic->ic_state != IEEE80211_S_RUN ||
	    (ic->ic_flags & IEEE80211_F_BGSCAN) == 0) {
		err = ENXIO;
		goto done;
	}

	/*
	 * Remove installed crypto keys while we still have access to them.
	 * Once mwx_newstate() is entered ic_bss will already contain
	 * information about our next AP.
	 */
	if (ic->ic_flags & IEEE80211_F_RSNON) {
		struct ieee80211_key *k;

#ifdef NOTYET
		if (ic->ic_bss->ni_flags & IEEE80211_NODE_MFP)
			mwx_mfp_leave(sc);
#endif

		if (ni->ni_pairwise_key.k_cipher != IEEE80211_CIPHER_NONE)
			(*ic->ic_delete_key)(ic, ni, &ni->ni_pairwise_key);

		if (ic->ic_def_txkey == 1 || ic->ic_def_txkey == 2) {
			k = &ic->ic_nw_keys[ic->ic_def_txkey];
			if ((k->k_flags & IEEE80211_KEY_GROUP) &&
			    k->k_cipher == IEEE80211_CIPHER_CCMP)
				(*ic->ic_delete_key)(ic, ni, k);
		}

		if (ic->ic_igtk_kid == 4 || ic->ic_igtk_kid == 5) {
			k = &ic->ic_nw_keys[ic->ic_igtk_kid];
			if (k->k_flags & IEEE80211_KEY_IGTK)
				(*ic->ic_delete_key)(ic, ni, k);
		}

		ni->ni_port_valid = 0;
		ni->ni_flags &= ~IEEE80211_NODE_TXRXPROT;
		ni->ni_flags &= ~IEEE80211_NODE_TXMGMTPROT;
		ni->ni_flags &= ~IEEE80211_NODE_RXMGMTPROT;
		ni->ni_rsn_supp_state = RSNA_SUPP_INITIALIZE;
	}

	/*
	 * Tx queues have been flushed and Tx agg has been stopped.
	 * Allow roaming to proceed.
	 */
	ni->ni_unref_arg = sc->sc_bgscan_arg;
	ni->ni_unref_arg_size = sc->sc_bgscan_arg_size;
	sc->sc_bgscan_arg = NULL;
	sc->sc_bgscan_arg_size = 0;
	if (ni->ni_flags & IEEE80211_NODE_MFP) {
		/* Deauth frame already sent. */
		ieee80211_node_switch_bss(ic, ni);
	} else {
#ifndef __APPLE__
		ieee80211_node_tx_stopped(ic, ni);
#endif
	}

 done:
	if (err) {
		mwx_free(sc->sc_bgscan_arg);
		sc->sc_bgscan_arg = NULL;
		sc->sc_bgscan_arg_size = 0;
	}
	splx(s);
}

int
mwx_set_key(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct ieee80211_key *k)
{
	struct mwx_softc *sc = (struct mwx_softc *)ic->ic_softc;
	int rv;

	DPRINTF("%s: set_key: ni %p, k_id %d, k_flags %x k_cipher %d\n",
	    DEVNAME(sc), ni, k->k_id, k->k_flags, k->k_cipher);

	/*
	 * Install the key synchronously, including the wait for the
	 * firmware reply.
	 *
	 * This runs from the receive path while the 4-way handshake is
	 * being processed, but the reply does not have to be delivered by
	 * an interrupt: mwx_mcu_wait_resp_int() services the receive rings
	 * itself through mwx_dma_poll(), so the reply is collected by the
	 * very thread that is waiting for it.
	 *
	 * The distinction matters for failure, not for speed.  net80211
	 * marks the port valid and brings the link up when this returns 0,
	 * and deauthenticates when it returns an error.  An installation
	 * that is merely queued always reports success, so a key the
	 * firmware rejected produces a link that reports up, shows full
	 * signal, and drops every frame in both directions: DHCP never
	 * completes and the interface waits for an internet connection
	 * that cannot arrive.  Failing here instead makes the association
	 * fail loudly and retry, with the firmware's status in the log.
	 */
	rv = mwx_mcu_sta_key_update(sc, ni, k);
	if (rv != 0)
		printf("%s: could not install key %d: error %d\n",
		    DEVNAME(sc), k->k_id, rv);

	return rv;
}

void
mwx_delete_key(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct ieee80211_key *k)
{
	struct mwx_softc *sc = (struct mwx_softc *)ic->ic_softc;

	DPRINTF("%s: delete_key: ni %p, k_id %d, k_flags %x k_cipher %d\n",
	    DEVNAME(sc), ni, k->k_id, k->k_flags, k->k_cipher);

	mwx_mcu_sta_key_delete(sc, ni, k);
}

#if NBPFILTER > 0
void
mwx_radiotap_attach(struct mwx_softc *sc)
{
	bpfattach(&sc->sc_drvbpf, &sc->sc_ic.ic_if, DLT_IEEE802_11_RADIO,
	    sizeof(struct ieee80211_frame) + IEEE80211_RADIOTAP_HDRLEN);

	sc->sc_rxtap_len = sizeof sc->sc_rxtapu;
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(MWX_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof sc->sc_txtapu;
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(MWX_TX_RADIOTAP_PRESENT);
}
#endif

int
mwx_tx(struct mwx_softc *sc, struct mbuf *m, struct ieee80211_node *ni)
{
#if 0
	struct mwx_node *mn = (struct mwx_node *)ni;
#endif
	struct mwx_txwi *mt;
	struct mt76_txwi *txp;
	int rv;

	if (mwx_dma_available_slots(&sc->sc_txq) < 1) {
		/*
		 * The ring may be full only because the completion
		 * interrupt was not delivered; drain it here before
		 * dropping the frame.
		 */
		mwx_dma_poll(sc);
		if (mwx_dma_available_slots(&sc->sc_txq) < 1) {
			rv = EBUSY;
			goto fail;
		}
	}

	if ((mt = mwx_txwi_get(sc)) == NULL) {
		/*
		 * A TXWI is only returned by the firmware's TX-done
		 * notification.  A single missed notification therefore
		 * permanently removes one of the limited TXWI slots, and
		 * after enough of them every frame - including the DHCP
		 * exchange - fails here.  Service the rings once and retry
		 * before giving up, exactly like the ring-full case above.
		 */
		mwx_dma_poll(sc);
		if ((mt = mwx_txwi_get(sc)) == NULL) {
			rv = ENOBUFS;
			goto fail;
		}
	}

	/* XXX DMA memory access without BUS_DMASYNC_PREWRITE */
	txp = mt->mt_desc;
	memset(txp, 0, sizeof(*txp));
	if (sc->sc_hwtype == MWX_HW_MT7925)
		mt7925_mac_write_txwi(sc, m, ni, txp);
	else
		mt7921_mac_write_txwi(sc, m, ni, txp);

	rv = mwx_txwi_enqueue(sc, mt, m, ni);
	if (rv != 0) {
		/*
		 * mwx_txwi_enqueue() did not take ownership of the mbuf,
		 * and mt still holds no reference to it, so return the TXWI
		 * slot to the pool (a leaked slot eventually makes every
		 * transmission fail) and free the frame here.
		 */
		mwx_txwi_put(sc, mt);
		goto fail;
	}

	mwx_dma_txwi_enqueue(sc, &sc->sc_txq, mt);
	return 0;

fail:
	/*
	 * mwx_start() releases the node reference and counts the error when
	 * this function fails, so the frame itself has to be freed here.
	 */
	m_freem(m);
	return rv;
}

void
mwx_rx(struct mwx_softc *sc, struct mbuf *m, struct mbuf_list *ml)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_node *ni;
	struct ieee80211_frame *wh;
	struct ieee80211_rxinfo rxi = { 0 };
	int rv;


	if (sc->sc_hwtype == MWX_HW_MT7925)
		rv = mt7925_mac_fill_rx(sc, m, &rxi);
	else
		rv = mt7921_mac_fill_rx(sc, m, &rxi);
	if (rv != 0) {
		/*
		 * Any failure means the mbuf has not been stripped of the
		 * receive descriptor and must not be parsed as a frame; the
		 * -EINVAL returns used to slip through the old "== -1"
		 * test and hand net80211 a descriptor as an 802.11 header.
		 */
		ifp->if_ierrors++;
		m_freem(m);
		return;
	}

#ifdef __APPLE__
	/*
	 * fill_rx() reports dBm (negative).  The macOS net80211 port, like
	 * itlwm, wants a positive quality value relative to ic_max_rssi,
	 * which is also what the Airport ioctl code assumes.
	 */
	if (rxi.rxi_rssi < 0) {
		if (rxi.rxi_rssi < MWX_MIN_DBM)
			rxi.rxi_rssi = MWX_MIN_DBM;	/* no chain data */
		rxi.rxi_rssi += -MWX_MIN_DBM;
		if (rxi.rxi_rssi > ic->ic_max_rssi)
			rxi.rxi_rssi = ic->ic_max_rssi;
	}
#endif

	wh = mtod(m, struct ieee80211_frame *);

#if NBPFILTER > 0
	if (__predict_false(sc->sc_drvbpf != NULL)) {
		struct mwx_rx_radiotap_header *tap = &sc->sc_rxtap;
		uint32_t tsf_lo, tsf_hi;
		/* get timestamp (low and high 32 bits) */
		tsf_hi = 0;
		tsf_lo = 0;
		tap->wr_tsft = htole64(((uint64_t)tsf_hi << 32) | tsf_lo);
		tap->wr_flags = 0;
		tap->wr_rate = 2;	/* XXX */
		tap->wr_chan_freq =
		    htole16(ic->ic_channels[rxi.rxi_chan].ic_freq);
		tap->wr_chan_flags =
		    ic->ic_channels[rxi.rxi_chan].ic_flags;
		tap->wr_dbm_antsignal = rxi.rxi_rssi;
		bpf_mtap_hdr(sc->sc_drvbpf, tap, sc->sc_rxtap_len, m,
		    BPF_DIRECTION_IN);
	}
#endif

	/* grab a reference to the source node */
	ni = ieee80211_find_rxnode(ic, wh);

	/* send the frame to the 802.11 layer */
	ieee80211_inputm(ifp, m, ni, &rxi, ml);

	/* node is no longer needed */
	ieee80211_release_node(ic, ni);
}

/*
 * Driver specific functions.
 */
/*
 * Bound the re-check loop in mwx_dma_service().  Draining a ring takes
 * whole bursts, so one or two rounds clear any backlog; the loop only
 * exists to catch completions that land while the rings are being
 * serviced.
 */
#define	MWX_INTR_ROUNDS		8

/*
 * Drain every ring the device may have completed, and acknowledge the
 * WFDMA status register.
 *
 * Must be called with sc_ring_lock held.
 *
 * Completed receive frames are only collected in sc_rx_pending here; they
 * are handed to net80211 later by mwx_dma_rx_deliver(), after the lock has
 * been dropped.  Nothing in this function may touch net80211 or the
 * network stack: if_input() takes the workloop gate (IOCommandGate::runAction
 * executes the action while holding the workloop's gate lock), and
 * mwx_intr() runs with that gate already held and can be waiting for
 * sc_ring_lock itself.  Taking the gate while holding the ring lock is
 * therefore an ABBA deadlock: the interrupt action waits for the ring lock,
 * the output thread waits for the workloop gate, and neither ever returns.
 * That is a frozen interface (no DHCP traffic, unresponsive Wi-Fi menu),
 * not a slow one.
 *
 * The rings are drained unconditionally instead of being selected from
 * the status register.  A completion whose MSI was lost leaves its
 * status bit set, and while the bit is set the PCIe block raises no
 * further edge for that source: a ring that is only serviced when its
 * bit appears in a fresh status read would then never be looked at
 * again.  One lost RX interrupt is enough to stall the 4-way handshake
 * for tens of seconds (the AP retransmits msg3 into a ring nobody
 * drains) and to hide every DHCP reply afterwards.  Each consumer
 * compares the device index against its own, so draining an empty ring
 * costs one register read and changes nothing.
 */
static void
mwx_dma_service(struct mwx_softc *sc)
{
	uint32_t intr, intr_sw;
	int i;

	/*
	 * Nothing to look at before mwx_dma_alloc() has published every
	 * descriptor ring; a stray vector in that window used to reach
	 * the queue code.  All the rings below are touched unconditionally
	 * (and the queue accounting divides by mq_count), so require the
	 * ones that exist on every chipset.  The MCU receive ring does not
	 * exist on MT7925 and is checked separately.
	 */
	if (sc->sc_txq.mq_desc == NULL || sc->sc_txmcuq.mq_desc == NULL ||
	    sc->sc_txfwdlq.mq_desc == NULL || sc->sc_rxq.mq_desc == NULL ||
	    sc->sc_rxfwdlq.mq_desc == NULL)
		return;

	for (i = 0; i < MWX_INTR_ROUNDS; i++) {
		intr = mwx_read(sc, MT_WFDMA0_HOST_INT_STA);
		if (intr != 0) {
			if (i == 0 && (intr & ~sc->sc_intr_mask))
				printf("%s: unhandled interrupt %08x\n",
				    DEVNAME(sc), intr & ~sc->sc_intr_mask);

			/*
			 * Ack before servicing, as mt76 does.  Do not clear
			 * MT_WFDMA0_HOST_INT_ENA here: the PCIe block raises
			 * an MSI for a completion that sets a status bit
			 * while the source is enabled, so masking ENA around
			 * this handler swallows any completion that arrives
			 * in the window.
			 */
			mwx_write(sc, MT_WFDMA0_HOST_INT_STA, intr);

			if (intr & MT_INT_MCU_CMD) {
				intr_sw = mwx_read(sc, MT_MCU_CMD);
				/* ack MCU2HOST_SW_INT_STA */
				mwx_write(sc, MT_MCU_CMD, intr_sw);
			}
		}

		/* Drain even when no status bit was reported. */
		mwx_dma_tx_done(sc);
		mwx_dma_rx_done(sc, &sc->sc_rxfwdlq);
		if (sc->sc_rxmcuq.mq_desc != NULL)
			mwx_dma_rx_done(sc, &sc->sc_rxmcuq);
		mwx_dma_rx_done(sc, &sc->sc_rxq);

		/*
		 * Refill the receive rings independently of whether a
		 * descriptor completed.  A refill that ran out of mbufs
		 * leaves the ring short, and once the ring stops producing
		 * completions nothing else ever comes back to it: receive
		 * would stay dead for the rest of the association and no
		 * DHCP reply would ever arrive.  Filling an already full
		 * ring is a single comparison.
		 */
		mwx_queue_fill(sc, &sc->sc_rxfwdlq);
		if (sc->sc_rxmcuq.mq_desc != NULL)
			mwx_queue_fill(sc, &sc->sc_rxmcuq);
		mwx_queue_fill(sc, &sc->sc_rxq);

		if (intr == 0)
			break;
	}
}

/*
 * Deliver everything collected by mwx_dma_service() to net80211.
 *
 * The outermost invocation drains both queues and hands data frames to
 * net80211; a nested invocation only dispatches what the caller may be
 * waiting for.  See mwx_dma_rx_process() for why the two are split.
 */
static void
mwx_dma_rx_deliver_run(struct mwx_softc *sc)
{
	struct mbuf_list ml;

	if (sc->sc_rx_delivering) {
		/*
		 * Called from inside net80211: a wait loop in the transmit
		 * or MCU send path polled the rings while a frame was being
		 * processed.
		 *
		 * The MCU replies and TX notifications in this batch have
		 * to be dispatched now -- the caller is sleeping on them --
		 * but data frames must not be handed to the stack
		 * recursively, so mwx_dma_rx_process() defers those and the
		 * outer loop delivers them once this frame is done.
		 */
		mwx_ring_lock(sc);
		ml = sc->sc_rx_pending;
		ml_init(&sc->sc_rx_pending);
		mwx_ring_unlock(sc);

		if (!ml_empty(&ml))
			mwx_dma_rx_process(sc, &ml, 0);
		return;
	}

	sc->sc_rx_delivering = 1;
	for (;;) {
		mwx_ring_lock(sc);
		/*
		 * Frames parked by a nested poll came off the ring before
		 * anything that arrived afterwards, so they go first.
		 */
		ml = sc->sc_rx_deferred;
		ml_init(&sc->sc_rx_deferred);
		if (ml_empty(&ml)) {
			ml = sc->sc_rx_pending;
			ml_init(&sc->sc_rx_pending);
		}
		mwx_ring_unlock(sc);

		if (ml_empty(&ml))
			break;

		mwx_dma_rx_process(sc, &ml, 1);
	}
	sc->sc_rx_delivering = 0;
}

static IOReturn
mwx_dma_rx_gated(OSObject *owner, void *arg0, void *arg1, void *arg2,
    void *arg3)
{
	mwx_dma_rx_deliver_run((struct mwx_softc *)arg0);
	return kIOReturnSuccess;
}

/*
 * Hand everything collected by mwx_dma_service() to net80211.
 *
 * net80211 has no locking of its own in this port (splnet() is a no-op),
 * so the receive path must never run in two contexts at once.  Run it as
 * a command gate action: the action executes with the workloop's gate
 * lock held, whichever thread performs it, which serialises it against
 * every workloop event source (in particular mwx_intr()).  if_input()
 * inside the action re-enters the same gate, which is recursive.
 *
 * Called with no lock held.
 */
static void
mwx_dma_rx_deliver(struct mwx_softc *sc)
{
	/*
	 * Before the command gate exists (attach time) there is no other
	 * thread to serialise against.
	 */
	if (_fCommandGate == NULL) {
		mwx_dma_rx_deliver_run(sc);
		return;
	}

	/*
	 * The gate action takes the frames out of sc_rx_pending itself.
	 * Taking them here would lose them if delivery is already in
	 * progress (the action returns immediately in that case, and the
	 * outermost invocation picks up everything that was queued).
	 */
	if (_fCommandGate->runAction(mwx_dma_rx_gated, sc, NULL, NULL,
	    NULL) != kIOReturnSuccess) {
		/*
		 * The gate is disabled, so the interface is being torn
		 * down and no further poll can deliver what is queued.
		 * Drop it here instead of leaking it.
		 */
		mwx_ring_lock(sc);
		ml_purge(&sc->sc_rx_pending);
		mwx_ring_unlock(sc);
	}
}

static void
mwx_tx_lock(struct mwx_softc *sc)
{
	if (sc->sc_tx_lock == NULL)
		return;
	IOLockLock((IOLock *)sc->sc_tx_lock);
}

static void
mwx_tx_unlock(struct mwx_softc *sc)
{
	if (sc->sc_tx_lock == NULL)
		return;
	IOLockUnlock((IOLock *)sc->sc_tx_lock);
}

static void
mwx_ring_lock(struct mwx_softc *sc)
{
	if (sc->sc_ring_lock == NULL)
		return;
	IOLockLock((IOLock *)sc->sc_ring_lock);
	sc->sc_ring_owner = (void *)current_thread();
}

static void
mwx_ring_unlock(struct mwx_softc *sc)
{
	if (sc->sc_ring_lock == NULL)
		return;
	sc->sc_ring_owner = NULL;
	IOLockUnlock((IOLock *)sc->sc_ring_lock);
}

/*
 * Service the rings from a thread that is waiting for the device.
 *
 * A completion interrupt is not guaranteed to be delivered while the
 * driver is inside a long handler (the platform may hold the vector
 * back, and the pending status bit then suppresses further edges), so
 * every wait loop drains the rings itself instead of trusting that an
 * interrupt will arrive.  That is what makes firmware download and the
 * transmit path independent of MSI delivery.
 *
 * TryLock: if the interrupt action (or another waiting thread) owns the
 * rings it is already draining them.  Never block here, because the
 * owner may itself be waiting for a reply that only the caller's next
 * poll can produce.  Re-entry from the interrupt action is allowed and
 * detected through sc_ring_owner; the outermost frame is responsible for
 * delivering what the nested call collected.
 */
static void
mwx_dma_poll(struct mwx_softc *sc)
{
	if (sc->sc_ring_lock == NULL)
		return;

	if (sc->sc_ring_owner == (void *)current_thread()) {
		mwx_dma_service(sc);
		return;
	}

	if (!IOLockTryLock((IOLock *)sc->sc_ring_lock))
		return;

	sc->sc_ring_owner = (void *)current_thread();
	mwx_dma_service(sc);
	sc->sc_ring_owner = NULL;
	IOLockUnlock((IOLock *)sc->sc_ring_lock);

	mwx_dma_rx_deliver(sc);
}

int
mwx_intr(void *arg)
{
	struct mwx_softc *sc = (struct mwx_softc *)arg;

	/*
	 * The MSI source is enabled before mwx_attach() maps the BAR; a
	 * stray vector in that window must not reach mwx_write().
	 */
	if (sc->sc_memh == 0)
		return 0;

	sc->sc_intr_count++;

	/*
	 * Serialise against the wait loops that poll the rings themselves
	 * (mwx_dma_poll()).  This runs on the workloop, in thread context,
	 * so blocking here is safe.
	 */
	mwx_ring_lock(sc);
	mwx_dma_service(sc);
	mwx_ring_unlock(sc);

	/*
	 * Deliver outside the ring lock: if_input() takes the workloop
	 * gate, and this action already runs with that gate held (the
	 * action is therefore executed inline and does not block).
	 */
	mwx_dma_rx_deliver(sc);

	return 1;
}

int
mwx_preinit(struct mwx_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	int rv, i, nss;
	uint8_t chan;

	DPRINTF("%s: preinit\n", DEVNAME(sc));

	if ((rv = mwx_init_hardware(sc)) != 0)
		return rv;

	if ((rv = mwx_mcu_set_deep_sleep(sc, 1)) != 0)
		return rv;

	ic->ic_sup_rates[IEEE80211_MODE_11B] = ieee80211_std_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = ieee80211_std_rateset_11g;

	if (sc->sc_capa.has_2ghz) {
		for (i = 0; i < MWX_NUM_2GHZ_CHANNELS; i++) {
			chan = mwx_channels_2ghz[i];
			ic->ic_channels[chan].ic_freq =
			    ieee80211_ieee2mhz(chan, IEEE80211_CHAN_2GHZ);
#ifdef __APPLE__
			/*
			 * itlwm's net80211 encodes the usable widths
			 * directly in ic_flags instead of the newer
			 * capability/xflags split.
			 */
			ic->ic_channels[chan].ic_flags =
			    IEEE80211_CHAN_CCK | IEEE80211_CHAN_OFDM |
			    IEEE80211_CHAN_DYN | IEEE80211_CHAN_2GHZ |
			    IEEE80211_CHAN_HT20 | IEEE80211_CHAN_HT40U |
			    IEEE80211_CHAN_HT40D;
#else
			ic->ic_channels[chan].ic_flags =
			    IEEE80211_CHAN_CCK | IEEE80211_CHAN_OFDM |
			    IEEE80211_CHAN_DYN | IEEE80211_CHAN_2GHZ |
			    IEEE80211_CHAN_HT | IEEE80211_CHAN_40MHZ;
			ic->ic_channels[chan].ic_xflags = 0;
#endif
		}

	}
	if (sc->sc_capa.has_5ghz) {
		ic->ic_sup_rates[IEEE80211_MODE_11A] =
		    ieee80211_std_rateset_11a;
		/* set supported .11a channels */
		for (i = 0; i < MWX_NUM_5GHZ_CHANNELS; i++) {
			chan = mwx_channels_5ghz[i];
			ic->ic_channels[chan].ic_freq =
			    ieee80211_ieee2mhz(chan, IEEE80211_CHAN_5GHZ);
#ifdef __APPLE__
			ic->ic_channels[chan].ic_flags = IEEE80211_CHAN_A |
			    IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_HT20 |
			    IEEE80211_CHAN_HT40U | IEEE80211_CHAN_HT40D |
			    IEEE80211_CHAN_VHT20 | IEEE80211_CHAN_VHT40U |
			    IEEE80211_CHAN_VHT40D | IEEE80211_CHAN_VHT80;
			if (sc->sc_hwtype == MWX_HW_MT7922 ||
			    sc->sc_hwtype == MWX_HW_MT7925)
				ic->ic_channels[chan].ic_flags |=
				    IEEE80211_CHAN_VHT160;
#else
			ic->ic_channels[chan].ic_flags = IEEE80211_CHAN_A |
			    IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_HT |
			    IEEE80211_CHAN_40MHZ | IEEE80211_CHAN_VHT;
			ic->ic_channels[chan].ic_xflags =
			    IEEE80211_CHANX_80MHZ;
			if (sc->sc_hwtype == MWX_HW_MT7922 ||
			    sc->sc_hwtype == MWX_HW_MT7925)
				ic->ic_channels[chan].ic_xflags |=
				    IEEE80211_CHANX_160MHZ;
#endif

			/* 5250-5720 MHz are DFS channels (52-140) */
			if (chan >= 52 && chan <= 140)
				ic->ic_channels[chan].ic_flags |=
				    IEEE80211_CHAN_PASSIVE;
		}
	}
#ifdef NOTYET
	/* TODO support for 6GHz */
	if (sc->sc_capa.has_6ghz) {
		for (i = 0; i < MWX_NUM_6GHZ_CHANNELS; i++) {
		}
	}
#endif

	/*
	 * Advertise the VHT MCS map matching the number of spatial streams
	 * reported by the firmware.  Streams beyond that are marked as not
	 * supported, otherwise net80211 may negotiate rates the radio cannot
	 * use.  This matters for 5 GHz 11ac operation on every chipset.
	 */
	nss = sc->sc_capa.num_streams;
	if (nss < 1 || nss > IEEE80211_VHT_NUM_SS)
		nss = 1;
	ic->ic_vht_rxmcs = 0;
	for (i = 1; i <= IEEE80211_VHT_NUM_SS; i++) {
		if (i <= nss)
			ic->ic_vht_rxmcs |= IEEE80211_VHT_MCS_0_9 <<
			    IEEE80211_VHT_MCS_FOR_SS_SHIFT(i);
		else
			ic->ic_vht_rxmcs |= IEEE80211_VHT_MCS_SS_NOT_SUPP <<
			    IEEE80211_VHT_MCS_FOR_SS_SHIFT(i);
	}
	ic->ic_vht_txmcs = ic->ic_vht_rxmcs;

	/*
	 * net80211 does not synthesize a supported HT MCS set, and an
	 * all-zero set makes HT negotiation fail against APs that mark
	 * MCS 0-7 as basic (which is the common case).  Advertise one
	 * byte of MCS per spatial stream, matching the VHT map above.
	 */
	ic->ic_tx_mcs_set = IEEE80211_TX_MCS_SET_DEFINED;
	memset(ic->ic_sup_mcs, 0, sizeof(ic->ic_sup_mcs));
	for (i = 0; i < nss && i < (int)sizeof(ic->ic_sup_mcs); i++)
		ic->ic_sup_mcs[i] = 0xff;

#ifdef __APPLE__
	/*
	 * itlwm's net80211 also wants the raw MCS bitmap that its rate
	 * adaptation code consults.  Bit n corresponds to MCS n.
	 */
	memset(ic->ic_vht_sup_mcs, 0, sizeof(ic->ic_vht_sup_mcs));
	for (i = 0; i < IEEE80211_VHT_NUM_SS * 10 && i < 80; i++) {
		if (i / 10 < nss && (i % 10) <= 9)
			setbit(ic->ic_vht_sup_mcs, i);
	}
#endif

	/* Configure channel information obtained from firmware. */
	ieee80211_channel_init(ifp);

	if (IEEE80211_ADDR_EQ(etheranyaddr, sc->sc_ic.ic_myaddr))
		IEEE80211_ADDR_COPY(ic->ic_myaddr, sc->sc_lladdr);

	/* Configure MAC address. */
	rv = if_setlladdr(ifp, ic->ic_myaddr);
	if (rv)
		printf("%s: could not set MAC address (error %d)\n",
		    DEVNAME(sc), rv);

	ieee80211_media_init(ifp);

	DPRINTF("%s: preinit done\n", DEVNAME(sc));
	return 0;
}

int
mwx_match(IOPCIDevice *device)
{
	int devId = device->configRead16(kIOPCIConfigDeviceID);

	return pci_matchbyid(PCI_VENDOR_MEDIATEK, devId, mwx_devices,
	    nitems(mwx_devices));
}

bool
mwx_attach(struct mwx_softc *sc, struct pci_attach_args *pa,
    IOEthernetController *controller)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	pcireg_t memtype, reg;
	uint32_t hwid, hwrev;
	int error;
	uint16_t devid;

	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;
	sc->sc_dmat = pa->pa_dmat;

	snprintf(sc->sc_dev.dv_xname, sizeof(sc->sc_dev.dv_xname), "mwx0");

	/*
	 * The interrupt source is enabled before this function is called,
	 * so the ring lock has to exist before the BAR is mapped and the
	 * handler can reach the WFDMA registers.
	 */
	sc->sc_ring_lock = IOLockAlloc();
	if (sc->sc_ring_lock == NULL) {
		printf("%s: can't allocate ring lock\n", DEVNAME(sc));
		return false;
	}
	sc->sc_ring_owner = NULL;

	sc->sc_tx_lock = IOLockAlloc();
	if (sc->sc_tx_lock == NULL) {
		printf("%s: can't allocate tx lock\n", DEVNAME(sc));
		IOLockFree((IOLock *)sc->sc_ring_lock);
		sc->sc_ring_lock = NULL;
		return false;
	}
	ml_init(&sc->sc_rx_pending);
	ml_init(&sc->sc_rx_deferred);
	sc->sc_rx_delivering = 0;

	/* Enable memory space and bus mastering. */
	reg = pci_conf_read(sc->sc_pc, sc->sc_tag, PCI_COMMAND_STATUS_REG);
	reg |= PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_MEM_ENABLE;
	reg &= ~PCI_COMMAND_INTERRUPT_DISABLE;
	pci_conf_write(sc->sc_pc, sc->sc_tag, PCI_COMMAND_STATUS_REG, reg);

	sc->sc_intr_mask = MT_INT_RX_DONE_ALL | MT_INT_TX_DONE_ALL |
	    MT_INT_MCU_CMD;
	devid = pa->pa_tag->configRead16(kIOPCIConfigDeviceID);
	switch (devid) {
	case PCI_PRODUCT_MEDIATEK_MT7920:
		sc->sc_hwtype = MWX_HW_MT7920;
		break;
	case PCI_PRODUCT_MEDIATEK_MT7921:
	case PCI_PRODUCT_MEDIATEK_MT7921K:
		sc->sc_hwtype = MWX_HW_MT7921;
		break;
	case PCI_PRODUCT_MEDIATEK_MT7922:
	case PCI_PRODUCT_MEDIATEK_RZ616:
		sc->sc_hwtype = MWX_HW_MT7922;
		break;
	case PCI_PRODUCT_MEDIATEK_MT7925:
	case PCI_PRODUCT_MEDIATEK_RZ717:
		sc->sc_hwtype = MWX_HW_MT7925;
		sc->sc_intr_mask = MT7925_INT_RX_DONE_ALL |
		    MT7925_INT_TX_DONE_ALL | MT_INT_MCU_CMD;
		break;
	default:
		printf("%s: unknown chip id 0x%04x\n", DEVNAME(sc), devid);
		return false;
	}

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, PCI_MAPREG_START);
	if (pci_mapreg_map(pa, PCI_MAPREG_START, memtype, 0,
	    &sc->sc_st, &sc->sc_memh, NULL, &sc->sc_mems, 0)) {
		printf("%s: can't map mem space\n", DEVNAME(sc));
		return false;
	}

	if (mwx_mcu_fw_pmctrl(sc) != 0 || mwx_mcu_drv_pmctrl(sc) != 0)
		goto fail;

	hwid = mwx_read(sc, MT_HW_CHIPID) & 0xffff;
	hwrev = mwx_read(sc, MT_HW_REV) & 0xff;

	if (sc->sc_hwtype == MWX_HW_MT7921 &&
	    (mwx_read(sc, MT_HW_BOUND) & 0x80) != 0) {
		hwid = 0x7920;
		sc->sc_hwtype = MWX_HW_MT7920;
	}

	printf("%s: MT%x.%x\n", DEVNAME(sc), hwid, hwrev);

	if (sc->sc_hwtype == MWX_HW_MT7925)
		mwx_set(sc, MT_HW_EMI_CTL, MT_HW_EMI_CTL_SLPPROT_EN);
	mwx_set(sc, MT_MCU2HOST_SW_INT_ENA, MT_MCU_CMD_WAKE_RX_PCIE);

	if (mwx_wfsys_reset(sc) != 0)
		goto fail;

	mwx_write(sc, MT_WFDMA0_HOST_INT_ENA, 0);
	mwx_write(sc, MT_PCIE_MAC_INT_ENABLE, 0xff);

	if ((error = mwx_txwi_alloc(sc, MWX_TXWI_MAX)) != 0) {
		printf("%s: failed to allocate DMA resources %d\n",
		    DEVNAME(sc), error);
		goto fail;
	}

	if ((error = mwx_dma_alloc(sc)) != 0) {
		printf("%s: failed to allocate DMA resources %d\n",
		    DEVNAME(sc), error);
		goto fail;
	}

	/* set regulatory domain to '00' */
	sc->sc_alpha2[0] = '0';
	sc->sc_alpha2[1] = '0';

	ic->ic_phytype = IEEE80211_T_OFDM;	/* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA;	/* default to BSS mode */
	ic->ic_state = IEEE80211_S_INIT;

	/* Set device capabilities. */
	ic->ic_caps =
	    IEEE80211_C_QOS |		/* WMM/QoS */
	    IEEE80211_C_WEP |		/* WEP */
	    IEEE80211_C_RSN |		/* WPA/RSN */
	    IEEE80211_C_SCANALL |	/* device scans all channels at once */
	    IEEE80211_C_SCANALLBAND |	/* device scans all bands at once */
	    IEEE80211_C_MONITOR |	/* monitor mode supported */
#ifndef IEEE80211_STA_ONLY
	    IEEE80211_C_IBSS |		/* IBSS mode supported */
	    IEEE80211_C_HOSTAP |	/* HostAP mode supported */
	    IEEE80211_C_APPMGT |	/* HostAP power management */
#endif
	    IEEE80211_C_SHSLOT |	/* short slot time supported */
	    IEEE80211_C_SHPREAMBLE;	/* short preamble supported */
	/*
	 * Tx aggregation is deliberately left off until the ADDBA paths in
	 * this driver are finished.  HT/VHT association itself does not
	 * require it and working 5 GHz links are more valuable than a
	 * half-implemented AMPDU offload.
	 */

	ic->ic_htcaps = IEEE80211_HTCAP_SGI20 | IEEE80211_HTCAP_SGI40;
	ic->ic_htcaps |= IEEE80211_HTCAP_CBW20_40;
	ic->ic_htcaps |=
	    (IEEE80211_HTCAP_SMPS_DIS << IEEE80211_HTCAP_SMPS_SHIFT);
	ic->ic_htxcaps = 0;
	ic->ic_txbfcaps = 0;
	ic->ic_aselcaps = 0;
	ic->ic_ampdu_params = (IEEE80211_AMPDU_PARAM_SS_4 | 0x3 /* 64k */);

	ic->ic_vhtcaps = IEEE80211_VHTCAP_MAX_MPDU_LENGTH_11454 |
	    (IEEE80211_VHTCAP_MAX_AMPDU_LEN_64K <<
	    IEEE80211_VHTCAP_MAX_AMPDU_LEN_SHIFT) |
	    (IEEE80211_VHTCAP_CHAN_WIDTH_80 <<
	    IEEE80211_VHTCAP_CHAN_WIDTH_SHIFT) | IEEE80211_VHTCAP_SGI80 |
	    IEEE80211_VHTCAP_RX_ANT_PATTERN | IEEE80211_VHTCAP_TX_ANT_PATTERN;
	/* MT7922 (RZ616) and MT7925 (RZ717) support 160 MHz. */
	if (sc->sc_hwtype == MWX_HW_MT7922 ||
	    sc->sc_hwtype == MWX_HW_MT7925) {
		ic->ic_vhtcaps &=
		    ~(IEEE80211_VHTCAP_CHAN_WIDTH_MASK <<
		    IEEE80211_VHTCAP_CHAN_WIDTH_SHIFT);
		ic->ic_vhtcaps |=
		    (IEEE80211_VHTCAP_CHAN_WIDTH_160 <<
		    IEEE80211_VHTCAP_CHAN_WIDTH_SHIFT) |
		    IEEE80211_VHTCAP_SGI160;
	}

	/* IBSS channel undefined for now. */
	ic->ic_ibss_chan = &ic->ic_channels[1];

	ic->ic_max_aid = min(IEEE80211_AID_MAX, MWX_WTBL_STA - 1);

#ifdef __APPLE__
	/*
	 * net80211 and the Airport ioctl layer expect a positive quality
	 * value scaled by ic_max_rssi; see the conversion in mwx_rx().
	 */
	ic->ic_max_rssi = MWX_MAX_DBM - MWX_MIN_DBM;
#else
	ic->ic_max_rssi = 0;	/* RSSI value is in dBm. */
#endif

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = mwx_ioctl;
	ifp->if_start = mwx_start;
	ifp->if_watchdog = mwx_watchdog;
	memcpy(ifp->if_xname, DEVNAME(sc), IFNAMSIZ);

	if_attach(ifp);
	ieee80211_ifattach(ifp, controller);
	ieee80211_media_init(ifp);

#if NBPFILTER > 0
	mwx_radiotap_attach(sc);
#endif

	ic->ic_node_alloc = mwx_node_alloc;
	ic->ic_newassoc = mwx_newassoc;
#ifndef IEEE80211_STA_ONLY
	ic->ic_node_leave = mwx_node_leave;
#endif

	/* Override 802.11 state transition machine. */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = mwx_newstate;
	ic->ic_bgscan_start = mwx_bgscan;
	ic->ic_set_key = mwx_set_key;
	ic->ic_delete_key = mwx_delete_key;

	sc->sc_nswq = taskq_create("mwxns", 1, IPL_NET, 0);
	if (sc->sc_nswq == NULL) {
		printf("%s: can't create task queue\n", DEVNAME(sc));
		goto fail;
	}

	task_set(&sc->sc_newstate_task, mwx_newstate_task, sc, "mwx_newstate");
	task_set(&sc->sc_bgscan_done_task, mwx_bgscan_done_task, sc,
	    "mwx_bgscan");
	task_set(&sc->sc_scan_task, mwx_end_scan_task, sc, "mwx_endscan");
	task_set(&sc->sc_bmiss_task, mwx_bmiss_task, sc, "mwx_bmiss");
	task_set(&sc->sc_reset_task, mwx_reset_task, sc, "mwx_reset");
	timeout_set(&sc->sc_reset_to, mwx_reset_timeo, sc);

	/*
	 * The device has no usable MAC address before firmware is loaded,
	 * so preinit is deferred until now, exactly like the OpenBSD
	 * config_mountroot() hook.
	 */
	if (mwx_preinit(sc) != 0)
		goto fail;

	printf("%s: attach succeed.\n", DEVNAME(sc));
	return true;

fail:
	mwx_txwi_free(sc);
	mwx_dma_free(sc);
	if (sc->sc_nswq != NULL) {
		taskq_destroy(sc->sc_nswq);
		sc->sc_nswq = NULL;
	}
	bus_space_unmap(sc->sc_st, sc->sc_memh, sc->sc_mems);
	if (sc->sc_ring_lock != NULL) {
		IOLock *lock = (IOLock *)sc->sc_ring_lock;

		/*
		 * Publish the removal before freeing so a poll running on
		 * another thread stops before it touches the lock.
		 */
		sc->sc_ring_lock = NULL;
		IOLockFree(lock);
	}
	if (sc->sc_tx_lock != NULL) {
		IOLock *lock = (IOLock *)sc->sc_tx_lock;

		sc->sc_tx_lock = NULL;
		IOLockFree(lock);
	}
	return false;
}

#ifdef __APPLE__
int
mwx_activate(struct mwx_softc *sc, int act)
{
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int err;
#else
int
mwx_activate(struct device *self, int act)
{
	struct mwx_softc *sc = (struct mwx_softc *)self;
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int err;
#endif

	switch (act) {
	case DVACT_QUIESCE:
		/*
		 * enable()/disable() call us on the main workloop, so the
		 * interrupt action cannot run until we return.  The MCU
		 * wait loops poll the rings themselves in that window
		 * instead of sleeping out every timeout.
		 */
		if (ifp->if_flags & IFF_RUNNING) {
			mwx_stop(ifp);
		}
		/*
		 * Without firmware the MCU never answers: every command
		 * below would burn the full 3 s timeout and then queue a
		 * reset.  Shut the DMA and interrupts down anyway, but
		 * skip the MCU traffic in that state.
		 */
		if (sc->sc_fw_loaded) {
			mwx_mcu_set_hif_suspend(sc, 1);
			/* wait until dma is idle  */
		}

		/* put dma disabled */
		mwx_dma_disable(sc, 1);

		/* disable interrupt */
		mwx_write(sc, MT_WFDMA0_HOST_INT_ENA, 0);
		mwx_write(sc, MT_PCIE_MAC_INT_ENABLE, 0);

		if (sc->sc_fw_loaded)
			mwx_mcu_fw_pmctrl(sc);
		sc->sc_pm_suspended = 1;
		break;
	case DVACT_WAKEUP:
		/*
		 * Only unwind a resume that matches an earlier quiesce.
		 * This platform drives DVACT_WAKEUP from the interface
		 * enable path, so the first "up" after attach would
		 * otherwise run a full resume against a chip that was
		 * never suspended: it resets the DMA ring indices the MCU
		 * is still using and sends HIF_CTRL(0) with no matching
		 * HIF_CTRL(1).  The result is an MCU that stops answering
		 * every command even though FW_N9_RDY still reads ready.
		 * OpenBSD only reaches this branch after a real suspend.
		 */
		if (sc->sc_pm_suspended) {
			sc->sc_pm_suspended = 0;

			if (sc->sc_fw_loaded)
				mwx_mcu_drv_pmctrl(sc);
			/* mt792x_wpdma_reinit_cond(sc); */

			/* enable interrupt */
			mwx_write(sc, MT_PCIE_MAC_INT_ENABLE, 0xff);
			mwx_write(sc, MT_WFDMA0_HOST_INT_ENA,
			    sc->sc_intr_mask);

			/* put dma enabled */
			mwx_dma_reset(sc, 0);

			if (sc->sc_fw_loaded)
				mwx_mcu_set_hif_suspend(sc, 0);
		}

		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == IFF_UP) {
			err = mwx_init(ifp);
			if (err)
				printf("%s: could not initialize hardware\n",
				    DEVNAME(sc));
		}
		break;
	}
	return 0;
}

#ifdef __APPLE__
void
mwx_detach(struct mwx_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;

	if (ifp->if_flags & IFF_RUNNING)
		mwx_stop(ifp);

	if (sc->sc_nswq != NULL) {
		taskq_destroy(sc->sc_nswq);
		sc->sc_nswq = NULL;
	}
	if (sc->sc_reset_to != NULL) {
		timeout_del(&sc->sc_reset_to);
		timeout_free(&sc->sc_reset_to);
	}

	ieee80211_ifdetach(ifp);
	mwx_txwi_free(sc);
	mwx_dma_free(sc);
	bus_space_unmap(sc->sc_st, sc->sc_memh, sc->sc_mems);

	/* Frames harvested but never delivered (teardown raced a poll). */
	mwx_ring_lock(sc);
	ml_purge(&sc->sc_rx_pending);
	ml_purge(&sc->sc_rx_deferred);
	mwx_ring_unlock(sc);

	if (sc->sc_ring_lock != NULL) {
		IOLock *lock = (IOLock *)sc->sc_ring_lock;

		/*
		 * Publish the removal before freeing so a poll running on
		 * another thread stops before it touches the lock.
		 */
		sc->sc_ring_lock = NULL;
		IOLockFree(lock);
	}
	if (sc->sc_tx_lock != NULL) {
		IOLock *lock = (IOLock *)sc->sc_tx_lock;

		sc->sc_tx_lock = NULL;
		IOLockFree(lock);
	}
}
#endif

#ifndef __APPLE__
struct cfdriver mwx_cd = {
	NULL, "mwx", DV_IFNET
};

struct cfattach mwx_ca = {
	sizeof(struct mwx_softc), mwx_match, mwx_attach,
	NULL, mwx_activate
};
#endif

void
mwx_reset(struct mwx_softc *sc)
{
	if (sc->sc_resetting)
		return;
	sc->sc_resetting = 1;
	task_add(sc->sc_nswq, &sc->sc_reset_task);
}

void
mwx_reset_timeo(void *arg)
{
	mwx_reset((struct mwx_softc *)arg);
}

void
mwx_reset_task(void *arg)
{
	struct mwx_softc *sc = (struct mwx_softc *)arg;
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int fatal = 0;

	if (ifp->if_flags & IFF_RUNNING)
		mwx_stop(ifp);

	if (!fatal && (ifp->if_flags & (IFF_UP | IFF_RUNNING)) == IFF_UP)
		mwx_init(ifp);
	sc->sc_resetting = 0;
	sc->sc_coredump_cnt = 0;
}

int
mwx_txwi_alloc(struct mwx_softc *sc, int count)
{
	int error, nsegs, i;
	struct mwx_txwi_desc *q = &sc->sc_txwi;
	bus_size_t size = count * sizeof(*q->mt_desc);
	uint32_t addr;

	LIST_INIT(&q->mt_freelist);
	q->mt_count = count;

	error = bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT, &q->mt_map);
	if (error != 0) {
		printf("%s: could not create desc TWXI map\n", DEVNAME(sc));
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, &q->mt_seg,
	    1, &nsegs, BUS_DMA_NOWAIT | BUS_DMA_ZERO);
	if (error != 0) {
		printf("%s: could not allocate TWXI memory\n", DEVNAME(sc));
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &q->mt_seg, nsegs, size,
	    (void **)&q->mt_desc, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: can't map desc DMA memory\n", DEVNAME(sc));
		goto fail;
	}

	error = mwx_dmamap_load_raw(sc->sc_dmat, q->mt_map, (caddr_t)q->mt_desc,
	    size);
	if (error != 0) {
		printf("%s: could not load desc DMA map\n", DEVNAME(sc));
		goto fail;
	}

	addr = q->mt_map->dm_segs[0].ds_addr;

	q->mt_data = (struct mwx_txwi *)mallocarray(count, sizeof(*q->mt_data),
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (q->mt_data == NULL) {
		printf("%s: could not allocate soft data\n", DEVNAME(sc));
		error = ENOMEM;
		goto fail;
	}

	for (i = 0; i < count; i++) {
		q->mt_data[i].mt_idx = i;
		q->mt_data[i].mt_desc = &q->mt_desc[i];
		q->mt_data[i].mt_addr = addr + i * sizeof(*q->mt_desc);
		error = bus_dmamap_create(sc->sc_dmat, MT_TXD_LEN_MASK,
		    MT_MAX_SCATTER, MT_TXD_LEN_MASK, 0, BUS_DMA_NOWAIT,
		    &q->mt_data[i].mt_map);
		if (error != 0) {
			printf("%s: could not create data DMA map\n",
			    DEVNAME(sc));
			goto fail;
		}
	}

	for (i = count - 1; i >= MT_PACKET_ID_FIRST; i--)
		LIST_INSERT_HEAD(&q->mt_freelist, &q->mt_data[i], mt_entry);

	return 0;

fail:
	mwx_txwi_free(sc);
	return error;
}

void
mwx_txwi_free(struct mwx_softc *sc)
{
	struct mwx_txwi_desc *q = &sc->sc_txwi;

	if (q->mt_data != NULL) {
		int i;
		for (i = 0; i < q->mt_count; i++) {
			struct mwx_txwi *mt = &q->mt_data[i];
			bus_dmamap_destroy(sc->sc_dmat, mt->mt_map);
			m_freem(mt->mt_mbuf);
			if (i >= MT_PACKET_ID_FIRST)
				LIST_REMOVE(mt, mt_entry);
		}
		mwx_free(q->mt_data);
	}

	if (q->mt_desc != NULL) {
		bus_dmamap_sync(sc->sc_dmat, q->mt_map, 0,
			q->mt_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, q->mt_map);
	}

	/*
	 * XXX TODO this is probably not correct as a check, should use
	 * some state variable bitfield to decide which steps need to be run.
	 */
	if (mwx_dmamem_allocated(q->mt_seg))
		bus_dmamem_unmap(q->mt_seg);
	if (q->mt_map != NULL)
		bus_dmamem_free(sc->sc_dmat, &q->mt_seg, 1);

	memset(q, 0, sizeof(*q));
}

struct mwx_txwi *
mwx_txwi_get(struct mwx_softc *sc)
{
	struct mwx_txwi *mt;

	mwx_tx_lock(sc);
	mt = LIST_FIRST(&sc->sc_txwi.mt_freelist);
	if (mt != NULL) {
		LIST_REMOVE(mt, mt_entry);
		mt->mt_busy = 1;
	}
	mwx_tx_unlock(sc);
	return mt;
}

void
mwx_txwi_put(struct mwx_softc *sc, struct mwx_txwi *mt)
{
	if (mt->mt_busy == 0)
		return;

	/*
	 * Clean the descriptor up before publishing it back to the pool:
	 * the moment it is on the freelist another thread may take it.
	 * Freeing an mbuf or releasing a node can block or re-enter the
	 * transmit path, so that part is done without the lock.
	 */
	if (mt->mt_mbuf != NULL) {
		bus_dmamap_sync(sc->sc_dmat, mt->mt_map, 0,
		    mt->mt_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, mt->mt_map);
		m_freem(mt->mt_mbuf);
		mt->mt_mbuf = NULL;
	}
	if (mt->mt_ni != NULL) {
		ieee80211_release_node(&sc->sc_ic, mt->mt_ni);
		mt->mt_ni = NULL;
	}

	memset(mt->mt_desc, 0, sizeof(*mt->mt_desc));

	if (mt->mt_idx < MT_PACKET_ID_FIRST) {
		mt->mt_busy = 0;
		return;
	}

	mwx_tx_lock(sc);
	mt->mt_busy = 0;
	LIST_INSERT_HEAD(&sc->sc_txwi.mt_freelist, mt, mt_entry);
	mwx_tx_unlock(sc);
}

int
mwx_txwi_enqueue(struct mwx_softc *sc, struct mwx_txwi *mt, struct mbuf *m,
    struct ieee80211_node *ni)
{
	struct mwx_txwi_desc *q = &sc->sc_txwi;
	struct mt76_txwi *txp = mt->mt_desc;
	struct mt76_connac_txp_ptr *ptr = &txp->ptr[0];
	uint32_t addr;
	uint16_t len;
	int i, nsegs, rv;

	rv = bus_dmamap_load_mbuf(sc->sc_dmat, mt->mt_map, m,
	    BUS_DMA_WRITE | BUS_DMA_NOWAIT);
	if (rv == EFBIG && m_defrag(m, M_DONTWAIT) == 0)
		rv = bus_dmamap_load_mbuf(sc->sc_dmat, mt->mt_map, m,
		    BUS_DMA_WRITE | BUS_DMA_NOWAIT);
	if (rv != 0)
		return rv;

	nsegs = mt->mt_map->dm_nsegs;

	bus_dmamap_sync(sc->sc_dmat, mt->mt_map, 0, mt->mt_map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	txp->msdu_id[0] = htole16(mt->mt_idx | MT_MSDU_ID_VALID);
	mt->mt_mbuf = m;
	mt->mt_ni = ni;

	bus_dmamap_sync(sc->sc_dmat, q->mt_map, 0, q->mt_map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);
	for (i = 0; i < nsegs; i++) {
		KASSERT(mt->mt_map->dm_segs[i].ds_addr <= UINT32_MAX);
		KASSERT(mt->mt_map->dm_segs[i].ds_len <= MT_TXD_LEN_MASK);
		addr = mt->mt_map->dm_segs[i].ds_addr;
		len = mt->mt_map->dm_segs[i].ds_len;

		if (i == nsegs - 1)
			len |= MT_TXD_LEN_LAST;

		if ((i & 1) == 0) {
			ptr->buf0 = htole32(addr);
			ptr->len0 = htole16(len);
		} else {
			ptr->buf1 = htole32(addr);
			ptr->len1 = htole16(len);
			ptr++;
		}
	}
	bus_dmamap_sync(sc->sc_dmat, q->mt_map, 0, q->mt_map->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE);

	return 0;
}

int
mwx_queue_alloc(struct mwx_softc *sc, struct mwx_queue *q, int count,
    uint32_t regbase)
{
	int error, nsegs, i;
	bus_size_t size = count * sizeof(*q->mq_desc);

	q->mq_regbase = regbase;
	q->mq_count = count;

	error = bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT, &q->mq_map);
	if (error != 0) {
		printf("%s: could not create desc DMA map\n", DEVNAME(sc));
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, &q->mq_seg,
	    1, &nsegs, BUS_DMA_NOWAIT | BUS_DMA_ZERO);
	if (error != 0) {
		printf("%s: could not allocate DMA memory\n", DEVNAME(sc));
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &q->mq_seg, nsegs, size,
	    (void **)&q->mq_desc, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: can't map desc DMA memory\n", DEVNAME(sc));
		goto fail;
	}

	error = mwx_dmamap_load_raw(sc->sc_dmat, q->mq_map, (caddr_t)q->mq_desc,
	    size);
	if (error != 0) {
		printf("%s: could not load desc DMA map\n", DEVNAME(sc));
		goto fail;
	}

	q->mq_data = (struct mwx_queue_data *)mallocarray(count, sizeof(*q->mq_data),
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (q->mq_data == NULL) {
		printf("%s: could not allocate soft data\n", DEVNAME(sc));
		error = ENOMEM;
		goto fail;
	}

	for (i = 0; i < count; i++) {
		error = bus_dmamap_create(sc->sc_dmat, MT_MAX_SIZE,
		    MT_MAX_SCATTER, MT_MAX_SIZE, 0, BUS_DMA_NOWAIT,
		    &q->mq_data[i].md_map);
		if (error != 0) {
			printf("%s: could not create data DMA map\n",
			    DEVNAME(sc));
			goto fail;
		}
	}

	mwx_queue_reset(sc, q);
	return 0;

fail:
	mwx_queue_free(sc, q);
	return error;
}

void
mwx_queue_free(struct mwx_softc *sc, struct mwx_queue *q)
{
	if (q->mq_data != NULL) {
		int i;
		for (i = 0; i < q->mq_count; i++) {
			struct mwx_queue_data  *md = &q->mq_data[i];
			bus_dmamap_destroy(sc->sc_dmat, md->md_map);
			m_freem(md->md_mbuf);
		}
		mwx_free(q->mq_data);
	}

	if (q->mq_desc != NULL) {
		bus_dmamap_sync(sc->sc_dmat, q->mq_map, 0,
			q->mq_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, q->mq_map);
	}

	/*
	 * XXX TODO this is probably not correct as a check, should use
	 * some state variable bitfield to decide which steps need to be run.
	 */
	if (mwx_dmamem_allocated(q->mq_seg))
		bus_dmamem_unmap(q->mq_seg);
	if (q->mq_map != NULL)
		bus_dmamem_free(sc->sc_dmat, &q->mq_seg, 1);

	memset(q, 0, sizeof(*q));
}

void
mwx_queue_reset(struct mwx_softc *sc, struct mwx_queue *q)
{
	int i;
	uint32_t dmaaddr;
	struct mwx_queue_data *md;

	if (q->mq_desc == NULL)
		return;

	/* clear descriptors */
	bus_dmamap_sync(sc->sc_dmat, q->mq_map, 0, q->mq_map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	for (i = 0; i < q->mq_count; i++) {
		q->mq_desc[i].buf0 = 0;
		q->mq_desc[i].buf1 = 0;
		q->mq_desc[i].info = 0;
		q->mq_desc[i].ctrl = htole32(MT_DMA_CTL_DMA_DONE);
	}

	bus_dmamap_sync(sc->sc_dmat, q->mq_map, 0, q->mq_map->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE);

	/* reset DMA registers */
	KASSERT(q->mq_map->dm_nsegs == 1);
	KASSERT(q->mq_map->dm_segs[0].ds_addr <= UINT32_MAX);
	dmaaddr	= q->mq_map->dm_segs[0].ds_addr;
	mwx_write(sc, q->mq_regbase + MT_DMA_DESC_BASE, dmaaddr);
	mwx_write(sc, q->mq_regbase + MT_DMA_RING_SIZE, q->mq_count);
	mwx_write(sc, q->mq_regbase + MT_DMA_CPU_IDX, 0);
	mwx_write(sc, q->mq_regbase + MT_DMA_DMA_IDX, 0);
	q->mq_cons = 0;
	q->mq_prod = 0;

	/* free buffers */
	for (i = 0; i < q->mq_count; i++) {
		md = &q->mq_data[i];
		if (md->md_mbuf != NULL) {
			bus_dmamap_sync(sc->sc_dmat, md->md_map, 0,
			    md->md_map->dm_mapsize,
			    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, md->md_map);
			m_freem(md->md_mbuf);
			md->md_mbuf = NULL;
		}
	}
}

int
mwx_buf_fill(struct mwx_softc *sc, struct mwx_queue_data *md,
    struct mwx_desc *desc)
{
	struct mbuf *m;
	uint32_t buf0, len0, ctrl;
	int rv;

	m = MCLGETL(NULL, M_DONTWAIT, MT_RX_BUF_SIZE);
	if (m == NULL)
		return (ENOMEM);

	mbuf_setlen(m, MT_RX_BUF_SIZE);
	mbuf_pkthdr_setlen(m, MT_RX_BUF_SIZE);

	rv = bus_dmamap_load_mbuf(sc->sc_dmat, md->md_map, m,
	     BUS_DMA_READ | BUS_DMA_NOWAIT);
	if (rv != 0) {
		printf("%s: could not load data, %d\n", DEVNAME(sc), rv);
		m_freem(m);
		return (rv);
	}

	bus_dmamap_sync(sc->sc_dmat, md->md_map, 0,
	    md->md_map->dm_mapsize, BUS_DMASYNC_PREREAD);

	md->md_mbuf = m;

	KASSERT(md->md_map->dm_nsegs == 1);
	KASSERT(md->md_map->dm_segs[0].ds_addr <= UINT32_MAX);
	buf0 = md->md_map->dm_segs[0].ds_addr;
	len0 = md->md_map->dm_segs[0].ds_len;
	ctrl = MT_DMA_CTL_SD_LEN0(len0);
	if (sc->sc_hwtype != MWX_HW_MT7925)
		ctrl |= MT_DMA_CTL_LAST_SEC0;

	desc->buf0 = htole32(buf0);
	desc->buf1 = 0;
	desc->info = 0;
	desc->ctrl = htole32(ctrl);

	return 0;
}

int
mwx_queue_fill(struct mwx_softc *sc, struct mwx_queue *q)
{
	u_int idx, last;
	int rv = 0;

	if (q->mq_desc == NULL)
		return 0;

	last = (q->mq_count + q->mq_cons - 1) % q->mq_count;
	idx = q->mq_prod;
	if (idx == last)
		return 0;

	bus_dmamap_sync(sc->sc_dmat, q->mq_map, 0, q->mq_map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	while (idx != last) {
		rv = mwx_buf_fill(sc, &q->mq_data[idx], &q->mq_desc[idx]);
		if (rv != 0) {
			printf("%s: could not fill data, slot %d err %d\n",
			    DEVNAME(sc), idx, rv);
			break;
		}
		idx = (idx + 1) % q->mq_count;
	}

	bus_dmamap_sync(sc->sc_dmat, q->mq_map, 0, q->mq_map->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE);

	/*
	 * Hand the hardware every descriptor that was filled, even when
	 * the ring could not be fully repopulated.  Failing without
	 * publishing mq_prod leaves those descriptors out of the
	 * producer's view: the next refill starts at mq_prod again and
	 * refills slots the hardware is already using, leaking the
	 * buffers that were loaded above until the ring runs dry.
	 */
	if (idx != q->mq_prod) {
		q->mq_prod = idx;
		mwx_write(sc, q->mq_regbase + MT_DMA_CPU_IDX, q->mq_prod);
	}

	return rv;
}

int
mwx_dma_alloc(struct mwx_softc *sc)
{
	uint32_t txq_mcu_regbase = MT_TX_MCU_RING_BASE;
	int rv;

	if (sc->sc_hwtype == MWX_HW_MT7925)
		txq_mcu_regbase = MT7925_TX_MCU_RING_BASE;

	/* TX queues */
	if ((rv = mwx_queue_alloc(sc, &sc->sc_txq, 256,
	    MT_TX_DATA_RING_BASE)) != 0)
		return rv;
	if ((rv = mwx_queue_alloc(sc, &sc->sc_txmcuq, 256,
	    txq_mcu_regbase)) != 0)
		return rv;
	/*
	 * mt76 sizes the FWDL ring at 128 descriptors
	 * (MT7921_TX_FWDL_RING_SIZE).  The firmware image is split into
	 * one scatter command per memory region, and the largest region
	 * needs 84 descriptors; with a 16 entry ring the download could
	 * only make progress if the device completed descriptors between
	 * two chunks, which turns a single lost completion interrupt into
	 * a permanent "queue still full" stall.
	 */
	if ((rv = mwx_queue_alloc(sc, &sc->sc_txfwdlq, 128,
	    MT_TX_FWDL_RING_BASE)) != 0)
		return rv;

	/* RX queues */
	if ((rv = mwx_queue_alloc(sc, &sc->sc_rxq, 256,
	    MT_RX_DATA_RING_BASE)) != 0 ||
	    (rv = mwx_queue_fill(sc, &sc->sc_rxq)) != 0)
		return rv;
	if (sc->sc_hwtype != MWX_HW_MT7925) {
		if ((rv = mwx_queue_alloc(sc, &sc->sc_rxmcuq, 256,
		    MT_RX_MCU_RING_BASE)) != 0 ||
		    (rv = mwx_queue_fill(sc, &sc->sc_rxmcuq)) != 0)
			return rv;
	}
	if ((rv = mwx_queue_alloc(sc, &sc->sc_rxfwdlq, 16 /* XXX */,
	    MT_RX_FWDL_RING_BASE)) != 0 ||
	    (rv = mwx_queue_fill(sc, &sc->sc_rxfwdlq)) != 0)
		return rv;

	/* enable DMA engine */
	mwx_dma_enable(sc);

	return 0;
}

int
mwx_dma_reset(struct mwx_softc *sc, int fullreset)
{
	int rv;

	DPRINTF("%s: DMA %sreset\n", DEVNAME(sc), fullreset ? "full-" : "");

	if ((rv = mwx_dma_disable(sc, fullreset)) != 0)
		return rv;
	if (fullreset)
		if ((rv = mwx_wfsys_reset(sc)) != 0)
			return rv;

	/* TX queues */
	mwx_queue_reset(sc, &sc->sc_txq);
	mwx_queue_reset(sc, &sc->sc_txmcuq);
	mwx_queue_reset(sc, &sc->sc_txfwdlq);

	/* RX queues */
	mwx_queue_reset(sc, &sc->sc_rxq);
	mwx_queue_reset(sc, &sc->sc_rxmcuq);
	mwx_queue_reset(sc, &sc->sc_rxfwdlq);

	/* TDOD mt76_tx_status_check */

	/* refill RX queues */
	if ((rv = mwx_queue_fill(sc, &sc->sc_rxq)) != 0 ||
	    (rv = mwx_queue_fill(sc, &sc->sc_rxmcuq)) != 0 ||
	    (rv = mwx_queue_fill(sc, &sc->sc_rxfwdlq)) != 0)
		return rv;

	/* enable DMA engine */
	mwx_dma_enable(sc);

	return 0;
}

void
mwx_dma_free(struct mwx_softc *sc)
{
	/* TX queues */
	mwx_queue_free(sc, &sc->sc_txq);
	mwx_queue_free(sc, &sc->sc_txmcuq);
	mwx_queue_free(sc, &sc->sc_txfwdlq);

	/* RX queues */
	mwx_queue_free(sc, &sc->sc_rxq);
	mwx_queue_free(sc, &sc->sc_rxmcuq);
	mwx_queue_free(sc, &sc->sc_rxfwdlq);
}

int
mwx_dma_tx_enqueue(struct mwx_softc *sc, struct mwx_queue *q, struct mbuf *m)
{
	struct mwx_queue_data *md;
	struct mwx_desc *desc;
	bus_dmamap_t map;
	int i, nsegs, idx, rv;

	/*
	 * One producer at a time.  Transmits can come from the network
	 * output thread, the state task queue and the receive path (EAPOL),
	 * and a second producer between the mq_prod read and write below
	 * would overwrite the descriptor of the first one.
	 */
	mwx_tx_lock(sc);

	idx = q->mq_prod;
	md = &q->mq_data[idx];
	desc = &q->mq_desc[idx];

	rv = bus_dmamap_load_mbuf(sc->sc_dmat, md->md_map, m,
	    BUS_DMA_WRITE | BUS_DMA_NOWAIT);
	if (rv == EFBIG && m_defrag(m, M_DONTWAIT) == 0)
		rv = bus_dmamap_load_mbuf(sc->sc_dmat, md->md_map, m,
		    BUS_DMA_WRITE | BUS_DMA_NOWAIT);
	if (rv != 0) {
		DPRINTF("%s: %s: load failed rv %d len %u\n", DEVNAME(sc),
		    __func__, rv, (unsigned)mbuf_pkthdr_len(m));
		goto done;
	}

	nsegs = md->md_map->dm_nsegs;

	/* check if there is enough space */
	if ((nsegs + 1)/2 > mwx_dma_available_slots(q)) {
		bus_dmamap_unload(sc->sc_dmat, md->md_map);
		rv = EBUSY;
		goto done;
	}

	bus_dmamap_sync(sc->sc_dmat, md->md_map, 0, md->md_map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);
	md->md_mbuf = m;
	md->md_txwi = NULL;

	/*
	 * The segment list belongs to the map loaded above, not to the
	 * slot the loop is about to fill next: md is advanced at the end
	 * of each iteration, so dereferencing md->md_map while writing
	 * descriptor i corrupted every descriptor after the first one
	 * unless the packet happened to have two segments at most.
	 */
	map = md->md_map;

	bus_dmamap_sync(sc->sc_dmat, q->mq_map, 0, q->mq_map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);
	for (i = 0; i < nsegs; i += 2) {
		uint32_t	buf0, buf1 = 0;
		uint32_t	len0, len1 = 0, ctrl;

		KASSERT(map->dm_segs[i].ds_addr <= UINT32_MAX);
		buf0 = map->dm_segs[i].ds_addr;
		len0 = map->dm_segs[i].ds_len;
		ctrl = MT_DMA_CTL_SD_LEN0(len0);

		if (i < nsegs - 1) {
			KASSERT(map->dm_segs[i + 1].ds_addr <= UINT32_MAX);
			buf1 = map->dm_segs[i + 1].ds_addr;
			len1 = map->dm_segs[i + 1].ds_len;
			ctrl |= MT_DMA_CTL_SD_LEN1(len1);
		}

		if (i == nsegs - 1)
			ctrl |= MT_DMA_CTL_LAST_SEC0;
		else if (i == nsegs - 2)
			ctrl |= MT_DMA_CTL_LAST_SEC1;

		desc->buf0 = htole32(buf0);
		desc->buf1 = htole32(buf1);
		desc->info = 0;
		desc->ctrl = htole32(ctrl);

		idx = (idx + 1) % q->mq_count;
		KASSERT(idx != q->mq_cons);
		md = &q->mq_data[idx];
		desc = &q->mq_desc[idx];
	}
	bus_dmamap_sync(sc->sc_dmat, q->mq_map, 0, q->mq_map->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE);

	q->mq_prod = idx;

	mwx_write(sc, q->mq_regbase + MT_DMA_CPU_IDX, q->mq_prod);

	rv = 0;
done:
	mwx_tx_unlock(sc);

	return rv;
}

void
mwx_dma_txwi_enqueue(struct mwx_softc *sc, struct mwx_queue *q,
    struct mwx_txwi *mt)
{
	struct mwx_queue_data *md;
	struct mwx_desc *desc;
	uint32_t buf0, len0, ctrl;
	int idx;

	mwx_tx_lock(sc);

	idx = q->mq_prod;
	md = &q->mq_data[idx];
	desc = &q->mq_desc[idx];

	md->md_txwi = mt;
	md->md_mbuf = NULL;

	bus_dmamap_sync(sc->sc_dmat, q->mq_map, 0, q->mq_map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	buf0 = mt->mt_addr;
	len0 = sizeof(*mt->mt_desc);
	ctrl = MT_DMA_CTL_SD_LEN0(len0);
	ctrl |= MT_DMA_CTL_LAST_SEC0;

	desc->buf0 = htole32(buf0);
	desc->buf1 = 0;
	desc->info = 0;
	desc->ctrl = htole32(ctrl);

	idx = (idx + 1) % q->mq_count;
	KASSERT(idx != q->mq_cons);

	bus_dmamap_sync(sc->sc_dmat, q->mq_map, 0, q->mq_map->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE);

	q->mq_prod = idx;

	mwx_write(sc, q->mq_regbase + MT_DMA_CPU_IDX, q->mq_prod);

	mwx_tx_unlock(sc);
}

void
mwx_dma_tx_cleanup(struct mwx_softc *sc, struct mwx_queue *q)
{
	struct mwx_queue_data *md;
	struct mwx_desc *desc;
	u_int left;
	int idx, last;

	/*
	 * Only descriptors that were actually enqueued can be completed.
	 * The DMA index register alone is not a safe bound: WFDMA
	 * prefetches, so it can point past mq_prod, and after a ring reset
	 * it may still hold a value left behind by the bootloader.
	 * Walking past mq_prod used to move mq_cons beyond it, after which
	 * the ring looked permanently full and the firmware download
	 * wedged ("queue still full ... prod 3 cons 4").
	 */
	left = mwx_dma_outstanding(q);
	if (left == 0)
		return;

	idx = q->mq_cons;
	last = mwx_read(sc, q->mq_regbase + MT_DMA_DMA_IDX);
	if (last >= q->mq_count)
		return;
	if (idx == last)
		return;

	bus_dmamap_sync(sc->sc_dmat, q->mq_map, 0, q->mq_map->dm_mapsize,
	     BUS_DMASYNC_PREWRITE);

	while (left != 0 && idx != last) {
		md = &q->mq_data[idx];
		desc = &q->mq_desc[idx];

		if (md->md_mbuf != NULL) {
			bus_dmamap_sync(sc->sc_dmat, md->md_map, 0,
			    md->md_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, md->md_map);
			m_freem(md->md_mbuf);
			md->md_mbuf = NULL;
		}
		if (md->md_txwi != NULL) {
			/* nothing here, cleanup via mwx_mac_tx_free() */
			md->md_txwi = NULL;
		}

		/* clear DMA desc just to be sure */
		desc->buf0 = 0;
		desc->buf1 = 0;
		desc->info = 0;
		desc->ctrl = htole32(MT_DMA_CTL_DMA_DONE);

		idx = (idx + 1) % q->mq_count;
		left--;

		/* check if more data made it in */
		/* XXX should we actually do that? */
		if (idx == last)
			last = mwx_read(sc, q->mq_regbase + MT_DMA_DMA_IDX);
	}

	bus_dmamap_sync(sc->sc_dmat, q->mq_map, 0, q->mq_map->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE);

	q->mq_cons = idx;
	if (q->mq_wakeme) {
		q->mq_wakeme = 0;
		wakeup(q);
	}
}

void
mwx_dma_tx_done(struct mwx_softc *sc)
{
	mwx_dma_tx_cleanup(sc, &sc->sc_txq);
	mwx_dma_tx_cleanup(sc, &sc->sc_txmcuq);
	mwx_dma_tx_cleanup(sc, &sc->sc_txfwdlq);

	/* XXX do we need to wakeup someone */
}

/*
 * Hand the frames collected from the receive rings to net80211.
 *
 * Runs from mwx_dma_rx_deliver() only, i.e. with the workloop gate held and
 * with no ring lock.  Splitting this from the ring drain is deliberate:
 * everything here can call into net80211, the command gate and the network
 * stack, none of which may happen while sc_ring_lock is held.
 */
/* XXX wrong place */
void
mwx_dma_rx_process(struct mwx_softc *sc, struct mbuf_list *ml, int deliver)
{
	struct mbuf_list mlout = MBUF_LIST_INITIALIZER();
	struct mbuf_list mldef = MBUF_LIST_INITIALIZER();
	struct mbuf *m;
	uint32_t *data, rxd, type, flag;

	while ((m = ml_dequeue(ml)) != NULL) {
		data = mtod(m, uint32_t *);
		rxd = le32toh(data[0]);

		type = MT_RXD0_PKT_TYPE_GET(rxd);
		flag = (rxd & MT_RXD0_PKT_FLAG_MASK) >> MT_RXD0_PKT_FLAG_SHIFT;

		if (sc->sc_hwtype == MWX_HW_MT7925 && type != PKT_TYPE_NORMAL) {
			uint32_t sw_type;

			sw_type = MT_RXD0_SW_PKT_TYPE_GET(rxd);
			if ((sw_type & MT_RXD0_SW_PKT_TYPE_MAP) ==
			    MT_RXD0_SW_PKT_TYPE_FRAME)
				type = PKT_TYPE_NORMAL;
		}

		if (type == PKT_TYPE_RX_EVENT && flag == 0x1)
			type = PKT_TYPE_NORMAL_MCU;

		switch (type) {
		case PKT_TYPE_RX_EVENT:
			mwx_mcu_rx_event(sc, m);
			break;
		case PKT_TYPE_TXRX_NOTIFY:
			if (sc->sc_hwtype == MWX_HW_MT7925)
				mt7925_mac_tx_free(sc, m);
			else
				mt7921_mac_tx_free(sc, m);
			break;
		case PKT_TYPE_TXS:
#if TODO
			for (rxd += 2; rxd + 8 <= end; rxd += 8)
				mt7921_mac_add_txs(dev, rxd);
#endif
			m_freem(m);
			break;
		case PKT_TYPE_NORMAL_MCU:
		case PKT_TYPE_NORMAL:
			/*
			 * A nested call (a wait loop polling the rings while
			 * net80211 is already processing a frame) must not
			 * enter net80211 at all: that would re-enter the
			 * state machine and reorder the receive stream.  Park
			 * the frame for the outermost delivery loop instead;
			 * everything the caller can be waiting for - MCU
			 * replies and TX completions - is handled above.
			 */
			if (!deliver) {
				ml_enqueue(&mldef, m);
				break;
			}
			mwx_rx(sc, m, &mlout);
			break;
		default:
			if (DEVDEBUG(sc))
				printf("%s: received unknown pkt type %d\n",
				    DEVNAME(sc), type);
			m_freem(m);
			break;
		}
	}

	/*
	 * Hand the deferred frames to the outermost delivery loop.
	 *
	 * mldef is non-empty only for a nested call, and the list is newer
	 * than anything the outer loop has already delivered, so appending
	 * keeps the receive stream in order.
	 */
	if (!ml_empty(&mldef)) {
		mwx_ring_lock(sc);
		ml_enlist(&sc->sc_rx_deferred, &mldef);
		mwx_ring_unlock(sc);
	}

	/*
	 * Hand the data frames to the stack.
	 *
	 * if_input() is a workloop command gate round trip; skip it when
	 * net80211 did not queue anything (a TX-free notification or an
	 * MCU event is enough to fill the list without producing data).
	 * mlout is only ever filled on the outermost call, but the check
	 * is cheap and keeps the invariant local.
	 */
	if (ml_empty(&mlout))
		return;

	/*
	 * Nothing can be delivered before the command gate that serialises
	 * the stack exists (the interface is not published either).
	 */
	if (_fCommandGate == NULL) {
		ml_purge(&mlout);
		return;
	}

	if_input(&sc->sc_ic.ic_if, &mlout);
}

void
mwx_dma_rx_dequeue(struct mwx_softc *sc, struct mwx_queue *q,
    struct mbuf_list *ml)
{
	struct mwx_queue_data *md;
	struct mwx_desc *desc;
	struct mbuf *m, *m0, *mtail;
	int idx, last;

	idx = q->mq_cons;
	last = mwx_read(sc, q->mq_regbase + MT_DMA_DMA_IDX);

	if (idx == last)
		return;

	bus_dmamap_sync(sc->sc_dmat, q->mq_map, 0, q->mq_map->dm_mapsize,
	     BUS_DMASYNC_PREREAD);

	m0 = q->mq_mbuf;
	mtail = q->mq_mtail;

	while (idx != last) {
		uint32_t ctrl;

		md = &q->mq_data[idx];
		desc = &q->mq_desc[idx];

		bus_dmamap_sync(sc->sc_dmat, md->md_map, 0,
		    md->md_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, md->md_map);

		/* dequeue mbuf */
		m = md->md_mbuf;
		md->md_mbuf = NULL;

		/* only buf0 is used on RX rings */
		ctrl = le32toh(desc->ctrl);
		mbuf_setlen(m, MT_DNA_CTL_SD_GET_LEN0(ctrl));

		if (m0 == NULL) {
			m0 = mtail = m;
			mbuf_pkthdr_setlen(m0, mbuf_len(m));
		} else {
			mbuf_setnext(mtail, m);
			mtail = m;
			mbuf_pkthdr_setlen(m0, mbuf_pkthdr_len(m0) + mbuf_len(m));
		}

		/* TODO handle desc->info */

		/* check if this was the last mbuf of the chain */
		if (ctrl & MT_DMA_CTL_LAST_SEC0) {
			ml_enqueue(ml, m0);
			m0 = NULL;
			mtail = NULL;
		}

		idx = (idx + 1) % q->mq_count;

		/* check if more data made it in */
		/* XXX should we actually do that? */
		if (idx == last)
			last = mwx_read(sc, q->mq_regbase + MT_DMA_DMA_IDX);
	}

	q->mq_mbuf = m0;
	q->mq_mtail = mtail;

	bus_dmamap_sync(sc->sc_dmat, q->mq_map, 0, q->mq_map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD);

	q->mq_cons = idx;
}

/*
 * Take completed frames off one receive ring.  Must be called with
 * sc_ring_lock held; the frames are only queued for mwx_dma_rx_deliver().
 */
void
mwx_dma_rx_done(struct mwx_softc *sc, struct mwx_queue *q)
{
	mwx_dma_rx_dequeue(sc, q, &sc->sc_rx_pending);
}

struct mbuf *
mwx_mcu_alloc_msg(size_t len)
{
	const int headspace = sizeof(struct mwx_mcu_txd);
	struct mbuf *m;

	/* Allocate mbuf with enough space */
	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return NULL;
	if (len + headspace > MHLEN) {
		m_clget(m, M_DONTWAIT, len + headspace);
		if (!ISSET(mbuf_flags(m), M_EXT)) {
			m_freem(m);
			return NULL;
		}
	}

	m_align(m, len + headspace);
	mbuf_setlen(m, len + headspace);
	mbuf_pkthdr_setlen(m, len + headspace);
	memset(mtod(m, caddr_t), 0, mbuf_len(m));
	m_adj(m, headspace);

	return m;
}

void
mwx_mcu_set_len(struct mbuf *m, void *end)
{
	caddr_t start = (caddr_t)mtod(m, void *);
	caddr_t stop = (caddr_t)end;
	int len = mbuf_pkthdr_len(m), adj;

	KASSERT(start <= stop && stop - start <= len);
	adj = len - (int)(stop - start);
	m_adj(m, -adj);
}

int
mwx_mcu_send_mbuf(struct mwx_softc *sc, uint32_t cmd, struct mbuf *m, int *seqp)
{
	struct mwx_uni_txd *uni_txd;
	struct mwx_mcu_txd *mcu_txd;
	struct mwx_queue *q;
	uint32_t *txd, val;
	int s, rv, i, mcu_cmd = cmd & MCU_CMD_FIELD_ID_MASK;
	int tot_len, txd_len, len = mbuf_pkthdr_len(m);
	uint8_t seq;

	if (cmd == MCU_CMD_FW_SCATTER) {
		q = &sc->sc_txfwdlq;
		KASSERT(seqp == NULL);
		goto enqueue;
	}

	seq = ++sc->sc_mcu_seq & 0x0f;
	if (seq == 0)
		seq = ++sc->sc_mcu_seq & 0x0f;

	KASSERT(seq < nitems(sc->sc_mcu_wait));

	txd_len = cmd & MCU_CMD_FIELD_UNI ? sizeof(*uni_txd) : sizeof(*mcu_txd);
	tot_len = txd_len + len;
	KASSERT(m_leadingspace(m) >= txd_len);
	m = m_prepend(m, txd_len, M_DONTWAIT);
	txd = mtod(m, uint32_t *);
	memset(txd, 0, txd_len);

	val = (tot_len & MT_TXD0_TX_BYTES_MASK) |
	    MT_TX_TYPE_CMD | MT_TXD0_Q_IDX(MT_TX_MCU_PORT_RX_Q0);
	txd[0] = htole32(val);

	if (sc->sc_hwtype == MWX_HW_MT7925)
		val = MT7925_HDR_FORMAT_CMD;
	else
		val = MT_TXD1_LONG_FORMAT | MT_HDR_FORMAT_CMD;
	txd[1] = htole32(val);

	if (cmd & MCU_CMD_FIELD_UNI) {
		uni_txd = (struct mwx_uni_txd *)txd;
		uni_txd->len = htole16(tot_len - sizeof(uni_txd->txd));
		if (sc->sc_hwtype == MWX_HW_MT7925) {
			if (cmd & MCU_CMD_FIELD_QUERY)
				uni_txd->option = MCU_CMD_UNI_QUERY_ACK;
			else
				uni_txd->option = MCU_CMD_UNI_EXT_ACK;
			/* Non-QUERY CHIP_CONFIG/HIF_CTRL must NOT have ACK */
			if (cmd == MCU_UNI_CMD_HIF_CTRL ||
			    cmd == MCU_UNI_CMD_CHIP_CONFIG)
				uni_txd->option &= ~MCU_CMD_ACK;
		} else {
			uni_txd->option = MCU_CMD_UNI_EXT_ACK;
		}
		uni_txd->cid = htole16(mcu_cmd);
		uni_txd->s2d_index = CMD_S2D_IDX_H2N;
		uni_txd->pkt_type = MCU_PKT_ID;
		uni_txd->seq = seq;
	} else {
		mcu_txd = (struct mwx_mcu_txd *)txd;
		mcu_txd->len = htole16(tot_len - sizeof(uni_txd->txd));
		mcu_txd->pq_id = htole16(MCU_PQ_ID(MT_TX_PORT_IDX_MCU,
			MT_TX_MCU_PORT_RX_Q0));
		mcu_txd->pkt_type = MCU_PKT_ID;
		mcu_txd->seq = seq;
		mcu_txd->cid = mcu_cmd;
		mcu_txd->s2d_index = CMD_S2D_IDX_H2N;
		mcu_txd->ext_cid = MCU_GET_EXT_CMD(cmd);

		if (mcu_txd->ext_cid || (cmd & MCU_CMD_FIELD_CE)) {
			if (cmd & MCU_CMD_FIELD_QUERY)
				mcu_txd->set_query = MCU_Q_QUERY;
			else
				mcu_txd->set_query = MCU_Q_SET;
			mcu_txd->ext_cid_ack = !!mcu_txd->ext_cid;
		} else {
			mcu_txd->set_query = MCU_Q_NA;
		}
	}

	if (seqp != NULL) {
		memset(&sc->sc_mcu_wait[seq], 0, sizeof(sc->sc_mcu_wait[0]));
		sc->sc_mcu_wait[seq].mcu_cmd = cmd;
		*seqp = seq;
	}
	q = &sc->sc_txmcuq;
enqueue:

	s = splnet();
	/*
	 * A full ring is drained by mwx_dma_tx_cleanup(), which also
	 * wakes us -- but only if its completion interrupt is delivered.
	 * Drain the ring here as well, so a lost interrupt cannot wedge
	 * the caller.  Bound the wait anyway: if the device index stops
	 * moving, looping here forever hangs the caller (and, during
	 * attach, the whole boot).
	 */
	for (i = 0; ; i++) {
		rv = mwx_dma_tx_enqueue(sc, q, m);
		if (rv != EBUSY)
			break;

		mwx_dma_poll(sc);

		rv = mwx_dma_tx_enqueue(sc, q, m);
		if (rv != EBUSY)
			break;

		if (i == 30) {
			q->mq_wakeme = 0;
			printf("%s: %s: queue still full after 3s "
			    "(prod %u cons %u)\n", DEVNAME(sc), __func__,
			    q->mq_prod, q->mq_cons);
			mwx_mcu_dump_state(sc, "queue full");
			break;
		}
		q->mq_wakeme = 1;
		mwx_tsleep_nsec(q, 0, "mwxq", MSEC_TO_NSEC(100));
	}
	splx(s);
	if (rv != 0) {
		/*
		 * FW_SCATTER never allocates a wait slot, so seq is not
		 * initialised on that path.  Only clear the slot when one
		 * was actually claimed above.
		 */
		if (seqp != NULL)
			memset(&sc->sc_mcu_wait[seq], 0,
			    sizeof(sc->sc_mcu_wait[0]));
		m_freem(m);
	}
	return rv;
}

int
mwx_mcu_send_msg(struct mwx_softc *sc, uint32_t cmd, void *data, size_t len,
    int *seqp)
{
	struct mbuf *m;

	m = mwx_mcu_alloc_msg(len);
	if (m == NULL)
		return ENOMEM;

	if (len != 0)
		memcpy(mtod(m, caddr_t), data, len);

	return mwx_mcu_send_mbuf(sc, cmd, m, seqp);
}

int
mwx_mcu_send_wait(struct mwx_softc *sc, uint32_t cmd, void *data, size_t len)
{
	int rv, seq;

	rv = mwx_mcu_send_msg(sc, cmd, data, len, &seq);
	if (rv != 0)
		return rv;
	return mwx_mcu_wait_resp_int(sc, cmd, seq, NULL);
}

/*
 * Send a command from a context that holds the workloop gate (the
 * receive path) and wait for the firmware's status word.
 *
 * The wait is bounded much tighter than the generic three seconds on
 * purpose: every millisecond spent here stalls every other event source
 * on the workloop, including the airport ioctls behind the Wi-Fi UI, so
 * a firmware that never answers would freeze the interface instead of
 * reporting a failure.  The firmware answers STA_REC_UPDATE in well
 * under a millisecond; a quarter of a second is already generous.
 */
#define	MWX_MCU_WAIT_MS		5
#define	MWX_MCU_TIMEOUT_MS	3000
#define	MWX_MCU_KEY_TIMEOUT_MS	250

int
mwx_mcu_send_mbuf_wait_status(struct mwx_softc *sc, uint32_t cmd,
    struct mbuf *m, uint32_t *status, int ms)
{
	int rv, seq;

	rv = mwx_mcu_send_mbuf(sc, cmd, m, &seq);
	if (rv != 0)
		return rv;
	return mwx_mcu_wait_resp_int_ms(sc, cmd, seq, status, ms);
}

int
mwx_mcu_send_mbuf_wait(struct mwx_softc *sc, uint32_t cmd, struct mbuf *m)
{
	return mwx_mcu_send_mbuf_wait_status(sc, cmd, m, NULL,
	    MWX_MCU_TIMEOUT_MS);
}

void
mwx_mcu_rx_event(struct mwx_softc *sc, struct mbuf *m)
{
	struct mwx_mcu_rxd *rxd;
	uint32_t cmd, mcu_int = 0;
	int len, rxd_size;

	rxd_size = (sc->sc_hwtype == MWX_HW_MT7925) ?
	    MT7925_MCU_RXD_SIZE : MT7921_MCU_RXD_SIZE;

	if ((m = m_pullup(m, sizeof(*rxd) + rxd_size)) == NULL)
		return;
	m_adj(m, rxd_size);
	rxd = mtod(m, struct mwx_mcu_rxd *);

	if (rxd->ext_eid == MCU_EXT_EVENT_RATE_REPORT) {
		m_freem(m);
		return;
	}

	len = le16toh(rxd->len);
	/* make sure all the data is in one mbuf */
	if ((m = m_pullup(m, len)) == NULL) {
		printf("%s: mwx_mcu_rx_event m_pullup failed\n", DEVNAME(sc));
		return;
	}
	/* refetch after pullup */
	rxd = mtod(m, struct mwx_mcu_rxd *);
	m_adj(m, sizeof(*rxd));

	switch (rxd->eid) {
	case MCU_EVENT_SCHED_SCAN_DONE:
	case MCU_EVENT_SCAN_DONE:
		mt7921_mcu_scan_event(sc, m);
		break;
#if 0
	case MCU_EVENT_BSS_BEACON_LOSS:
		mt7921_mcu_connection_loss_event(dev, skb);
		break;
	case MCU_EVENT_BSS_ABSENCE:
		mt7921_mcu_bss_event(dev, skb);
		break;
	case MCU_EVENT_DBG_MSG:
		mt7921_mcu_debug_msg_event(dev, skb);
		break;
#endif
	case MCU_EVENT_COREDUMP:
		if (!sc->sc_coredump_cnt)
			printf("%s: coredump event\n", DEVNAME(sc));
		sc->sc_coredump_cnt++;
		timeout_add_msec(&sc->sc_reset_to, 50);
		break;
	case MCU_EVENT_LP_INFO:
		mt7921_mcu_low_power_event(sc, m);
		break;
	case MCU_EVENT_TX_DONE:
		mt7921_mcu_tx_done_event(sc, m);
		break;
	case 0x6:
	default:
		/*
		 * Events that do not answer a command have no sequence
		 * number of their own.  The firmware marks most of them
		 * with MCU_UNI_CMD_UNSOLICITED_EVENT, but not all:
		 * connection loss, beacon loss and RSSI notifications
		 * arrive with seq 0 and the option bit clear.  Linux
		 * decides on the event id as well (mt7921_mcu_rx_event()),
		 * because treating such an event as an unmatched reply
		 * drops it -- and a dropped beacon-loss event leaves
		 * net80211 in RUN on a BSS the firmware has already
		 * abandoned, which looks exactly like a connected
		 * interface that cannot pass traffic.
		 */
		if ((rxd->option & MCU_UNI_CMD_UNSOLICITED_EVENT) ||
		    rxd->eid == MCU_EVENT_BSS_BEACON_LOSS ||
		    rxd->eid == MCU_EVENT_BSS_ABSENCE ||
		    rxd->seq == 0) {
			mwx_mcu_rx_unsolicited_event(sc, rxd, m);
			break;
		}

		if (rxd->seq == 0 || rxd->seq >= nitems(sc->sc_mcu_wait)) {
			printf("%s: mcu rx bad seq %x, eid %x ext_eid %x, "
			    "opt %x len %u\n",
			    DEVNAME(sc), rxd->seq, rxd->eid, rxd->ext_eid,
			    rxd->option, le16toh(rxd->len));
			break;
		}

		cmd = sc->sc_mcu_wait[rxd->seq].mcu_cmd;
		if (cmd == 0)
			break;

		if (cmd == MCU_CMD_PATCH_SEM_CONTROL ||
		    cmd == MCU_CMD_PATCH_FINISH_REQ) {
			/*
			 * The MCU puts the status of these two commands in
			 * ext_eid, not in the payload.  Upstream mt76 reads
			 * it by pulling the skb down to that byte
			 * (sizeof(*rxd) - 4).  The old code did the same
			 * with m_prepend(), which only works when the mbuf
			 * has four bytes of leading space; on the macOS RX
			 * path it does not, so m_prepend() allocated a
			 * fresh zeroed mbuf and the status always read 0.
			 */
			mcu_int = rxd->ext_eid;
		} else if (cmd == MCU_EXT_CMD_THERMAL_CTRL) {
			if (mbuf_len(m) < sizeof(uint32_t) * 2)
				break;
			mcu_int = le32toh(mtod(m, uint32_t *)[1]);
		} else if (cmd == MCU_EXT_CMD_EFUSE_ACCESS) {
			//ret = mt7921_mcu_parse_eeprom(sc, m);
			if (DEVDEBUG(sc))
				printf("%s: efuse response not handled\n",
				    DEVNAME(sc));
		} else if (cmd == MCU_UNI_CMD_DEV_INFO_UPDATE ||
		    cmd == MCU_UNI_CMD_BSS_INFO_UPDATE ||
		    cmd == MCU_UNI_CMD_STA_REC_UPDATE ||
		    cmd == MCU_UNI_CMD_HIF_CTRL ||
		    cmd == MCU_UNI_CMD_OFFLOAD ||
		    cmd == MCU_UNI_CMD_SUSPEND) {
			struct mt7921_mcu_uni_event *event;

			if (mbuf_len(m) < sizeof(*event))
				break;
			event = mtod(m, struct mt7921_mcu_uni_event *);
			mcu_int = le32toh(event->status);
		} else if (cmd == MCU_CE_QUERY_REG_READ) {
			struct mt7921_mcu_reg_event *event;

			if (mbuf_len(m) < sizeof(*event))
				break;
			event = mtod(m, struct mt7921_mcu_reg_event *);
			mcu_int = le32toh(event->val);
		}

		if (DEVDEBUG(sc))
			printf("%s: mcu rx event, seq %x eid %x "
			    "ext_eid %x, opt %x len %u, cmd %x, "
			    "mcu_int %d\n", DEVNAME(sc), rxd->seq,
			    rxd->eid, rxd->ext_eid, rxd->option,
			    le16toh(rxd->len), cmd, mcu_int);

		sc->sc_mcu_wait[rxd->seq].mcu_int = mcu_int;
		sc->sc_mcu_wait[rxd->seq].mcu_m = m;
		wakeup(&sc->sc_mcu_wait[rxd->seq]);
		return;
	}

	m_freem(m);
}

void
mwx_mcu_rx_unsolicited_event(struct mwx_softc *sc, struct mwx_mcu_rxd *rxd,
    struct mbuf *m)
{
	switch (rxd->eid) {
	case MCU_UNI_EVENT_SCAN_DONE:
		mt7925_mcu_scan_event(sc, m);
		break;
	case MCU_UNI_EVENT_TX_DONE:
		/* ignore for now */
		break;
	case MCU_EVENT_BSS_BEACON_LOSS:
	case MCU_EVENT_BSS_ABSENCE:
	case MCU_UNI_EVENT_BSS_BEACON_LOSS:
		/*
		 * The firmware stopped hearing the AP.
		 *
		 * This does not have to be the end of the association: a
		 * long off-channel scan is enough to trip it.  Do what
		 * iwm/iwx do and probe the AP from a task (the receive path
		 * must not drive the 802.11 state machine directly).  A
		 * probe response resets the management timer and the link
		 * is kept; silence lets ieee80211_watchdog() move us to
		 * S_SCAN, so a BSS that is really gone is replaced by a
		 * fresh association instead of leaving net80211 in RUN on
		 * a dead link.
		 */
		task_add(sc->sc_nswq, &sc->sc_bmiss_task);
		break;
	case MCU_EVENT_DBG_MSG:
		/* firmware debug log, only useful with a debugfs equivalent */
		break;
	default:
		if (DEVDEBUG(sc))
			printf("%s: mcu rx unsolicited event, eid %x "
			    "ext_eid %x, opt %x len %u\n",
			    DEVNAME(sc), rxd->eid, rxd->ext_eid,
			    rxd->option, le16toh(rxd->len));
		break;
	}
}

/*
 * Describe where an MCU command got stuck: whether a reply is sitting
 * unread in the MCU RX ring, whether the WFDMA still considers this
 * queue busy, and whether the firmware flagged an error.
 */
static void
mwx_mcu_dump_state(struct mwx_softc *sc, const char *why)
{
	printf("%s: %s: INT_STA %08x INT_ENA %08x MCU_CMD %08x MISC %08x\n",
	    DEVNAME(sc), why,
	    mwx_read(sc, MT_WFDMA0_HOST_INT_STA),
	    mwx_read(sc, MT_WFDMA0_HOST_INT_ENA),
	    mwx_read(sc, MT_MCU_CMD),
	    mwx_read(sc, MT_CONN_ON_MISC));
	printf("%s: %s: intr %u filt %u PCIE_INT %08x GLO_CFG %08x pm %d\n",
	    DEVNAME(sc), why, sc->sc_intr_count, sc->sc_intr_filter,
	    mwx_read(sc, MT_PCIE_MAC_INT_ENABLE),
	    mwx_read(sc, MT_WFDMA0_GLO_CFG),
	    sc->sc_pm_suspended);
	printf("%s: %s: mcu seq %u | rx event cons %u dev %u | "
	    "rx wa cons %u dev %u | txmcu prod %u cons %u dev %u | "
	    "fwdl prod %u cons %u dev %u | tx prod %u cons %u dev %u\n",
	    DEVNAME(sc), why, sc->sc_mcu_seq,
	    sc->sc_rxfwdlq.mq_cons,
	    mwx_read(sc, sc->sc_rxfwdlq.mq_regbase + MT_DMA_DMA_IDX),
	    sc->sc_rxmcuq.mq_cons,
	    mwx_read(sc, sc->sc_rxmcuq.mq_regbase + MT_DMA_DMA_IDX),
	    sc->sc_txmcuq.mq_prod, sc->sc_txmcuq.mq_cons,
	    mwx_read(sc, sc->sc_txmcuq.mq_regbase + MT_DMA_DMA_IDX),
	    sc->sc_txfwdlq.mq_prod, sc->sc_txfwdlq.mq_cons,
	    mwx_read(sc, sc->sc_txfwdlq.mq_regbase + MT_DMA_DMA_IDX),
	    sc->sc_txq.mq_prod, sc->sc_txq.mq_cons,
	    mwx_read(sc, sc->sc_txq.mq_regbase + MT_DMA_DMA_IDX));
}

/*
 * A lost wakeup and a missing reply look identical once tsleep has
 * expired, so give the RX ring one last look before declaring failure.
 * Returns 0 if the reply was there after all.
 */
static int
mwx_mcu_resp_recover(struct mwx_softc *sc, uint32_t cmd, int seq)
{
	/*
	 * Give the rings one more chance to hand us the reply.  This also
	 * acknowledges a status bit whose interrupt was never delivered,
	 * which would otherwise keep suppressing further edges (MSI) and
	 * make the next command time out exactly the same way.
	 */
	mwx_dma_poll(sc);

	if (sc->sc_mcu_wait[seq].mcu_m == NULL) {
		printf("%s: no reply for cmd %x in MCU RX ring\n",
		    DEVNAME(sc), cmd);
		return ETIMEDOUT;
	}

	printf("%s: cmd %x reply was still in the MCU RX ring\n",
	    DEVNAME(sc), cmd);
	return 0;
}

/*
 * Wait for the MCU to publish a reply into sc_mcu_wait[seq].
 *
 * Keeps the upstream three-second budget by default, but wakes in short
 * slices and polls the rings in between (mwx_dma_poll()) instead of
 * trusting that the completion interrupt will arrive: the action may be
 * blocked behind mwx_activate() on the workloop, or the MSI may simply
 * have been lost.
 */
static int
mwx_mcu_wait_slot(struct mwx_softc *sc, int seq, int ms)
{
	int i;

	for (i = 0; i * MWX_MCU_WAIT_MS < ms; i++) {
		if (sc->sc_mcu_wait[seq].mcu_m != NULL)
			return 0;
		/*
		 * Service the rings ourselves: the reply may be sitting in
		 * one of them because its completion interrupt was never
		 * delivered.  This is also what keeps init synchronous
		 * while mwx_activate() holds the workloop.
		 */
		mwx_dma_poll(sc);
		if (sc->sc_mcu_wait[seq].mcu_m != NULL)
			return 0;
		mwx_tsleep_nsec(&sc->sc_mcu_wait[seq], 0, "mwxwait",
		    MSEC_TO_NSEC(MWX_MCU_WAIT_MS));
	}

	return ETIMEDOUT;
}

int
mwx_mcu_wait_resp_int_ms(struct mwx_softc *sc, uint32_t cmd, int seq,
    uint32_t *val, int ms)
{
	int rv;

	KASSERT(seq < nitems(sc->sc_mcu_wait));

	rv = mwx_mcu_wait_slot(sc, seq, ms);
	if (rv != 0) {
		printf("%s: command %x timeout\n", DEVNAME(sc), cmd);
		mwx_mcu_dump_state(sc, "timeout");
		if (mwx_mcu_resp_recover(sc, cmd, seq) != 0) {
			mwx_reset(sc);
			return rv;
		}
	}

	if (sc->sc_mcu_wait[seq].mcu_m != NULL) {
		m_freem(sc->sc_mcu_wait[seq].mcu_m);
		sc->sc_mcu_wait[seq].mcu_m = NULL;
	}
	if (val != NULL)
		*val = sc->sc_mcu_wait[seq].mcu_int;
	return 0;
}

int
mwx_mcu_wait_resp_int(struct mwx_softc *sc, uint32_t cmd, int seq,
    uint32_t *val)
{
	return mwx_mcu_wait_resp_int_ms(sc, cmd, seq, val,
	    MWX_MCU_TIMEOUT_MS);
}

int
mwx_mcu_wait_resp_msg(struct mwx_softc *sc, uint32_t cmd, int seq,
    struct mbuf **mp)
{
	int rv;

	KASSERT(seq < nitems(sc->sc_mcu_wait));

	rv = mwx_mcu_wait_slot(sc, seq, MWX_MCU_TIMEOUT_MS);
	if (rv != 0) {
		printf("%s: command %x timeout\n", DEVNAME(sc), cmd);
		mwx_mcu_dump_state(sc, "timeout");
		if (mwx_mcu_resp_recover(sc, cmd, seq) != 0) {
			mwx_reset(sc);
			return rv;
		}
	}
	if (sc->sc_mcu_wait[seq].mcu_m == NULL) {
		printf("%s: command response missing\n", DEVNAME(sc));
		return ENOENT;
	}
	if (mp != NULL)
		*mp = sc->sc_mcu_wait[seq].mcu_m;
	else
		m_freem(sc->sc_mcu_wait[seq].mcu_m);
	sc->sc_mcu_wait[seq].mcu_m = NULL;
	return 0;
}

int
mwx_dma_disable(struct mwx_softc *sc, int force)
{
	int rv;

	/* disable WFDMA0 */
	mwx_clear(sc, MT_WFDMA0_GLO_CFG,
	    MT_WFDMA0_GLO_CFG_TX_DMA_EN | MT_WFDMA0_GLO_CFG_RX_DMA_EN |
	    MT_WFDMA0_GLO_CFG_CSR_DISP_BASE_PTR_CHAIN_EN |
	    MT_WFDMA0_GLO_CFG_OMIT_TX_INFO |
	    MT_WFDMA0_GLO_CFG_OMIT_RX_INFO |
	    MT_WFDMA0_GLO_CFG_OMIT_RX_INFO_PFET2);

	rv = mwx_poll(sc, MT_WFDMA0_GLO_CFG, 0,
	    MT_WFDMA0_GLO_CFG_TX_DMA_BUSY | MT_WFDMA0_GLO_CFG_RX_DMA_BUSY, 100);
	if (!force && rv != 0)
		return rv;

	/* disable DMASHDL and set bypass */
	mwx_clear(sc, MT_WFDMA0_GLO_CFG_EXT0, MT_WFDMA0_CSR_TX_DMASHDL_ENABLE);
	mwx_set(sc, MT_DMASHDL_SW_CONTROL, MT_DMASHDL_DMASHDL_BYPASS);

	if (force) {
		/* reset */
		mwx_clear(sc, MT_WFDMA0_RST, MT_WFDMA0_RST_DMASHDL_ALL_RST |
		    MT_WFDMA0_RST_LOGIC_RST);
		mwx_set(sc, MT_WFDMA0_RST, MT_WFDMA0_RST_DMASHDL_ALL_RST |
		    MT_WFDMA0_RST_LOGIC_RST);
		delay(20);
	}

	return 0;
}

void
mwx_dma_enable(struct mwx_softc *sc)
{
#define P(base, depth)   ((base) << 16 | (depth))
	/* configure perfetch settings */
	if (sc->sc_hwtype == MWX_HW_MT7925) {
		mwx_write(sc, MT_WFDMA0_RX_RING0_EXT_CTRL, P(0x0, 0x4));
		mwx_write(sc, MT_WFDMA0_RX_RING1_EXT_CTRL, P(0x40, 0x4));
		mwx_write(sc, MT_WFDMA0_RX_RING2_EXT_CTRL, P(0x80, 0x4));
		mwx_write(sc, MT_WFDMA0_RX_RING3_EXT_CTRL, P(0xc0, 0x4));

		mwx_write(sc, MT_WFDMA0_TX_RING0_EXT_CTRL, P(0x100, 0x10));
		mwx_write(sc, MT_WFDMA0_TX_RING1_EXT_CTRL, P(0x200, 0x10));
		mwx_write(sc, MT_WFDMA0_TX_RING2_EXT_CTRL, P(0x300, 0x10));
		mwx_write(sc, MT_WFDMA0_TX_RING3_EXT_CTRL, P(0x400, 0x10));
		mwx_write(sc, MT_WFDMA0_TX_RING15_EXT_CTRL, P(0x500, 0x4));
		mwx_write(sc, MT_WFDMA0_TX_RING16_EXT_CTRL, P(0x540, 0x4));

	} else {
		mwx_write(sc, MT_WFDMA0_RX_RING0_EXT_CTRL, P(0x0, 0x4));
		mwx_write(sc, MT_WFDMA0_RX_RING2_EXT_CTRL, P(0x40, 0x4));
		mwx_write(sc, MT_WFDMA0_RX_RING3_EXT_CTRL, P(0x80, 0x4));
		mwx_write(sc, MT_WFDMA0_RX_RING4_EXT_CTRL, P(0xc0, 0x4));
		mwx_write(sc, MT_WFDMA0_RX_RING5_EXT_CTRL, P(0x100, 0x4));

		mwx_write(sc, MT_WFDMA0_TX_RING0_EXT_CTRL, P(0x140, 0x4));
		mwx_write(sc, MT_WFDMA0_TX_RING1_EXT_CTRL, P(0x180, 0x4));
		mwx_write(sc, MT_WFDMA0_TX_RING2_EXT_CTRL, P(0x1c0, 0x4));
		mwx_write(sc, MT_WFDMA0_TX_RING3_EXT_CTRL, P(0x200, 0x4));
		mwx_write(sc, MT_WFDMA0_TX_RING4_EXT_CTRL, P(0x240, 0x4));
		mwx_write(sc, MT_WFDMA0_TX_RING5_EXT_CTRL, P(0x280, 0x4));
		mwx_write(sc, MT_WFDMA0_TX_RING6_EXT_CTRL, P(0x2c0, 0x4));
		mwx_write(sc, MT_WFDMA0_TX_RING16_EXT_CTRL, P(0x340, 0x4));
		mwx_write(sc, MT_WFDMA0_TX_RING17_EXT_CTRL, P(0x380, 0x4));
	}
#undef P

	/* reset dma idx */
	mwx_write(sc, MT_WFDMA0_RST_DTX_PTR, ~0);
	if (sc->sc_hwtype == MWX_HW_MT7925)
		mwx_write(sc, MT_WFDMA0_RST_DRX_PTR, ~0);

	/* disable delayed interrupt */
	mwx_write(sc, MT_WFDMA0_PRI_DLY_INT_CFG0, 0);

	mwx_set(sc, MT_WFDMA0_GLO_CFG,
	    MT_WFDMA0_GLO_CFG_TX_WB_DDONE |
	    MT_WFDMA0_GLO_CFG_FIFO_LITTLE_ENDIAN |
	    MT_WFDMA0_GLO_CFG_CLK_GAT_DIS |
	    MT_WFDMA0_GLO_CFG_OMIT_TX_INFO |
	    MT_WFDMA0_GLO_CFG_DMA_SIZE(3) |
	    MT_WFDMA0_GLO_CFG_FIFO_DIS_CHECK |
	    MT_WFDMA0_GLO_CFG_RX_WB_DDONE |
	    MT_WFDMA0_GLO_CFG_CSR_DISP_BASE_PTR_CHAIN_EN |
	    MT_WFDMA0_GLO_CFG_OMIT_RX_INFO_PFET2);

	mwx_barrier(sc);
	mwx_set(sc, MT_WFDMA0_GLO_CFG,
	    MT_WFDMA0_GLO_CFG_TX_DMA_EN | MT_WFDMA0_GLO_CFG_RX_DMA_EN);

	if (sc->sc_hwtype == MWX_HW_MT7925) {
		/* enable extended DMA config */
		mwx_set(sc, MT_UWFDMA0_GLO_CFG_EXT1, (1U << 28));
		mwx_set(sc, MT_WFDMA0_INT_RX_PRI, 0x0f00);
		mwx_set(sc, MT_WFDMA0_INT_TX_PRI, 0x7f00);
	}

	mwx_set(sc, MT_WFDMA_DUMMY_CR, MT_WFDMA_NEED_REINIT);

	/* enable interrupts for TX/RX rings */
	mwx_write(sc, MT_WFDMA0_HOST_INT_ENA, sc->sc_intr_mask);
	/* maybe not needed */
	mwx_write(sc, MT_PCIE_MAC_INT_ENABLE, 0xff);
}

int
mwx_mcu_fw_pmctrl(struct mwx_softc *sc)
{
	int i;

	for (i = 0; i < MWX_MCU_INIT_RETRY_COUNT; i++) {
		mwx_write(sc, MT_CONN_ON_LPCTL, PCIE_LPCR_HOST_SET_OWN);
		if (mwx_poll(sc, MT_CONN_ON_LPCTL, PCIE_LPCR_HOST_OWN_SYNC,
		    4, 50) == 0)
			break;
	}

	if (i == MWX_MCU_INIT_RETRY_COUNT) {
		printf("%s: firmware own failed\n", DEVNAME(sc));
		return EIO;
	}

	return 0;
}

int
mwx_mcu_drv_pmctrl(struct mwx_softc *sc)
{
	int i;

	for (i = 0; i < MWX_MCU_INIT_RETRY_COUNT; i++) {
		mwx_write(sc, MT_CONN_ON_LPCTL, PCIE_LPCR_HOST_CLR_OWN);
		delay(3000);	/* 3ms for MT7925 */
		if (mwx_poll(sc, MT_CONN_ON_LPCTL, 0,
		    PCIE_LPCR_HOST_OWN_SYNC, 50) == 0)
			break;
	}

	if (i == MWX_MCU_INIT_RETRY_COUNT) {
		printf("%s: driver own failed\n", DEVNAME(sc));
		return EIO;
	}

	return 0;
}

int
mwx_wfsys_reset(struct mwx_softc *sc)
{
	uint32_t reg;

	DPRINTF("%s: WFSYS reset\n", DEVNAME(sc));

	reg = (sc->sc_hwtype == MWX_HW_MT7925) ?
	    MT7925_WFSYS_SW_RST_B : MT_WFSYS_SW_RST_B;

	mwx_clear(sc, reg, WFSYS_SW_RST_B);
	delay(50 * 1000);
	mwx_set(sc, reg, WFSYS_SW_RST_B);

	return mwx_poll(sc, reg, WFSYS_SW_INIT_DONE, WFSYS_SW_INIT_DONE, 500);
}

uint32_t
mwx_reg_addr(struct mwx_softc *sc, uint32_t reg)
{
	const struct mwx_connac_reg_map *map;
	size_t i, nelms;
	uint32_t low_limit;

	if (sc->sc_hwtype == MWX_HW_MT7925) {
		map = mt7925_fixed_map;
		nelms = nitems(mt7925_fixed_map);
		low_limit = 0x200000;
	} else {
		map = mt7921_fixed_map;
		nelms = nitems(mt7921_fixed_map);
		low_limit = 0x100000;
	}

	if (reg < low_limit)
		return reg;

	for (i = 0; i < nelms; i++) {
		uint32_t offset;

		if (reg < map[i].phys)
			continue;
		offset = reg - map[i].phys;
		if (offset > map[i].size)
			continue;
		return map[i].maps + offset;
	}

	if ((reg >= 0x18000000 && reg < 0x18c00000) ||
	    (reg >= 0x70000000 && reg < 0x78000000) ||
	    (reg >= 0x7c000000 && reg < 0x7c400000)) {
		DPRINTF("%s: %s reg %x via L1\n", DEVNAME(sc), __func__, reg);
		if (sc->sc_hwtype == MWX_HW_MT7925)
			return mt7925_map_reg_l1(sc, reg);
		else
			return mt7921_map_reg_l1(sc, reg);
	}

	panic("%s: Access to currently unsupported address %08x",
	    DEVNAME(sc), reg);
}

int
mwx_init_hardware(struct mwx_softc *sc)
{
	int rv;

	/* reset dma */
	rv = mwx_dma_reset(sc, 1);
	if (rv != 0)
		return rv;

	/*
	 * force firmware operation mode into normal state,
	 * which should be set before firmware download stage.
	 */
	mwx_write(sc, MT_SWDEF_MODE, MT_SWDEF_NORMAL_MODE);

	if ((rv = mwx_load_firmware(sc)) != 0)
		goto fail;

	if (sc->sc_hwtype == MWX_HW_MT7925) {
		if (mt7925_mcu_get_nic_capability(sc) != 0)
			goto fail;
		if (mt7925_mcu_fw_log_2_host(sc, 1) != 0)
			goto fail;
		if (mt7925_mcu_set_eeprom(sc) != 0)
			goto fail;
	} else {
		if (mt7921_mcu_get_nic_capability(sc) != 0)
			goto fail;
		if (mt7921_mcu_fw_log_2_host(sc, 1) != 0)
			goto fail;
		/* TODO override eeprom for systems with FDT */
		if (mt7921_mcu_set_eeprom(sc) != 0)
			goto fail;
	}

	mwx_mac_init(sc);

	sc->sc_vif.vif_mn.wcid = MWX_WTBL_RESERVED;

	/*
	 * mcu_uni_add_dev, mwx_mac_wtbl_update,
	 * mcu_set_rxfilter_enable, mcu_radio_on_off_ctrl
	 */
	/* MAYBE alloc beacon and mgmt frame wcid 0 here */
	sc->sc_fw_loaded = 1;

	return 0;

 fail:
	/* reset dma */
	rv = mwx_dma_reset(sc, 1);
	if (rv != 0)
		return rv;
	return EAGAIN;
}

static inline uint32_t
mwx_get_data_mode(struct mwx_softc *sc, uint32_t info)
{
	uint32_t mode = DL_MODE_NEED_RSP;

	if (info == PATCH_SEC_NOT_SUPPORT)
		return mode;
	switch (info & PATCH_SEC_ENC_TYPE_MASK) {
	case PATCH_SEC_ENC_TYPE_PLAIN:
		break;
	case PATCH_SEC_ENC_TYPE_AES:
		mode |= DL_MODE_ENCRYPT;
		mode |= (info << DL_MODE_KEY_IDX_SHIFT) & DL_MODE_KEY_IDX_MASK;
		mode |= DL_MODE_RESET_SEC_IV;
		break;
	case PATCH_SEC_ENC_TYPE_SCRAMBLE:
		mode |= DL_MODE_ENCRYPT;
		mode |= DL_CONFIG_ENCRY_MODE_SEL;
		mode |= DL_MODE_RESET_SEC_IV;
		break;
	default:
		printf("%s: encryption type not supported\n", DEVNAME(sc));
	}
	return mode;
}

static inline uint32_t
mwx_mcu_gen_dl_mode(uint8_t feature_set)
{
	uint32_t ret = DL_MODE_NEED_RSP;

	if (feature_set & FW_FEATURE_SET_ENCRYPT)
		ret |= (DL_MODE_ENCRYPT | DL_MODE_RESET_SEC_IV);
	if (feature_set & FW_FEATURE_ENCRY_MODE)
		ret |= DL_CONFIG_ENCRY_MODE_SEL;

	/* FW_FEATURE_SET_KEY_IDX_MASK == DL_MODE_KEY_IDX_MASK */
	ret |= feature_set & FW_FEATURE_SET_KEY_IDX_MASK;

	return ret;
}


int
mwx_load_firmware(struct mwx_softc *sc)
{
	const struct mwx_patch_hdr *hdr;
	const struct mwx_patch_sec *sec;
	const struct mwx_fw_trailer *fwhdr;
	const struct mwx_fw_region *region;
	const char *rompatch, *fw;
	u_char *buf, *fwbuf, *dl;
	size_t buflen, fwlen, offset = 0;
	uint32_t reg, n_region, override = 0, option = 0;
	int i, rv, sem;

	reg = mwx_read(sc, MT_CONN_ON_MISC) & MT_TOP_MISC2_FW_N9_RDY;
	if (reg != 0) {
		DPRINTF("%s: firmware already downloaded\n", DEVNAME(sc));
		return 0;
	}

	/* this read is needed to make interrupts work */
	(void) mwx_read(sc, MT_TOP_LPCR_HOST_BAND0);

	/* Take driver ownership before touching firmware registers */
	mwx_write(sc, MT_TOP_LPCR_HOST_BAND0, MT_TOP_LPCR_HOST_DRV_OWN);
	if (mwx_poll(sc, MT_TOP_LPCR_HOST_BAND0, 0, MT_TOP_LPCR_HOST_FW_OWN,
	    5000) != 0) {
	    printf("%s: timeout for driver own\n", DEVNAME(sc));
	    return EIO;
	}

	/* Disable PCIe L0s to prevent the link from entering low-power state */
	mwx_set(sc, MT_PCIE_MAC_PM, MT_PCIE_MAC_PM_L0S_DIS);

	switch (sc->sc_hwtype) {
	case MWX_HW_MT7920:
		rompatch = MT7920_ROM_PATCH;
		fw = MT7920_FIRMWARE_WM;
	case MWX_HW_MT7921:
		rompatch = MT7921_ROM_PATCH;
		fw = MT7921_FIRMWARE_WM;
		break;
	case MWX_HW_MT7922:
		rompatch = MT7922_ROM_PATCH;
		fw = MT7922_FIRMWARE_WM;
		break;
	case MWX_HW_MT7925:
		rompatch = MT7925_ROM_PATCH;
		fw = MT7925_FIRMWARE_WM;
		break;
	}
	if ((rv = loadfirmware(rompatch, &buf, &buflen)) != 0 ||
	    (rv = loadfirmware(fw, &fwbuf, &fwlen)) != 0) {
		printf("%s: loadfirmware error %d\n", DEVNAME(sc), rv);
		return rv;
	}

	rv = mwx_mcu_patch_sem_ctrl(sc, 1);
	if (rv != 0)
		goto fail;

	if (buflen < sizeof(*hdr)) {
		printf("%s: invalid firmware\n", DEVNAME(sc));
		rv = EINVAL;
		goto out;
	}
	hdr = (struct mwx_patch_hdr *)buf;
	n_region = be32toh(hdr->desc.n_region);
	if (buflen < sizeof(*hdr) + n_region * sizeof(*sec)) {
		printf("%s: invalid firmware, short header\n", DEVNAME(sc));
		rv = EINVAL;
		goto out;
	}
	printf("%s: HW/SW version: 0x%x, build time: %.15s\n",
	    DEVNAME(sc), be32toh(hdr->hw_sw_ver), hdr->build_date);

	for (i = 0; i < n_region; i++) {
		uint32_t len, addr, mode, sec_info;

		sec = (struct mwx_patch_sec *)(buf + sizeof(*hdr) +
		    i * sizeof(*sec));
		if ((be32toh(sec->type) & PATCH_SEC_TYPE_MASK) !=
		    PATCH_SEC_TYPE_INFO) {
			printf("%s: invalid firmware sector\n", DEVNAME(sc));
			rv = EINVAL;
			goto out;
		}

		addr = be32toh(sec->info.addr);
		len = be32toh(sec->info.len);
		dl = buf + be32toh(sec->offs);
		sec_info = be32toh(sec->info.sec_key_idx);
		mode = mwx_get_data_mode(sc, sec_info);

		if (dl + len > buf + buflen) {
			printf("%s: firmware sector %d exceeds payload\n",
			    DEVNAME(sc), i);
			rv = EINVAL;
			goto out;
		}

		rv = mwx_mcu_init_download(sc, addr, len, mode);
		if (rv != 0) {
			DPRINTF("%s: download request failed\n", DEVNAME(sc));
			goto out;
		}
		rv = mwx_mcu_send_firmware(sc, MCU_CMD_FW_SCATTER,
		    dl, len, 4096);
		if (rv != 0) {
			DPRINTF("%s: failed to send patch\n", DEVNAME(sc));
			goto out;
		}
	}

	/* Give MCU time to process final FWDL data */
	delay(5000);
	rv = mwx_mcu_start_patch(sc);
	if (rv != 0) {
		printf("%s: patch start failed\n", DEVNAME(sc));
		goto fail;
	}

out:
	sem = mwx_mcu_patch_sem_ctrl(sc, 0);
	if (sem != 0)
		rv = sem;
	if (rv != 0)
		goto fail;

	if (fwlen < sizeof(*fwhdr)) {
		printf("%s: invalid WM firmware\n", DEVNAME(sc));
		rv = EINVAL;
		goto out;
	}
	fwhdr = (struct mwx_fw_trailer *)(fwbuf + fwlen - sizeof(*fwhdr));
	printf("%s: WM firmware version: %.10s, build time: %.15s\n",
	    DEVNAME(sc), fwhdr->fw_ver, fwhdr->build_date);

	if (fwlen < sizeof(*fwhdr) + fwhdr->n_region * sizeof(*region)) {
		printf("%s: invalid WM firmware, short header\n", DEVNAME(sc));
		rv = EINVAL;
		goto out;
	}

	for (i = 0; i < fwhdr->n_region; i++) {
		uint32_t len, addr, mode;

		region = (struct mwx_fw_region *)((u_char *)fwhdr -
		    (fwhdr->n_region - i) * sizeof(*region));

		addr = le32toh(region->addr);
		len = le32toh(region->len);
		mode = mwx_mcu_gen_dl_mode(region->feature_set);

		if (offset + len > fwlen - sizeof(*fwhdr) -
		    fwhdr->n_region * sizeof(*region)) {
			printf("%s: WM region %d exceeds firmware payload\n",
			    DEVNAME(sc), i);
			rv = EINVAL;
			goto fail;
		}

		if (region->feature_set & FW_FEATURE_OVERRIDE_ADDR)
			override = addr;
		/* Skip non-download regions */
		if (region->feature_set & FW_FEATURE_NON_DL) {
			DPRINTF("%s: WM region %d addr %08x len %u skipped\n",
			    DEVNAME(sc), i, addr, len);
			offset += len;
			continue;
		}

		DPRINTF("%s: WM region %d addr %08x len %u mode %08x\n",
		    DEVNAME(sc), i, addr, len, mode);
		rv = mwx_mcu_init_download(sc, addr, len, mode);
		if (rv != 0) {
			DPRINTF("%s: download request failed\n", DEVNAME(sc));
			goto fail;
		}

		rv = mwx_mcu_send_firmware(sc, MCU_CMD_FW_SCATTER,
		    fwbuf + offset, len, 4096);
		if (rv != 0) {
			DPRINTF("%s: failed to send firmware\n", DEVNAME(sc));
			goto fail;
		}
		offset += len;
	}

	if (override != 0)
		option |= FW_START_OVERRIDE;

	rv = mwx_mcu_start_firmware(sc, override, option);
	if (rv != 0) {
		DPRINTF("%s: firmware start failed\n", DEVNAME(sc));
		goto fail;
	}

	/* XXX should not busy poll here */
	if (mwx_poll(sc, MT_CONN_ON_MISC, MT_TOP_MISC2_FW_N9_RDY,
	    MT_TOP_MISC2_FW_N9_RDY, 1500) != 0) {
		DPRINTF("%s: Timeout initializing firmware\n", DEVNAME(sc));
		return EIO;
	}

	DPRINTF("%s: firmware loaded\n", DEVNAME(sc));
	rv = 0;

	/* TODO load CLC data if available */
fail:
	mwx_free(buf);
	mwx_free(fwbuf);
	return rv;
}

int
mwx_mac_wtbl_update(struct mwx_softc *sc, uint32_t idx)
{
	uint32_t reg, val, mask;

	if (sc->sc_hwtype == MWX_HW_MT7925) {
		reg = MT7925_WTBL_UPDATE;
		mask = MT7925_WTBL_UPDATE_WLAN_IDX_MASK;
		val = (idx & mask) | MT7925_WTBL_UPDATE_ADM_COUNT_CLEAR;
	} else {
		reg = MT_WTBL_UPDATE;
		mask = MT_WTBL_UPDATE_WLAN_IDX_MASK;
		val = (idx & mask) | MT_WTBL_UPDATE_ADM_COUNT_CLEAR;
	}

	mwx_rmw(sc, reg, val, mask);

	return mwx_poll(sc, reg, 0, MT_WTBL_UPDATE_BUSY, 5000);
}

void
mt7925_mac_set_fixed_rate_table(struct mwx_softc *sc, uint8_t tbl_idx,
    uint16_t rate_idx)
{
	uint32_t ctrl;

	ctrl = MT_WTBL_ITCR_WR | MT_WTBL_ITCR_EXEC | tbl_idx;

	mwx_write(sc, MT_WTBL_ITDR0, rate_idx);
	/* Use the WTBL SPE index, matching Linux MT7925. */
	mwx_write(sc, MT_WTBL_ITDR1, MT_WTBL_SPE_IDX_SEL);
	mwx_write(sc, MT_WTBL_ITCR, ctrl);
}

void
mt7925_mac_init_basic_rates(struct mwx_softc *sc)
{
	size_t i;

	for (i = 0; i < nitems(mt76_rates); i++) {
		uint16_t rate, idx;
		uint16_t mode;

		rate = mt76_rates[i].hw_value;
		idx = 11 + i;

		mode = (rate >> 8) << MT_TX_RATE_MODE_SHIFT;
		rate = (rate & MT_TX_RATE_IDX_MASK) |
		    (mode & MT_TX_RATE_MODE_MASK);
		mt7925_mac_set_fixed_rate_table(sc, idx, rate);
	}
}

void
mwx_mac_init_band(struct mwx_softc *sc, uint32_t band)
{
	/* configure tx count control timer */
	mwx_rmw(sc, MT_TMAC_CTCR0(band), 0x3f, MT_TMAC_CTCR0_INS_DDLMT_REFTIME);
	mwx_set(sc, MT_TMAC_CTCR0(band),
	    MT_TMAC_CTCR0_INS_DDLMT_VHT_SMPDU_EN |
	    MT_TMAC_CTCR0_INS_DDLMT_EN);

	/* enable MIB rx time reporting */
	mwx_set(sc, MT_WF_RMAC_MIB_TIME0(band), MT_WF_RMAC_MIB_RXTIME_EN);
	mwx_set(sc, MT_WF_RMAC_MIB_AIRTIME0(band), MT_WF_RMAC_MIB_RXTIME_EN);

	/* enable MIB tx-rx time reporting */
	mwx_set(sc, MT_MIB_SCR1(band), MT_MIB_TXDUR_EN);
	mwx_set(sc, MT_MIB_SCR1(band), MT_MIB_RXDUR_EN);

	/* set max RX length */
	mwx_rmw(sc, MT_DMA_DCR0(band),
	    1536 << MT_DMA_DCR0_MAX_RX_LEN_SHIFT, MT_DMA_DCR0_MAX_RX_LEN_MASK);
	/* disable rx rate report by default due to hw issues */
	mwx_clear(sc, MT_DMA_DCR0(band), MT_DMA_DCR0_RXD_G5_EN);

	/* filter out non-resp frames and get instantaneous signal reporting */
	mwx_rmw(sc, MT_WTBLOFF_TOP_RSCR(band),
	    0 << MT_WTBLOFF_TOP_RSCR_RCPI_MODE_SHIFT |
	    0x3 << MT_WTBLOFF_TOP_RSCR_RCPI_PARAM_SHIFT,
	    MT_WTBLOFF_TOP_RSCR_RCPI_MODE_MASK |
	    MT_WTBLOFF_TOP_RSCR_RCPI_PARAM_MASK);
}

void
mwx_mac_init(struct mwx_softc *sc)
{
	uint32_t dcr0, dcr1;
	uint32_t i;

	if (sc->sc_hwtype == MWX_HW_MT7925) {
		dcr0 = MT7925_MDP_DCR0;
		dcr1 = MT7925_MDP_DCR1;
	} else {
		dcr0 = MT_MDP_DCR0;
		dcr1 = MT_MDP_DCR1;
	}

	mwx_rmw(sc, dcr1, 1536 << MT_MDP_DCR1_MAX_RX_LEN_SHIFT,
	    MT_MDP_DCR1_MAX_RX_LEN_MASK);

	/* enable hardware de-aggregation */
	mwx_set(sc, dcr0, MT_MDP_DCR0_DAMSDU_EN);

	if (sc->sc_hwtype != MWX_HW_MT7925) {
		/* enable hardware rx header translation */
		mwx_set(sc, dcr0, MT_MDP_DCR0_RX_HDR_TRANS_EN);
	}

	for (i = 0; i < MWX_WTBL_SIZE; i++)
		mwx_mac_wtbl_update(sc, i);

	mwx_mac_init_band(sc, 0);
	mwx_mac_init_band(sc, 1);

	if (sc->sc_hwtype == MWX_HW_MT7925)
		mt7925_mac_init_basic_rates(sc);
}

int
mwx_mcu_patch_sem_ctrl(struct mwx_softc *sc, int semget)
{
#define	PATCH_SEM_RELEASE		0
#define PATCH_SEM_GET			1
#define	PATCH_NOT_DL_SEM_FAIL		0
#define	PATCH_IS_DL			1
#define	PATCH_NOT_DL_SEM_SUCCESS	2
#define	PATCH_REL_SEM_SUCCESS		3

	uint32_t op = semget ? PATCH_SEM_GET : PATCH_SEM_RELEASE;
	struct {
		uint32_t op;
	} req = {
		.op = htole32(op),
	};
	int rv, seq, sem;

	rv = mwx_mcu_send_msg(sc, MCU_CMD_PATCH_SEM_CONTROL,
	    &req, sizeof(req), &seq);
	if (rv != 0)
		return rv;

	rv = mwx_mcu_wait_resp_int(sc, MCU_CMD_PATCH_SEM_CONTROL, seq,
	    (uint32_t *)&sem);
	if (rv != 0)
		return rv;

	if (semget) {
		switch (sem) {
		case PATCH_IS_DL:
			return -1;
		case PATCH_NOT_DL_SEM_SUCCESS:
			return 0;
		default:
			DPRINTF("%s: failed to %s patch semaphore\n",
			    DEVNAME(sc), "get");
			return EAGAIN;
		}
	} else {
		switch (sem) {
		case PATCH_REL_SEM_SUCCESS:
			return 0;
		default:
			DPRINTF("%s: failed to %s patch semaphore\n",
			    DEVNAME(sc), "release");
			return EAGAIN;
		}
	}
}

int
mwx_mcu_init_download(struct mwx_softc *sc, uint32_t addr,
    uint32_t len, uint32_t mode)
{
	struct {
		uint32_t addr;
		uint32_t len;
		uint32_t mode;
	} req = {
		.addr = htole32(addr),
		.len = htole32(len),
		.mode = htole32(mode),
	};
	int cmd;

	if (addr == 0x200000 || addr == 0x900000)
		cmd = MCU_CMD_PATCH_START_REQ;
	else
		cmd = MCU_CMD_TARGET_ADDRESS_LEN_REQ;

	return mwx_mcu_send_wait(sc, cmd, &req, sizeof(req));
}

int
mwx_mcu_send_firmware(struct mwx_softc *sc, int cmd, u_char *data,
    size_t len, size_t max_len)
{
	struct mbuf *m;
	size_t cur_len;
	int rv, nchunks = 0;

	while (len > 0) {
		cur_len = len;
		if (cur_len > max_len)
			cur_len = max_len;

		/*
		 * Firmware scatter payloads are raw data: no MCU txd is
		 * prepended to them.  Allocate the buffer without the txd
		 * headroom so that a 4096 byte chunk still fits in a 4096
		 * byte cluster rather than requiring a jumbo cluster.
		 */
		m = MCLGETL(NULL, M_DONTWAIT, cur_len);
		if (m == NULL)
			return ENOMEM;
		mbuf_setlen(m, cur_len);
		mbuf_pkthdr_setlen(m, cur_len);
		memcpy(mtod(m, caddr_t), data, cur_len);

		rv = mwx_mcu_send_mbuf(sc, cmd, m, NULL);
		if (rv != 0) {
			DPRINTF("%s: fw chunk enqueue failed, len %zu rv %d\n",
			    DEVNAME(sc), cur_len, rv);
			return rv;
		}

		data += cur_len;
		len -= cur_len;
		nchunks++;

		/*
		 * Do not drain the TX ring here.  mwx_dma_tx_cleanup() is
		 * called by the interrupt action, which runs on the
		 * workloop, while this loop runs on the attach thread.
		 * Upstream gets away with the call below because the
		 * interrupt is serviced at splnet on the same CPU; here
		 * splnet() is a no-op, so the two consumers raced and
		 * corrupted mq_cons (seen as "prod 11 cons 12" with a
		 * stalled FWDL ring and a failed firmware download).
		 * The enqueue wait loop below already sleeps until the
		 * interrupt action frees a slot.
		 */
	}

	DPRINTF("%s: fw scatter cmd %x: %d chunk(s) queued\n", DEVNAME(sc),
	    cmd, nchunks);

	return 0;
}

int
mwx_mcu_start_patch(struct mwx_softc *sc)
{
	struct {
		uint8_t check_crc;
		uint8_t reserved[3];
	} req = {
		.check_crc = 0,
	};

	return mwx_mcu_send_wait(sc, MCU_CMD_PATCH_FINISH_REQ, &req,
	    sizeof(req));
}

int
mwx_mcu_start_firmware(struct mwx_softc *sc, uint32_t addr, uint32_t option)
{
	struct {
		uint32_t option;
		uint32_t addr;
	} req = {
		.option = htole32(option),
		.addr = htole32(addr),
	};

	return mwx_mcu_send_wait(sc, MCU_CMD_FW_START_REQ, &req, sizeof(req));
}

int
mt7921_mcu_get_nic_capability(struct mwx_softc *sc)
{
	struct mt76_connac_cap_hdr {
		uint16_t	n_elements;
		uint16_t	pad;
	} __packed *hdr;
	struct tlv_hdr {
		uint32_t	type;
		uint32_t	len;
	} __packed *tlv;
	struct mwx_connac_phy_cap *cap;
	struct mbuf *m;
	int rv, seq, count, i;

	rv = mwx_mcu_send_msg(sc, MCU_CE_CMD_GET_NIC_CAPAB, NULL, 0, &seq);
	if (rv != 0)
		return rv;

	rv = mwx_mcu_wait_resp_msg(sc, MCU_CE_CMD_GET_NIC_CAPAB, seq, &m);
	if (rv != 0)
		return rv;

	/* the message was already pulled up by mwx_mcu_rx_event() */
	if (mbuf_len(m) < sizeof(*hdr)) {
		printf("%s: GET_NIC_CAPAB response size error\n", DEVNAME(sc));
		m_freem(m);
		return EINVAL;
	}
	hdr = mtod(m, struct mt76_connac_cap_hdr *);
	count = le16toh(hdr->n_elements);
	m_adj(m, sizeof(*hdr));

	for (i = 0; i < count; i++) {
		uint32_t type, len;

		if (mbuf_len(m) < sizeof(*tlv)) {
			printf("%s: GET_NIC_CAPAB tlv size error\n",
			    DEVNAME(sc));
			m_freem(m);
			return EINVAL;
		}

		tlv = mtod(m, struct tlv_hdr *);
		type = le32toh(tlv->type);
		len = le32toh(tlv->len);
		m_adj(m, sizeof(*tlv));

		if (mbuf_len(m) < len) {
			printf("%s: GET_NIC_CAPAB tlv length error\n",
			    DEVNAME(sc));
			m_freem(m);
			return EINVAL;
		}
		switch (type) {
		case MT_NIC_CAP_6G:
			/* TODO 6GHZ SUPPORT */
			sc->sc_capa.has_6ghz = /* XXX 1 */ 0;
			break;
		case MT_NIC_CAP_MAC_ADDR:
			if (len < ETHER_ADDR_LEN)
				break;
			memcpy(sc->sc_lladdr, mtod(m, caddr_t), ETHER_ADDR_LEN);
			break;
		case MT_NIC_CAP_PHY:
			if (len < sizeof(*cap))
				break;
			cap = mtod(m, struct mwx_connac_phy_cap *);

			sc->sc_capa.num_streams = cap->nss;
			sc->sc_capa.antenna_mask = (1U << cap->nss) - 1;
			sc->sc_capa.has_2ghz = cap->hw_path & 0x01;
			sc->sc_capa.has_5ghz = cap->hw_path & 0x02;
			break;
		case MT_NIC_CAP_TX_RESOURCE:
			/* unused on PCIe devices */
			break;
		}
		m_adj(m, len);
	}

	printf("%s: address %s\n", DEVNAME(sc), ether_sprintf(sc->sc_lladdr));

	m_freem(m);
	return 0;
}

int
mt7925_mcu_get_nic_capability(struct mwx_softc *sc)
{
	struct mt76_connac_cap_hdr {
		uint16_t	n_elements;
		uint8_t		pad[2];
	} __packed *hdr;
	struct tlv_hdr {
		uint16_t	tag;
		uint16_t	len;
	} __packed *tlv;
	struct mwx_connac_phy_cap *cap;
	struct mbuf *m;
	struct {
		uint8_t		rsv[4];
		uint16_t	tag;
		uint16_t	len;
	} __packed req = {
		.tag = htole16(UNI_CHIP_CONFIG_NIC_CAPA),
		.len = htole16(sizeof(req) - 4),
	};
	int rv, seq, count, i;

	rv = mwx_mcu_send_msg(sc, MCU_UNI_CMD_CHIP_CONFIG, &req, sizeof(req),
	    &seq);
	if (rv != 0)
		return rv;

	rv = mwx_mcu_wait_resp_msg(sc, MCU_UNI_CMD_CHIP_CONFIG, seq, &m);
	if (rv != 0)
		return rv;

	if (mbuf_len(m) < sizeof(*hdr)) {
		printf("%s: CHIP_CONFIG NIC_CAPA response size error\n",
		    DEVNAME(sc));
		m_freem(m);
		return EINVAL;
	}
	hdr = mtod(m, struct mt76_connac_cap_hdr *);
	count = le16toh(hdr->n_elements);
	m_adj(m, sizeof(*hdr));

	for (i = 0; i < count; i++) {
		uint16_t tag, len;

		if (mbuf_len(m) < sizeof(*tlv)) {
			printf("%s: GET_NIC_CAPAB tlv size error\n",
			    DEVNAME(sc));
			m_freem(m);
			return EINVAL;
		}

		tlv = mtod(m, struct tlv_hdr *);
		tag = le16toh(tlv->tag);
		len = le16toh(tlv->len);
		/* unlike 7921 the len includes the header */
		if (len < sizeof(*tlv) || mbuf_len(m) < len) {
			printf("%s: GET_NIC_CAPAB tlv length error\n",
			    DEVNAME(sc));
			m_freem(m);
			return EINVAL;
		}
		len -= sizeof(*tlv);
		m_adj(m, sizeof(*tlv));

		switch (tag) {
		case MT_NIC_CAP_6G:
			/* TODO 6GHZ SUPPORT */
			sc->sc_capa.has_6ghz = 0; /* *mtod(m, caddr_t) != 0; */
			break;
		case MT_NIC_CAP_MAC_ADDR:
			if (len < ETHER_ADDR_LEN)
				break;
			memcpy(sc->sc_lladdr, mtod(m, caddr_t), ETHER_ADDR_LEN);
			break;
		case MT_NIC_CAP_PHY:
			if (len < sizeof(*cap))
				break;
			cap = mtod(m, struct mwx_connac_phy_cap *);

			sc->sc_capa.num_streams = cap->nss;
			sc->sc_capa.antenna_mask = (1U << cap->nss) - 1;
			sc->sc_capa.has_2ghz = cap->hw_path & 0x01;
			sc->sc_capa.has_5ghz = cap->hw_path & 0x02;
			break;
		}

		m_adj(m, len);
	}

	printf("%s: address %s\n", DEVNAME(sc), ether_sprintf(sc->sc_lladdr));

	m_freem(m);
	return 0;
}

int
mt7921_mcu_fw_log_2_host(struct mwx_softc *sc, uint8_t ctrl)
{
	struct {
		uint8_t	ctrl;
		uint8_t	pad[3];
	} req = {
		.ctrl = ctrl,
	};

	return mwx_mcu_send_msg(sc, MCU_CE_CMD_FWLOG_2_HOST, &req,
	    sizeof(req), NULL);
}

int
mt7925_mcu_fw_log_2_host(struct mwx_softc *sc, uint8_t ctrl)
{
	struct {
		uint8_t		rsv[4];
		uint16_t	tag;
		uint16_t	len;
		uint8_t		ctrl;
		uint8_t		interval;
		uint8_t		rsv2[2];
	} req = {
		.tag = htole16(UNI_WSYS_CONFIG_FW_LOG_CTRL),
		.len = htole16(sizeof(req) - 4),
		.ctrl = ctrl,
	};

	return mwx_mcu_send_msg(sc, MCU_UNI_CMD_WSYS_CONFIG, &req,
	    sizeof(req), NULL);
}

int
mt7921_mcu_set_eeprom(struct mwx_softc *sc)
{
	struct {
		uint8_t	buffer_mode;
		uint8_t	format;
		uint8_t pad[2];
	} req = {
		.buffer_mode = EE_MODE_EFUSE,
		.format = EE_FORMAT_WHOLE,
	};

	return mwx_mcu_send_wait(sc, MCU_EXT_CMD_EFUSE_BUFFER_MODE, &req,
	    sizeof(req));
}

int
mt7925_mcu_set_eeprom(struct mwx_softc *sc)
{
	struct {
		uint8_t		rsv[4];
		uint16_t	tag;
		uint16_t	len;
		uint8_t		buffer_mode;
		uint8_t		format;
		uint16_t	buf_len;
	} req = {
		.tag = htole16(UNI_EFUSE_BUFFER_MODE),
		.len = htole16(sizeof(req) - 4),
		.buffer_mode = EE_MODE_EFUSE,
		.format = EE_FORMAT_WHOLE,
	};

	return mwx_mcu_send_wait(sc, MCU_UNI_CMD_EFUSE_CTRL, &req,
	    sizeof(req));
}

int
mt7921_mcu_set_rts_thresh(struct mwx_softc *sc, uint32_t val, uint8_t band)
{
	struct {
		uint8_t		prot_idx;
		uint8_t		band;
		uint8_t		rsv[2];
		uint32_t	len_thresh;
		uint32_t	pkt_thresh;
	} __packed req = {
		.prot_idx = 1,
		.band = band,
		.len_thresh = htole32(val),
		.pkt_thresh = htole32(0x2),
	};

	return mwx_mcu_send_wait(sc, MCU_EXT_CMD_PROTECT_CTRL, &req,
	    sizeof(req));
}

int
mt7925_mcu_set_rts_thresh(struct mwx_softc *sc, uint32_t val)
{
	struct {
		uint8_t		band_idx;
		uint8_t		rsv[3];
		uint16_t	tag;
		uint16_t	len;
		uint16_t	len_thresh;
		uint16_t	pkt_thresh;
	} __packed req = {
		.band_idx = sc->sc_vif.band_idx,
		.tag = htole16(UNI_BAND_CONFIG_RTS_THRESHOLD),
		.len = htole16(sizeof(req) - 4),
		.len_thresh = htole32(val),
		.pkt_thresh = htole32(0x2),
	};

	return mwx_mcu_send_wait(sc, MCU_UNI_CMD_BAND_CONFIG, &req,
	    sizeof(req));
}

int
mwx_mcu_set_deep_sleep(struct mwx_softc *sc, int ena)
{
	struct {
		uint8_t				rsv[4];
		uint16_t			tag;
		uint16_t			len;
		struct mwx_connac_config	config;
	} req = {
		.tag = htole16(UNI_CHIP_CONFIG_CHIP_CFG),
		.len = htole16(sizeof(req) - 4),
	};
	int len;

	DPRINTF("%s: %s deep sleep\n", DEVNAME(sc), ena ? "enable" : "disable");
	len = snprintf((char *)req.config.data, sizeof(req.config.data),
	    "KeepFullPwr %d", !ena);

	if (sc->sc_hwtype != MWX_HW_MT7925) {
		return mwx_mcu_send_msg(sc, MCU_CE_CMD_CHIP_CONFIG,
		    &req.config, sizeof(req.config), NULL);
	} else {
		req.config.data_size = htole16(len + 1);
		return mwx_mcu_send_msg(sc, MCU_UNI_CMD_CHIP_CONFIG,
		     &req, sizeof(req), NULL);
	}
}

void
mt7921_mcu_low_power_event(struct mwx_softc *sc, struct mbuf *m)
{
	struct mt7921_mcu_lp_event {
		uint8_t state;
		uint8_t reserved[3];
	} __packed *event;

	if (mbuf_len(m) < sizeof(*event))
		return;
	event = mtod(m, struct mt7921_mcu_lp_event *);
	DPRINTF("%s: low power event state %d\n", DEVNAME(sc), event->state);
}

void
mt7921_mcu_tx_done_event(struct mwx_softc *sc, struct mbuf *m)
{
	struct mt7921_mcu_tx_done_event {
		uint8_t		pid;
		uint8_t		status;
		uint16_t	seq;
		uint8_t		wlan_idx;
		uint8_t		tx_cnt;
		uint16_t	tx_rate;
		uint8_t		flag;
		uint8_t		tid;
		uint8_t		rsp_rate;
		uint8_t		mcs;
		uint8_t		bw;
		uint8_t		tx_pwr;
		uint8_t		reason;
		uint8_t		rsv0[1];
		uint32_t	delay;
		uint32_t	timestamp;
		uint32_t	applied_flag;
		uint8_t		txs[28];
		uint8_t		rsv1[32];
	} __packed *event;

	if (mbuf_len(m) < sizeof(*event))
		return;
	event = mtod(m, struct mt7921_mcu_tx_done_event *);
	// TODO mt7921_mac_add_txs(dev, event->txs);
}

int
mt7921_mcu_hw_scan(struct mwx_softc *sc, int bgscan)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_channel *c;
	struct mt76_connac_hw_scan_req	*req;
	struct mbuf *m;
	int n_ssids = 0;
	int rv;
	uint8_t nchan;

	m = mwx_mcu_alloc_msg(sizeof(*req));
	if (m == NULL)
		return ENOMEM;
	req = mtod(m, struct mt76_connac_hw_scan_req *);

	sc->sc_flags |= MWX_FLAG_SCANNING;
	sc->sc_scan_start = mwx_uptime_ms();
	sc->sc_scan_seq_num = (sc->sc_scan_seq_num + 1) & 0x7f;

	req->seq_num = sc->sc_scan_seq_num /* | sc->sc_band_idx << 7 */;
	req->bss_idx = /* mvif->idx */ 0;
	req->scan_type = /* sreq->n_ssids ? 1 : */ 0;
	req->probe_req_num = /* sreq->n_ssids ? 2 : */ 0;
	req->version = 1;

#ifdef NOTYET
	for (i = 0; i < sreq->n_ssids; i++) {
		if (!sreq->ssids[i].ssid_len)
			continue;
		req->ssids[i].ssid_len = htole32(sreq->ssids[i].ssid_len);
		memcpy(req->ssids[i].ssid, sreq->ssids[i].ssid,
		    sreq->ssids[i].ssid_len);
		n_ssids++;
	}
#endif

	req->ssid_type = n_ssids ? 0x4 : 0x1;
	req->ssid_type_ext = n_ssids ? 1 : 0;
	req->ssids_num = n_ssids;

	for (nchan = 0, c = &ic->ic_channels[1];
	    c <= &ic->ic_channels[IEEE80211_CHAN_MAX] &&
	    nchan < 64; c++) {
		struct mt76_connac_mcu_scan_channel *chan;
		uint8_t channel_num;
		int ieee;

		if (c->ic_flags == 0)
			continue;

		ieee = ieee80211_chan2ieee(ic, c);
		if (!isset(ic->ic_chan_active, ieee))
			continue;

		if (nchan < 32)
			chan = &req->channels[nchan];
		else
			chan = &req->ext_channels[nchan - 32];

		channel_num = ieee;
		/* TODO IEEE80211_IS_CHAN_6GHZ -> chan->band = 3*/
		if (IEEE80211_IS_CHAN_2GHZ(c)) {
			chan->band = 1;
		} else {
			chan->band = 2;
		}
		chan->channel_num = channel_num;
		nchan++;
	}

	if (nchan <= 32) {
		req->channels_num = nchan;
	} else {
		req->channels_num = 32;
		req->ext_channels_num = nchan - 32;
	}

	req->channel_type = nchan ? 4 : 0;
	req->timeout_value = htole16(nchan * 120);
	req->channel_min_dwell_time = htole16(120);
	req->channel_dwell_time = htole16(120);


#ifdef NOTYET
	if (sreq->ie_len > 0) {
		memcpy(req->ies, sreq->ie, sreq->ie_len);
		req->ies_len = htole16(sreq->ie_len);
	}
#endif

	req->scan_func |= SCAN_FUNC_SPLIT_SCAN;

	/* wildcard BSSID */
	memset(req->bssid, 0xff, ETHER_ADDR_LEN);
#ifdef NOTYET
	if (sreq->flags & NL80211_SCAN_FLAG_RANDOM_ADDR) {
		get_random_mask_addr(req->random_mac, sreq->mac_addr,
		    sreq->mac_addr_mask);
		req->scan_func |= SCAN_FUNC_RANDOM_MAC;
	}
#endif

	rv = mwx_mcu_send_mbuf(sc, MCU_CE_CMD_START_HW_SCAN, m, NULL);
	if (rv != 0)
		sc->sc_flags &= ~(MWX_FLAG_SCANNING | MWX_FLAG_BGSCAN);

	return rv;
}

/*
 * Channel-context parameters shared by the CONNAC2 and CONNAC3 MCU paths.
 *
 * The firmware needs the control channel, the centre channel of the bonded
 * channel, the bandwidth and the secondary-channel offset.  net80211 gives us
 * the negotiated width and centre channel via the node's HT/VHT operation
 * information; deriving these values here keeps 20/40/80/160 MHz handling
 * identical for every supported chipset.
 */
struct mwx_chanctx {
	uint8_t	control_ch;
	uint8_t	center_ch;
	uint8_t	bw;		/* CMD_CBW_* */
	uint8_t	channel_band;	/* 0 = 2 GHz, 1 = 5 GHz, 2 = 6 GHz */
	uint8_t	rlm_band;	/* 1 = 2 GHz, 2 = 5 GHz, 3 = 6 GHz */
	uint8_t	sco;		/* 0 = none, 1 = SCA, 3 = SCB */
	uint8_t	ht_op_info;
};

static void
mwx_get_chanctx(struct mwx_softc *sc, struct ieee80211_node *ni,
    struct ieee80211_channel *chan, struct mwx_chanctx *ctx)
{
	uint8_t width = IEEE80211_VHTOP0_CHAN_WIDTH_HT;
	uint8_t sco = IEEE80211_HTOP0_SCO_SCN;

	memset(ctx, 0, sizeof(*ctx));
	ctx->control_ch = ieee80211_mhz2ieee(chan->ic_freq, chan->ic_flags);
	ctx->center_ch = ctx->control_ch;
	ctx->bw = CMD_CBW_20MHZ;

	if (IEEE80211_IS_CHAN_2GHZ(chan)) {
		ctx->channel_band = 0;
		ctx->rlm_band = 1;
	} else {
		/* 6 GHz will need a third value once net80211 supports it. */
		ctx->channel_band = 1;
		ctx->rlm_band = 2;
	}

	if (ni == NULL || ni->ni_chan != chan)
		return;

	width = ieee80211_node_vht_channel_width(ni);
	sco = ieee80211_node_ht_secondary_channel_offset(ni);

	switch (width) {
	case IEEE80211_VHTOP0_CHAN_WIDTH_8080:
		ctx->bw = CMD_CBW_8080MHZ;
		/* centre of the primary 80 MHz segment */
		if (ni->ni_vht_chan_center_freq_idx0 != 0)
			ctx->center_ch = ni->ni_vht_chan_center_freq_idx0;
		break;
	case IEEE80211_VHTOP0_CHAN_WIDTH_160:
		ctx->bw = CMD_CBW_160MHZ;
		if (ni->ni_vht_chan_center_freq_idx0 != 0)
			ctx->center_ch = ni->ni_vht_chan_center_freq_idx0;
		break;
	case IEEE80211_VHTOP0_CHAN_WIDTH_80:
		ctx->bw = CMD_CBW_80MHZ;
		if (ni->ni_vht_chan_center_freq_idx0 != 0)
			ctx->center_ch = ni->ni_vht_chan_center_freq_idx0;
		break;
	default:
		if (sco == IEEE80211_HTOP0_SCO_SCA) {
			ctx->bw = CMD_CBW_40MHZ;
			ctx->center_ch = ctx->control_ch + 2;
		} else if (sco == IEEE80211_HTOP0_SCO_SCB) {
			ctx->bw = CMD_CBW_40MHZ;
			ctx->center_ch = ctx->control_ch - 2;
		}
		break;
	}

	/*
	 * Fall back to the 80/160 MHz block centre if the AP did not provide
	 * a usable VHT centre-channel index.  This only applies to 5 GHz;
	 * 6 GHz uses a different channel numbering and is intentionally
	 * excluded until net80211 supports it.  Note that the UNII-3 block
	 * starting at channel 149 is not aligned to the 36+16n pattern.
	 */
	if (ctx->center_ch == ctx->control_ch &&
	    (ctx->bw == CMD_CBW_80MHZ || ctx->bw == CMD_CBW_160MHZ) &&
	    IEEE80211_IS_CHAN_5GHZ(chan)) {
		uint8_t base;

		if (ctx->control_ch >= 149)
			base = 149;
		else if (ctx->control_ch >= 100)
			base = 100;
		else
			base = 36;

		if (ctx->bw == CMD_CBW_160MHZ)
			ctx->center_ch = base +
			    ((ctx->control_ch - base) / 32) * 32 + 14;
		else
			ctx->center_ch = base +
			    ((ctx->control_ch - base) / 16) * 16 + 6;
	}

	if (ctx->control_ch < ctx->center_ch)
		ctx->sco = 1;	/* SCA */
	else if (ctx->control_ch > ctx->center_ch)
		ctx->sco = 3;	/* SCB */

	/* Tell the firmware that a 40 MHz HT operation is permitted. */
	ctx->ht_op_info = (ctx->bw == CMD_CBW_20MHZ) ? 0 : 4;
}

static int
mt7925_mcu_set_chctx(struct mwx_softc *sc, struct ieee80211_node *ni,
    struct ieee80211_channel *channel)
{
	struct {
		struct {
			uint8_t		bss_idx;
			uint8_t		pad[3];
		} __packed hdr;
		struct {
			uint16_t	tag;
			uint16_t	len;
			uint8_t		control_channel;
			uint8_t		center_chan;
			uint8_t		center_chan2;
			uint8_t		bw;
			uint8_t		tx_streams;
			uint8_t		rx_streams;
			uint8_t		short_st;
			uint8_t		ht_op_info;
			uint8_t		sco;
			uint8_t		band;
			uint8_t		pad[2];
		} __packed rlm;
	} __packed req = {
		.hdr = {
			.bss_idx = sc->sc_vif.idx,
		},
		.rlm = {
			.tag = htole16(UNI_BSS_INFO_RLM),
			.len = htole16(sizeof(req.rlm)),
			.tx_streams = sc->sc_capa.num_streams,
			.rx_streams = sc->sc_capa.num_streams,
			.short_st = 1,
			.ht_op_info = 4,
			.band = 1,
			.bw = CMD_CBW_20MHZ,
		},
	};
	struct mwx_chanctx ctx;

	if (channel == NULL)
		return EINVAL;

	mwx_get_chanctx(sc, ni, channel, &ctx);
	req.rlm.control_channel = ctx.control_ch;
	req.rlm.center_chan = ctx.center_ch;
	req.rlm.bw = ctx.bw;
	req.rlm.ht_op_info = ctx.ht_op_info;
	req.rlm.sco = ctx.sco;
	req.rlm.band = ctx.rlm_band;

	DPRINTF("%s: mt7925 set chctx bss %u ctrl %u center %u band %u bw %u "
	    "sco %u\n", DEVNAME(sc), req.hdr.bss_idx, req.rlm.control_channel,
	    req.rlm.center_chan, req.rlm.band, req.rlm.bw, req.rlm.sco);
	return mwx_mcu_send_wait(sc, MCU_UNI_CMD_BSS_INFO_UPDATE, &req,
	    sizeof(req));
}

int
mt7925_mcu_hw_scan(struct mwx_softc *sc, int bgscan)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_channel *c;
	struct mt76_connac_mcu_scan_channel *chan;
	struct mbuf *m;
	struct mt7925_scan_hdr_tlv *hdr;
	struct mt7925_scan_req_tlv *req;
	struct mt7925_scan_ssid_tlv *ssid;
	struct mt7925_scan_bssid_tlv *bssid;
	struct mt7925_scan_chan_info_tlv *chan_info;
	struct mt7925_scan_ie_tlv *ie;
	struct mt7925_scan_misc_tlv *misc;
	struct mbuf *probe;
	struct ieee80211_channel *scan0;
	uint8_t *ptr;
	int active, i, ie_len, nchan, maxlen, rv;

	maxlen = sizeof(*hdr) + sizeof(*req) + sizeof(*ssid) +
	    sizeof(*bssid) + sizeof(*chan_info) + sizeof(*misc) +
	    sizeof(*ie) + MT76_HW_SCAN_IE_LEN;
	m = mwx_mcu_alloc_msg(maxlen);
	if (m == NULL)
		return ENOMEM;

	sc->sc_scan_seq_num = (sc->sc_scan_seq_num + 1) & 0x7f;
	scan0 = NULL;
	active = !!(ic->ic_flags & IEEE80211_F_ASCAN);
	//active = 0;

	hdr = mtod(m, struct mt7925_scan_hdr_tlv *);
	hdr->seq_num = sc->sc_scan_seq_num;
	hdr->bss_idx = sc->sc_vif.idx;
	hdr->tlv_num = 5;

	req = (struct mt7925_scan_req_tlv *)(hdr + 1);
	req->tag = htole16(UNI_SCAN_REQ);
	req->len = htole16(sizeof(*req));
	req->scan_type = active ? 1 : 0;
	req->probe_req_num = active ? 2 : 0;
	req->scan_func = SCAN_FUNC_SPLIT_SCAN;

	ssid = (struct mt7925_scan_ssid_tlv *)(req + 1);
	ssid->tag = htole16(UNI_SCAN_SSID);
	ssid->len = htole16(sizeof(*ssid));
	if (active && ic->ic_des_esslen > 0) {
		ssid->ssid_type = 0x4;
		ssid->ssids_num = 1;
		ssid->ssids[0].ssid_len = htole32(ic->ic_des_esslen);
		memcpy(ssid->ssids[0].ssid, ic->ic_des_essid,
		    ic->ic_des_esslen);
	} else
		ssid->ssid_type = 0x1;

	bssid = (struct mt7925_scan_bssid_tlv *)(ssid + 1);
	bssid->tag = htole16(UNI_SCAN_BSSID);
	bssid->len = htole16(sizeof(*bssid));
	memset(bssid->bssid, 0xff, sizeof(bssid->bssid));

	chan_info = (struct mt7925_scan_chan_info_tlv *)(bssid + 1);
	chan_info->tag = htole16(UNI_SCAN_CHANNEL);
	chan_info->len = htole16(sizeof(*chan_info));
	chan_info->channel_type = 4;

	for (i = 0, nchan = 0, c = &ic->ic_channels[1];
	    c <= &ic->ic_channels[IEEE80211_CHAN_MAX] &&
	    nchan < nitems(chan_info->channels); c++) {
		int ieee;
		uint8_t channel_num;

		if (c->ic_flags == 0)
			continue;
		ieee = ieee80211_chan2ieee(ic, c);
		if (!isset(ic->ic_chan_active, ieee))
			continue;

		chan = &chan_info->channels[nchan];
		channel_num = ieee80211_mhz2ieee(c->ic_freq, 0);
		chan->band = IEEE80211_IS_CHAN_2GHZ(c) ? 1 : 2;
		chan->channel_num = channel_num;
		if (scan0 == NULL)
			scan0 = c;
		nchan++;
	}

	chan_info->channels_num = nchan;

	misc = (struct mt7925_scan_misc_tlv *)(chan_info + 1);
	misc->tag = htole16(UNI_SCAN_MISC);
	misc->len = htole16(sizeof(*misc));
	ptr = (uint8_t *)(misc + 1);

	if (active) {
		probe = ieee80211_get_probe_req(ic, ic->ic_bss);
		if (probe == NULL) {
			m_freem(m);
			if (bgscan == 0)
				sc->sc_flags &= ~(MWX_FLAG_SCANNING |
				    MWX_FLAG_BGSCAN);
			return ENOMEM;
		}

		ie_len = MIN(mbuf_pkthdr_len(probe), MT76_HW_SCAN_IE_LEN);
		if (ie_len > 0) {
			ie = (struct mt7925_scan_ie_tlv *)ptr;
			ie->tag = htole16(UNI_SCAN_IE);
			ie->len = htole16(sizeof(*ie) + ie_len);
			ie->ies_len = htole16(ie_len);
			ie->band = 1;
			m_copydata(probe, 0, ie_len, ie->ies);
			ptr += sizeof(*ie) + ie_len;
			hdr->tlv_num++;

			/* XXX append 5Ghz probe all the time */
			ie = (struct mt7925_scan_ie_tlv *)ptr;
			ie->tag = htole16(UNI_SCAN_IE);
			ie->len = htole16(sizeof(*ie) + ie_len);
			ie->ies_len = htole16(ie_len);
			ie->band = 2;
			m_copydata(probe, 0, ie_len, ie->ies);
			ptr += sizeof(*ie) + ie_len;
			hdr->tlv_num++;
		}
		m_freem(probe);
	}

	rv = mt7925_mcu_set_chctx(sc, NULL, scan0);
	if (rv != 0) {
		m_freem(m);
		if (bgscan == 0)
			sc->sc_flags &= ~(MWX_FLAG_SCANNING | MWX_FLAG_BGSCAN);
		return rv;
	}

	mwx_mcu_set_len(m, ptr);
	rv = mwx_mcu_send_mbuf_wait(sc, MCU_UNI_CMD_SCAN_REQ, m);
	if (rv != 0 && bgscan == 0)
		sc->sc_flags &= ~(MWX_FLAG_SCANNING | MWX_FLAG_BGSCAN);
	return rv;
}

int
mt7921_mcu_hw_scan_cancel(struct mwx_softc *sc)
{
	struct {
		uint8_t		seq_num;
		uint8_t		is_ext_channel;
		uint8_t		rsv[2];
	} __packed req = {
		.seq_num = sc->sc_scan_seq_num,
	};
	int rv;

	rv = mwx_mcu_send_msg(sc, MCU_CE_CMD_CANCEL_HW_SCAN, &req,
	    sizeof(req), NULL);
	if (rv == 0)
		sc->sc_flags &= ~(MWX_FLAG_SCANNING | MWX_FLAG_BGSCAN);
	return rv;
}

int
mt7925_mcu_hw_scan_cancel(struct mwx_softc *sc)
{
	struct {
		struct mt7925_scan_hdr_tlv	hdr;
		struct {
			uint16_t		tag;
			uint16_t		len;
			uint8_t			is_ext_channel;
			uint8_t			rsv[3];
		} __packed cancel;
	} __packed req = {
		.hdr = {
			.seq_num = sc->sc_scan_seq_num,
			.bss_idx = sc->sc_vif.idx,
		},
		.cancel = {
			.tag = htole16(UNI_SCAN_CANCEL),
			.len = htole16(sizeof(req.cancel)),
		},
	};
	int rv;

	rv = mwx_mcu_send_msg(sc, MCU_UNI_CMD_SCAN_REQ, &req,
	    sizeof(req), NULL);
	if (rv == 0)
		sc->sc_flags &= ~(MWX_FLAG_SCANNING | MWX_FLAG_BGSCAN);
	return rv;
}

void
mwx_end_scan_task(void *arg)
{
	struct mwx_softc *sc = (struct mwx_softc *)arg;
	struct ieee80211com *ic = &sc->sc_ic;
	int rv, s;

	s = splnet();
	ieee80211_end_scan(&ic->ic_if);

	/*
	 * A background scan that ends on the BSS we are already using (the
	 * common case: the current AP is still the best one) keeps net80211
	 * in RUN and returns from ieee80211_end_scan() without selecting a
	 * channel.  The firmware, however, is still parked on the last
	 * channel of the scan, so neither the AP nor anything else can be
	 * heard: the interface reports RUN and passes no traffic, and the
	 * only symptom is a beacon-loss event a few seconds later.
	 *
	 * There is no ic_set_channel callback in this net80211 port, so put
	 * the radio back on the BSS channel explicitly.  When the scan
	 * ended with a different state (S_SCAN or a BSS switch) the
	 * following state transition programmes the channel itself; this
	 * only has to cover the "stay in RUN" case.
	 */
	if (ic->ic_state == IEEE80211_S_RUN && ic->ic_bss != NULL &&
	    ic->ic_bss->ni_chan != NULL) {
		if (sc->sc_hwtype == MWX_HW_MT7925)
			rv = mt7925_set_channel(sc);
		else
			rv = mt7921_set_channel(sc);
		if (rv != 0)
			printf("%s: could not restore the BSS channel after "
			    "the scan: %d\n", DEVNAME(sc), rv);
	}
	splx(s);
}

/*
 * Recover a radio that is not hearing the AP.
 *
 * Scheduled by the firmware's beacon-loss notification, by the watchdog
 * when a scan completion was lost, and by the receive path for both.
 * Runs from the state task queue because it transmits a management frame
 * and touches the 802.11 state, neither of which belongs in the receive
 * or timer path.  The probe response resets the management timer (see
 * ieee80211_recv_probe_resp()); if the AP does not answer, the timer
 * expires and ieee80211_watchdog() moves the interface to S_SCAN, which
 * reassociates and reinstalls the keys.
 */
void
mwx_bmiss_task(void *arg)
{
	struct mwx_softc *sc = (struct mwx_softc *)arg;
	struct ieee80211com *ic = &sc->sc_ic;
	int rv, s;

	s = splnet();

	if (ic->ic_opmode != IEEE80211_M_STA ||
	    ic->ic_state != IEEE80211_S_RUN ||
	    ic->ic_mgt_timer != 0) {
		splx(s);
		return;
	}

	/*
	 * The firmware also reports beacon loss for the APs it cannot
	 * hear from another channel while our own scan is running.  That
	 * is expected and is not worth a probe: ending the scan restores
	 * the BSS channel and its beacons.
	 *
	 * A scan that never completed is the exception.  net80211 will not
	 * end it (the notification is gone), so do it here and carry on
	 * with the channel restore and the probe, or the radio would stay
	 * off-channel for good.
	 */
	mwx_scan_check_stale(sc, mwx_uptime_ms());
	if (sc->sc_flags & MWX_FLAG_SCANNING) {
		splx(s);
		return;
	}

	printf("%s: no beacons from %s, probing the AP\n", DEVNAME(sc),
	    ether_sprintf(ic->ic_bss->ni_macaddr));

	/*
	 * Put the radio back on the BSS channel before probing.  The
	 * notification usually means the AP went away, but it is also the
	 * symptom of a radio that was left on another channel (a scan
	 * whose completion never arrived).  Re-programming the channel is
	 * cheap and makes the probe below leave on the right frequency;
	 * if the AP is really gone the probe goes unanswered and
	 * ieee80211_watchdog() moves us to S_SCAN as before.
	 */
	if (sc->sc_hwtype == MWX_HW_MT7925)
		rv = mt7925_set_channel(sc);
	else
		rv = mt7921_set_channel(sc);
	if (rv != 0)
		printf("%s: could not restore the BSS channel: %d\n",
		    DEVNAME(sc), rv);

	/*
	 * A directed probe request, exactly like iwm and iwx do.  Failure
	 * is not fatal: the management timer is left alone, so
	 * ieee80211_watchdog() can still fall back to a full scan.
	 */
	(void)ieee80211_send_mgmt(ic, ic->ic_bss,
	    IEEE80211_FC0_SUBTYPE_PROBE_REQ, 0, 0);

	splx(s);
}

void
mt7921_mcu_scan_event(struct mwx_softc *sc, struct mbuf *m)
{
	/*
	 * Always hand the completion to net80211, even when the cancel
	 * command fails.  Returning early used to leave MWX_FLAG_SCANNING
	 * and IEEE80211_F_BGSCAN set with the radio on the last scanned
	 * channel: no further scan could start, net80211 never restored
	 * the BSS channel, and the interface stayed in RUN without
	 * receiving anything.  Ending the scan lets mwx_end_scan_task()
	 * put the radio back on the associated channel and lets the state
	 * machine decide what to do with the results.
	 */
	if (mt7921_mcu_hw_scan_cancel(sc) != 0) {
		printf("%s: could not cancel scan, ending it anyway\n",
		    DEVNAME(sc));
		sc->sc_flags &= ~(MWX_FLAG_SCANNING | MWX_FLAG_BGSCAN);
	}
	task_add(sc->sc_nswq, &sc->sc_scan_task);
}

void
mt7925_mcu_scan_event(struct mwx_softc *sc, struct mbuf *m)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct {
		uint16_t	tag;
		uint16_t	len;
		uint8_t		data[];
	} __packed *tlv;
	struct mt76_connac_hw_scan_done *done;
	struct mt7925_mcu_scan_chinfo_event {
		uint8_t		nr_chan;
		uint8_t		alpha2[3];
	} __packed *chinfo;
	size_t len;

	if (mbuf_len(m) < sizeof(uint32_t)) {
		m_freem(m);
		return;
	}

	m_adj(m, sizeof(uint32_t));
	while (mbuf_len(m) >= sizeof(*tlv)) {
		tlv = (typeof(tlv))mtod(m, void *);
		len = le16toh(tlv->len);
		if (len < sizeof(*tlv) || len > mbuf_len(m))
			break;

		switch (le16toh(tlv->tag)) {
		case UNI_EVENT_SCAN_DONE_BASIC:
			if (len >= sizeof(*tlv) + sizeof(*done)) {
				done = (struct mt76_connac_hw_scan_done *)
				    tlv->data;
				if (DEVDEBUG(sc))
					printf("%s: mt7925 scan done seq %u "
					    "state %u complete %u sparse %u "
					    "beacon_scan %u beacon_2g %u "
					    "beacon_5g %u\n",
					    DEVNAME(sc), done->seq_num,
					    done->current_state,
					    done->complete_channel_num,
					    done->sparse_channel_num,
					    le32toh(done->beacon_scan_num),
					    le32toh(done->beacon_2g_num),
					    le32toh(done->beacon_5g_num));
			}
			break;
		case UNI_EVENT_SCAN_DONE_CHNLINFO:
			if (len >= sizeof(*tlv) + sizeof(*chinfo)) {
				chinfo = (struct mt7925_mcu_scan_chinfo_event *)
				    tlv->data;
				if (DEVDEBUG(sc))
					printf("%s: mt7925 scan chnlinfo "
					    "nr %u alpha2 %c%c\n",
					    DEVNAME(sc), chinfo->nr_chan,
					    chinfo->alpha2[0],
					    chinfo->alpha2[1]);
			}
			break;
		case UNI_EVENT_SCAN_DONE_NLO:
			break;
		default:
			if (DEVDEBUG(sc))
				printf("%s: mt7925 scan done tlv tag %u "
				    "len %zu\n", DEVNAME(sc),
				    le16toh(tlv->tag), len);
			break;
		}

		m_adj(m, len);
	}

	if ((sc->sc_flags & (MWX_FLAG_SCANNING | MWX_FLAG_BGSCAN)) == 0 &&
	    (ic->ic_state == IEEE80211_S_AUTH ||
	    ic->ic_state == IEEE80211_S_ASSOC ||
	    ic->ic_state == IEEE80211_S_RUN)) {
		printf("%s: mt7925 stale scan done in state %d\n",
		    DEVNAME(sc), ic->ic_state);
		return;
	}

	sc->sc_flags &= ~(MWX_FLAG_SCANNING | MWX_FLAG_BGSCAN);
	task_add(systq, &sc->sc_scan_task);
}


int
mt7921_mcu_set_mac_enable(struct mwx_softc *sc, int band, int enable)
{
	struct {
		uint8_t		enable;
		uint8_t		band;
		uint8_t		rsv[2];
	} __packed req = {
		.enable = enable,
		.band = band,
	};

	return mwx_mcu_send_wait(sc, MCU_EXT_CMD_MAC_INIT_CTRL, &req,
	    sizeof(req));
}

int
mt7921_mcu_set_channel_domain(struct mwx_softc *sc)
{
	struct {
		uint8_t		alpha2[4]; /* regulatory_request.alpha2 */
		uint8_t		bw_2g;	/* BW_20_40M		0
					 * BW_20M		1
					 * BW_20_40_80M		2
					 * BW_20_40_80_160M	3
					 * BW_20_40_80_8080M	4
					 */
		uint8_t		bw_5g;
		uint8_t		bw_6g;
		uint8_t		pad;
		uint8_t		n_2ch;
		uint8_t		n_5ch;
		uint8_t		n_6ch;
		uint8_t		pad2;
	} __packed *hdr;
	struct {
		uint16_t	hw_value;
		uint16_t	pad;
		uint32_t	flags;
	} __packed *channel;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_channel *chan;
	struct mbuf *m;
	int i, len;
	int n_2ch = 0, n_5ch = 0, n_6ch = 0;

	len = sizeof(*hdr) + IEEE80211_CHAN_MAX * sizeof(channel);
	m = mwx_mcu_alloc_msg(len);
	if (m == NULL)
		return ENOMEM;
	hdr = (typeof(hdr))mtod(m, void *);
	hdr->bw_2g = 0;	/* BW_20_40M */
	hdr->bw_5g = 3; /* BW_20_40_80_160M */
	hdr->bw_6g = 3; /* BW_20_40_80_160M */

	channel = (typeof(channel))(hdr + 1);

	for (i = 1; i <= IEEE80211_CHAN_MAX; i++) {
		chan = &ic->ic_channels[i];
		if (chan->ic_flags == 0)
			continue;

		channel->hw_value = htole16(i);
		channel->flags = htole32(chan->ic_flags);

		channel++;
		if (IEEE80211_IS_CHAN_2GHZ(chan))
			n_2ch++;
		else
			n_5ch++;
	}

	memcpy(hdr->alpha2, sc->sc_alpha2, sizeof(sc->sc_alpha2));
	hdr->n_2ch = n_2ch;
	hdr->n_5ch = n_5ch;
	hdr->n_6ch = n_6ch;

	if (DEVDEBUG(sc))
		printf("%s: set domain n2 %d n5 %d n6 %d\n",
		    DEVNAME(sc), n_2ch, n_5ch, n_6ch);
	mwx_mcu_set_len(m, channel);
	return mwx_mcu_send_mbuf(sc, MCU_CE_CMD_SET_CHAN_DOMAIN, m, NULL);
}

int
mt7925_mcu_set_channel_domain(struct mwx_softc *sc)
{
	struct {
		struct {
			uint8_t		alpha2[4];
			uint8_t		bw_2g;	/* BW_20_40M		0
						 * BW_20M		1
						 * BW_20_40_80M		2
						 * BW_20_40_80_160M	3
						 * BW_20_40_80_8080M	4
						 */
			uint8_t		bw_5g;
			uint8_t		bw_6g;
			uint8_t		pad;
		} __packed hdr;
		struct {
			uint16_t	tag;
			uint16_t	len;
			uint8_t		n_2ch;
			uint8_t		n_5ch;
			uint8_t		n_6ch;
			uint8_t		pad;
		} __packed n_ch;
	} __packed req = {
		.hdr = {
			.bw_2g = 0,	/* BW_20_40M */
			.bw_5g = 3,	/* BW_20_40_80_160M */
			.bw_6g = 3,
		},
		.n_ch = {
			.tag = htole16(2),
		},
	};
	struct mt76_connac_mcu_chan {
		uint16_t	hw_value;
		uint16_t	pad;
		uint32_t	flags;
	} __packed *channel;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_channel *chan;
	struct mbuf *m;
	int i, len;
	int n_2ch = 0, n_5ch = 0, n_6ch = 0;

	len = sizeof(req) + IEEE80211_CHAN_MAX * sizeof(*channel);
	m = mwx_mcu_alloc_msg(len);
	if (m == NULL)
		return ENOMEM;

	channel = (struct mt76_connac_mcu_chan *)(mtod(m, uint8_t *) + sizeof(req));

	for (i = 1; i <= IEEE80211_CHAN_MAX; i++) {
		chan = &ic->ic_channels[i];
		if (chan->ic_flags == 0)
			continue;

		channel->hw_value = htole16(ieee80211_mhz2ieee(chan->ic_freq,
		    chan->ic_flags));
		channel->flags = htole32(chan->ic_flags);

		channel++;
		if (IEEE80211_IS_CHAN_2GHZ(chan))
			n_2ch++;
		else
			n_5ch++;
	}

	req.n_ch.n_2ch = n_2ch;
	req.n_ch.n_5ch = n_5ch;
	req.n_ch.n_6ch = n_6ch;
	req.n_ch.len = htole16(sizeof(req.n_ch) +
	    (n_2ch + n_5ch + n_6ch) * sizeof(*channel));
	memcpy(req.hdr.alpha2, sc->sc_alpha2, sizeof(sc->sc_alpha2));
	memcpy(mtod(m, void *), &req, sizeof(req));

	if (DEVDEBUG(sc))
		printf("%s: set domain n2 %d n5 %d n6 %d\n",
		    DEVNAME(sc), n_2ch, n_5ch, n_6ch);
	mwx_mcu_set_len(m, channel);
	return mwx_mcu_send_mbuf_wait(sc, MCU_UNI_CMD_SET_DOMAIN_INFO, m);
}

int
mt7921_mcu_set_chan_info(struct mwx_softc *sc, int cmd)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = NULL;
	struct ieee80211_channel *channel;
	struct mwx_chanctx ctx;
	struct {
		uint8_t		control_ch;
		uint8_t		center_ch;
		uint8_t		bw;
		uint8_t		tx_streams_num;
		uint8_t		rx_streams;	/* mask or num */
		uint8_t		switch_reason;
		uint8_t		band_idx;
		uint8_t		center_ch2;	/* for 80+80 only */
		uint16_t	cac_case;
		uint8_t		channel_band;
		uint8_t		rsv0;
		uint32_t	outband_freq;
		uint8_t		txpower_drop;
		uint8_t		ap_bw;
		uint8_t		ap_center_ch;
		uint8_t		rsv1[57];
	} __packed req = {
		.tx_streams_num = sc->sc_capa.num_streams,
		.rx_streams = sc->sc_capa.antenna_mask,
		/*
		 * This is the radio/phy index, not the channel band.  The
		 * mt7921 family has a single phy, so it is always 0; the
		 * 2/5/6 GHz distinction is carried by channel_band below.
		 */
		.band_idx = 0,
	};

	if (ic->ic_opmode == IEEE80211_M_STA && ic->ic_bss != NULL) {
		ni = ic->ic_bss;
		channel = ni->ni_chan;
	} else
		channel = ic->ic_ibss_chan;

	if (channel == NULL)
		return EINVAL;

	mwx_get_chanctx(sc, ni, channel, &ctx);
	req.control_ch = ctx.control_ch;
	req.center_ch = ctx.center_ch;
	req.bw = ctx.bw;
	req.channel_band = ctx.channel_band;

#ifdef NOTYET
	if (dev->mt76.hw->conf.flags & IEEE80211_CONF_OFFCHANNEL)
		req.switch_reason = CH_SWITCH_SCAN_BYPASS_DPD;
	else if ((chandef->chan->flags & IEEE80211_CHAN_RADAR) &&
		 chandef->chan->dfs_state != NL80211_DFS_AVAILABLE)
		req.switch_reason = CH_SWITCH_DFS;
	else
#endif
		req.switch_reason = CH_SWITCH_NORMAL;

	if (cmd == MCU_EXT_CMD_CHANNEL_SWITCH)
		req.rx_streams = sc->sc_capa.num_streams;

#ifdef NOTYET
	if (chandef->width == NL80211_CHAN_WIDTH_80P80) {
		int freq2 = chandef->center_freq2;
		req.center_ch2 = ieee80211_frequency_to_channel(freq2);
	}
#endif

	return mwx_mcu_send_wait(sc, cmd, &req, sizeof(req));
}

/* hardcoded version of what linux does */
void
mt7921_mcu_build_sku(struct mwx_softc *sc, int band, int8_t *sku)
{
	int max_power = 127;
	int i, offset = 4;

	memset(sku, max_power, MT_SKU_POWER_LIMIT);

	if (band == MT_TX_PWR_BAND_2GHZ) {
		/* cck */
		memset(sku, 0x28, 4);
	}

	/* ofdm */
	memset(sku + offset, 0x28, 8);
	offset += 8;

	/* ht */
	for (i = 0; i < 2; i++) {
		memset(sku + offset, 0x28, 8);
		offset += 8;
	}
	sku[offset++] = 0x28;

	/* vht */
	for (i = 0; i < 4; i++) {
		/* this only sets 10 out of 12 bytes on purpose */
		memset(sku + offset, 0x28, 10);
		offset += 12;
	}

	/* he */
	for (i = 0; i < 7; i++) {
		memset(sku + offset, 0x28, 12);
		offset += 12;
	}
}

int
mt7921_mcu_rate_txpower_band(struct mwx_softc *sc, int band,
    const uint8_t *chans, int n_chans, int is_last)
{
	struct mt76_connac_sku_tlv *sku_tlv;
	struct mt76_connac_tx_power_limit_tlv *tx_power_tlv;
	struct mbuf *m;
	int batch_size = 8;
	const int len = sizeof(*tx_power_tlv) + batch_size * sizeof(*sku_tlv);
	int rv = 0, idx, j;

	for (idx = 0; idx < n_chans; ) {
		int num_ch = batch_size;

		m = mwx_mcu_alloc_msg(len);
		if (m == NULL)
			return ENOMEM;
		tx_power_tlv = mtod(m, struct mt76_connac_tx_power_limit_tlv *);
		tx_power_tlv->n_chan = num_ch;
		tx_power_tlv->band = band;
		memcpy(tx_power_tlv->alpha2, sc->sc_alpha2,
		    sizeof(sc->sc_alpha2));

		if (n_chans - idx < batch_size) {
			num_ch = n_chans - idx;
			if (is_last)
				tx_power_tlv->last_msg = 1;
		}

		sku_tlv = (struct mt76_connac_sku_tlv *)(tx_power_tlv + 1);

		for (j = 0; j < num_ch; j++, idx++) {
			sku_tlv->channel = chans[idx];
			mt7921_mcu_build_sku(sc, band, sku_tlv->pwr_limit);
			sku_tlv++;
		}

		mwx_mcu_set_len(m, sku_tlv);
		rv = mwx_mcu_send_mbuf(sc, MCU_CE_CMD_SET_RATE_TX_POWER, m,
		    NULL);
		if (rv != 0)
			break;
	}

	return rv;
}

int
mt7921_mcu_set_rate_txpower(struct mwx_softc *sc)
{
	static const uint8_t chan_list_2ghz[] = {
	    1, 2,  3,  4,  5,  6,  7,
	    8, 9, 10, 11, 12, 13, 14
	};
	static const uint8_t chan_list_5ghz[] = {
	    36,  38,  40,  42,  44,  46,  48,
	    50,  52,  54,  56,  58,  60,  62,
	    64, 100, 102, 104, 106, 108, 110,
	    112, 114, 116, 118, 120, 122, 124,
	    126, 128, 132, 134, 136, 138, 140,
	    142, 144, 149, 151, 153, 155, 157,
	    159, 161, 165
	};
	static const uint8_t chan_list_6ghz[] = {
	    1,   3,   5,   7,   9,  11,  13,
	    15,  17,  19,  21,  23,  25,  27,
	    29,  33,  35,  37,  39,  41,  43,
	    45,  47,  49,  51,  53,  55,  57,
	    59,  61,  65,  67,  69,  71,  73,
	    75,  77,  79,  81,  83,  85,  87,
	    89,  91,  93,  97,  99, 101, 103,
	    105, 107, 109, 111, 113, 115, 117,
	    119, 121, 123, 125, 129, 131, 133,
	    135, 137, 139, 141, 143, 145, 147,
	    149, 151, 153, 155, 157, 161, 163,
	    165, 167, 169, 171, 173, 175, 177,
	    179, 181, 183, 185, 187, 189, 193,
	    195, 197, 199, 201, 203, 205, 207,
	    209, 211, 213, 215, 217, 219, 221,
	    225, 227, 229, 233
	};
	int rv = 0;

	if (sc->sc_capa.has_2ghz)
		rv = mt7921_mcu_rate_txpower_band(sc, MT_TX_PWR_BAND_2GHZ,
		    chan_list_2ghz, nitems(chan_list_2ghz),
		    !(sc->sc_capa.has_5ghz || sc->sc_capa.has_6ghz));
	if (rv == 0 && sc->sc_capa.has_5ghz)
		rv = mt7921_mcu_rate_txpower_band(sc, MT_TX_PWR_BAND_5GHZ,
		    chan_list_5ghz, nitems(chan_list_5ghz),
		    !sc->sc_capa.has_6ghz);
	if (rv == 0 && sc->sc_capa.has_6ghz)
		rv = mt7921_mcu_rate_txpower_band(sc, MT_TX_PWR_BAND_6GHZ,
		    chan_list_6ghz, nitems(chan_list_6ghz), 1);
	return rv;
}

void
mwx_mac_reset_counters(struct mwx_softc *sc)
{
	int i;

	for (i = 0; i < 4; i++) {
		mwx_read(sc, MT_TX_AGG_CNT(0, i));
		mwx_read(sc, MT_TX_AGG_CNT2(0, i));
	}

	/* XXX TODO stats in softc */

	/* reset airtime counters */
	mwx_read(sc, MT_MIB_SDR9(0));
	mwx_read(sc, MT_MIB_SDR36(0));
	mwx_read(sc, MT_MIB_SDR37(0));
	mwx_set(sc, MT_WF_RMAC_MIB_TIME0(0), MT_WF_RMAC_MIB_RXTIME_CLR);
	mwx_set(sc, MT_WF_RMAC_MIB_AIRTIME0(0), MT_WF_RMAC_MIB_RXTIME_CLR);
}

void
mt7921_mac_set_timing(struct mwx_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_channel *chan = NULL;
	uint16_t coverage_class = 0;		/* XXX */
	uint32_t val, reg_offset;
	uint32_t cck = MT_TIMEOUT_CCK_DEF_VAL;
	uint32_t ofdm = MT_TIMEOUT_OFDM_DEF_VAL;
	uint32_t offset;
	int is_2ghz;
	uint32_t sifs;
	uint32_t slottime;

	if (ic->ic_bss != NULL && ic->ic_bss->ni_chan != NULL)
		chan = ic->ic_bss->ni_chan;
	else
		chan = ic->ic_ibss_chan;
	is_2ghz = chan != NULL && IEEE80211_IS_CHAN_2GHZ(chan);
	sifs = is_2ghz ? 10 : 16;
	slottime = (is_2ghz && !(ic->ic_flags & IEEE80211_F_SHSLOT)) ?
	    IEEE80211_DUR_DS_SLOT : IEEE80211_DUR_DS_SHSLOT;

#ifdef NOTYET
	if (!test_bit(MT76_STATE_RUNNING, &phy->mt76->state))
		return;
#endif

	mwx_set(sc, MT_ARB_SCR(0),
	    MT_ARB_SCR_TX_DISABLE | MT_ARB_SCR_RX_DISABLE);
	delay(1);

	offset = 3 * coverage_class;
	reg_offset = offset | (offset << 16);

	mwx_write(sc, MT_TMAC_CDTR(0), cck + reg_offset);
	mwx_write(sc, MT_TMAC_ODTR(0), ofdm + reg_offset);
	mwx_write(sc, MT_TMAC_ICR0(0),
	    MT_IFS_EIFS_DEF | MT_IFS_RIFS_DEF |
	    (sifs << MT_IFS_SIFS_SHIFT) |
	    (slottime << MT_IFS_SLOT_SHIFT));

	if (slottime < 20 || !is_2ghz)
		val = MT7921_CFEND_RATE_DEFAULT;
	else
		val = MT7921_CFEND_RATE_11B;

	mwx_rmw(sc, MT_AGG_ACR0(0), val, MT_AGG_ACR_CFEND_RATE_MASK);
	mwx_clear(sc, MT_ARB_SCR(0),
	    MT_ARB_SCR_TX_DISABLE | MT_ARB_SCR_RX_DISABLE);
}

/*
 * STA type the firmware should use for the interface's own BSS/station
 * entry.  The device and BSS notifications both need it, and letting them
 * disagree would leave the firmware with two different ideas about what
 * kind of network the interface describes.
 */
static uint32_t
mwx_bss_conn_type(struct mwx_softc *sc)
{
	switch (sc->sc_ic.ic_opmode) {
	case IEEE80211_M_MONITOR:
#ifndef IEEE80211_STA_ONLY
	case IEEE80211_M_HOSTAP:
#endif
		return STA_TYPE_AP | NETWORK_INFRA;
	case IEEE80211_M_STA:
		return STA_TYPE_STA | NETWORK_INFRA;
#ifndef IEEE80211_STA_ONLY
	case IEEE80211_M_IBSS:
	case IEEE80211_M_AHDEMO:
		return STA_TYPE_ADHOC | NETWORK_IBSS;
#endif
	default:
		panic("%s: unknown operation mode", DEVNAME(sc));
		/* NOTREACHED */
		return 0;
	}
}

int
mwx_mcu_uni_add_dev(struct mwx_softc *sc, struct mwx_vif *mvif,
    struct mwx_node *mn, int enable)
{
	struct {
		struct {
			uint8_t		omac_idx;
			uint8_t		band_idx;
			uint16_t	pad;
		} __packed hdr;
		struct req_tlv {
			uint16_t	tag;
			uint16_t	len;
			uint8_t		active;
			uint8_t		pad;
			uint8_t		omac_addr[ETHER_ADDR_LEN];
		} __packed tlv;
	} dev_req = {
		.hdr = {
			.omac_idx = mvif->omac_idx,
			.band_idx = mvif->band_idx,
		},
		.tlv = {
			.tag = htole16(DEV_INFO_ACTIVE),
			.len = htole16(sizeof(dev_req.tlv)),
			.active = enable,
		},
	};
	struct {
		struct {
			uint8_t		bss_idx;
			uint8_t		pad[3];
		} __packed hdr;
		struct mt76_connac_bss_basic_tlv basic;
	} basic_req = {
		.hdr = {
			.bss_idx = mvif->idx,
		},
		.basic = {
			.tag = htole16(UNI_BSS_INFO_BASIC),
			.len = htole16(sizeof(basic_req.basic)),
			.omac_idx = mvif->omac_idx,
			.band_idx = mvif->band_idx,
			.wmm_idx = mvif->wmm_idx,
			.active = enable,
			.bmc_tx_wlan_idx = htole16(mn->wcid),
			.sta_idx = htole16(mn->wcid),
			.conn_state = 1,
		},
	};
	int rv, idx, cmd, len;
	void *data;

	basic_req.basic.conn_type = htole32(mwx_bss_conn_type(sc));

	idx = mvif->omac_idx > EXT_BSSID_START ? HW_BSSID_0 : mvif->omac_idx;
	basic_req.basic.hw_bss_idx = idx;

	memcpy(dev_req.tlv.omac_addr, sc->sc_lladdr, ETHER_ADDR_LEN);

	if (enable) {
		cmd = MCU_UNI_CMD_DEV_INFO_UPDATE;
		data = &dev_req;
		len = sizeof(dev_req);
	} else {
		cmd = MCU_UNI_CMD_BSS_INFO_UPDATE;
		data = &basic_req;
		len = sizeof(basic_req);
	}

	if (DEVDEBUG(sc))
		printf("%s: %s cmd %x mvif idx %d omac %d band %d "
		    "wmm %d\n", DEVNAME(sc), __func__, cmd, mvif->idx,
		    mvif->omac_idx, mvif->band_idx, mvif->wmm_idx);
	rv = mwx_mcu_send_wait(sc, cmd, data, len);
	if (rv < 0)
		return rv;

	if (enable) {
		cmd = MCU_UNI_CMD_BSS_INFO_UPDATE;
		data = &basic_req;
		len = sizeof(basic_req);
	} else {
		cmd = MCU_UNI_CMD_DEV_INFO_UPDATE;
		data = &dev_req;
		len = sizeof(dev_req);
	}

	if (DEVDEBUG(sc))
		printf("%s: %s cmd %x wcid %d\n", DEVNAME(sc), __func__,
		    cmd, mn->wcid);
	return mwx_mcu_send_wait(sc, cmd, data, len);
}

/*
 * PHY mode bitmap for UNI_BSS_INFO_BASIC, in the encoding the firmware
 * expects (the same one the reference driver's mt76_connac_get_phy_mode()
 * produces).
 */
static uint8_t
mwx_bss_phymode(struct ieee80211_node *ni)
{
	uint8_t mode = 0;

	if (ni == NULL || ni->ni_chan == NULL)
		return 0;

	if (IEEE80211_IS_CHAN_2GHZ(ni->ni_chan)) {
		mode = PHY_MODE_B | PHY_MODE_G;
		if (ieee80211_node_supports_ht(ni))
			mode |= PHY_MODE_GN;
	} else if (IEEE80211_IS_CHAN_5GHZ(ni->ni_chan)) {
		mode = PHY_MODE_A;
		if (ieee80211_node_supports_ht(ni))
			mode |= PHY_MODE_AN;
		if (ieee80211_node_supports_vht(ni))
			mode |= PHY_MODE_AC;
	}

	return mode;
}

/*
 * Announce the BSS this station has joined.
 *
 * The interface is added long before there is a BSSID to describe, and the
 * AP's station record is created when authentication starts, i.e. while
 * the association ID is still zero.  Neither of those describes a link
 * that can carry data: the firmware needs the BSSID, the beacon interval,
 * the DTIM period and the negotiated PHY mode to program its receive
 * filter and rate/aggregation state, and the station record has to be
 * re-sent once the AID is known.
 *
 * The reference driver sends exactly this on the transition from
 * authenticated to associated (mt7921_mac_sta_event() calls
 * mt76_connac_mcu_uni_add_bss() and then mt7921_mcu_sta_update() with
 * MT76_STA_INFO_STATE_ASSOC).  mwx_newstate() does the same before it
 * hands the state change to net80211, so that from the moment the port
 * becomes valid the firmware is transmitting and receiving for the right
 * BSS.  Without it the firmware keeps the interface's pre-association
 * idea of the BSS, which shows up as an association that reports RUN
 * while every frame - including the one carrying message 2 of the 4-way
 * handshake - is lost, and DHCP never starts.
 */
int
mwx_mcu_uni_add_bss(struct mwx_softc *sc, struct mwx_vif *mvif,
    struct ieee80211_node *ni, struct mwx_node *mn, int enable)
{
	struct {
		struct {
			uint8_t		bss_idx;
			uint8_t		pad[3];
		} __packed hdr;
		struct mt76_connac_bss_basic_tlv basic;
		struct {
			uint16_t	tag;
			uint16_t	len;
			uint8_t		qos;
			uint8_t		pad[3];
		} __packed qos;
	} basic_req = {
		.hdr = {
			.bss_idx = mvif->idx,
		},
		.basic = {
			.tag = htole16(UNI_BSS_INFO_BASIC),
			.len = htole16(sizeof(basic_req.basic)),
			.active = 1,
			.omac_idx = mvif->omac_idx,
			.band_idx = mvif->band_idx,
			.wmm_idx = mvif->wmm_idx,
			.conn_type = htole32(STA_TYPE_STA | NETWORK_INFRA),
			/*
			 * The firmware's name for this field is misleading:
			 * 0 means "connected to the BSS", i.e. the value it
			 * expects together with active=1 when the station is
			 * associated (same as the reference driver, which
			 * stores !enable here).
			 */
			.conn_state = !enable,
			.bmc_tx_wlan_idx = htole16(mn->wcid),
			.sta_idx = htole16(mn->wcid),
		},
		.qos = {
			.tag = htole16(UNI_BSS_INFO_QBSS),
			.len = htole16(sizeof(basic_req.qos)),
			.qos = ni != NULL &&
			    (ni->ni_flags & IEEE80211_NODE_QOS) != 0,
		},
	};

	int rv, seq;
	uint32_t status = 0;

	basic_req.basic.hw_bss_idx =
	    mvif->omac_idx > EXT_BSSID_START ? HW_BSSID_0 : mvif->omac_idx;
	basic_req.basic.bcn_interval = htole16(ni->ni_intval);
	basic_req.basic.dtim_period = ni->ni_dtimperiod;
	basic_req.basic.phymode = mwx_bss_phymode(ni);
	basic_req.basic.nonht_basic_phy =
	    htole16(mt7921_get_phy_mode_v2(sc, ni));
	IEEE80211_ADDR_COPY(basic_req.basic.bssid, ni->ni_bssid);

	rv = mwx_mcu_send_msg(sc, MCU_UNI_CMD_BSS_INFO_UPDATE, &basic_req,
	    sizeof(basic_req), &seq);
	if (rv != 0)
		return rv;

	rv = mwx_mcu_wait_resp_int(sc, MCU_UNI_CMD_BSS_INFO_UPDATE, seq,
	    &status);
	if (rv != 0)
		return rv;
	if (status != 0) {
		/*
		 * The firmware kept its old view of the BSS.  Report it:
		 * net80211 would otherwise mark the port valid and the
		 * interface would look associated while nothing can pass.
		 */
		printf("%s: %s: firmware rejected BSS update, status %u\n",
		    DEVNAME(sc), __func__, status);
		return EIO;
	}

	return 0;
}

int
mwx_mcu_set_sniffer(struct mwx_softc *sc, int enable)
{
	struct {
		uint8_t		band_idx;
		uint8_t		pad[3];
		struct sniffer_enable_tlv {
			uint16_t	tag;
			uint16_t	len;
			uint8_t		enable;
			uint8_t		pad[3];
		}		enable;
	} req = {
		.band_idx = 0,
		.enable = {
			.tag = htole16(UNI_SNIFFER_ENABLE),
			.len = htole16(sizeof(req.enable)),
			.enable = enable,
		},
	};

	return mwx_mcu_send_wait(sc, MCU_UNI_CMD_SNIFFER, &req, sizeof(req));
}

int
mt7925_mcu_config_sniffer(struct mwx_softc *sc)
{
	struct {
		uint8_t		band_idx;
		uint8_t		pad[3];
		struct sniffer_config_tlv {
			uint16_t	tag;
			uint16_t	len;
			uint16_t	aid;
			uint8_t		ch_band;
			uint8_t		bw;
			uint8_t		control_ch;
			uint8_t		sco;
			uint8_t		center_ch;
			uint8_t		center_ch2;
			uint8_t		drop_err;
			uint8_t		pad[3];
		}		tlv;
	} req = {
		.band_idx = 0,
		.tlv = {
			.tag = htole16(UNI_SNIFFER_CONFIG),
			.len = htole16(sizeof(req.tlv)),
			.drop_err = 1,
		},
	};
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_channel *chan = ic->ic_des_chan;
	uint8_t ctrl_ch;

	if (chan == IEEE80211_CHAN_ANYC)
		chan = ic->ic_ibss_chan;
	if (chan == IEEE80211_CHAN_ANYC) {
		printf("%s: monitor: no channel set\n", sc->sc_dev.dv_xname);
		return EINVAL;
	}

	if (IEEE80211_IS_CHAN_2GHZ(chan))
		req.tlv.ch_band = 1;
	else if (IEEE80211_IS_CHAN_5GHZ(chan))
		req.tlv.ch_band = 2;
	else
		req.tlv.ch_band = 3; /* 6Ghz */

	ctrl_ch = ieee80211_chan2ieee(ic, chan);
	req.tlv.control_ch = req.tlv.center_ch = ctrl_ch;

	return mwx_mcu_send_wait(sc, MCU_UNI_CMD_SNIFFER, &req, sizeof(req));
}

int
mt7921_mcu_set_beacon_filter(struct mwx_softc *sc, int enable)
{
	int rv;

	if (enable) {
#ifdef NOTYET
		rv = mt7921_mcu_uni_bss_bcnft(dev, vif, true);
		if (rv)
			return rv;
#endif
		mwx_set(sc, MT_WF_RFCR(0), MT_WF_RFCR_DROP_OTHER_BEACON);
	} else {
		rv = mt7921_mcu_set_bss_pm(sc, 0);
		if (rv)
			return rv;
		mwx_clear(sc, MT_WF_RFCR(0), MT_WF_RFCR_DROP_OTHER_BEACON);
	}
	return 0;
}

int
mt7921_mcu_set_bss_pm(struct mwx_softc *sc, int enable)
{
#ifdef NOTYET
	struct {
		uint8_t		bss_idx;
		uint8_t		dtim_period;
		uint16_t	aid;
		uint16_t	bcn_interval;
		uint16_t	atim_window;
		uint8_t		uapsd;
		uint8_t		bmc_delivered_ac;
		uint8_t		bmc_triggered_ac;
		uint8_t		pad;
	} req = {
		.bss_idx = mvif->mt76.idx,
		.aid = htole16(vif->cfg.aid),
		.dtim_period = vif->bss_conf.dtim_period,
		.bcn_interval = htole16(vif->bss_conf.beacon_int),
	};
#endif
	struct {
		uint8_t		bss_idx;
		uint8_t		pad[3];
	} req_hdr = {
		.bss_idx = /* mvif->mt76.idx XXX */ 0,
	};
	int rv;

	rv = mwx_mcu_send_msg(sc, MCU_CE_CMD_SET_BSS_ABORT,
		&req_hdr, sizeof(req_hdr), NULL);
#ifdef NOTYET
	if (rv != 0 || !enable)
		return rv;
	rv = mwx_mcu_send_msg(sc, MCU_CE_CMD_SET_BSS_CONNECTED,
		&req, sizeof(req), NULL);
#endif
	return rv;
}

#define IEEE80211_NUM_ACS	4
int
mt7921_mcu_set_tx(struct mwx_softc *sc, struct mwx_vif *mvif)
{
	struct mt7921_edca {
		uint16_t	cw_min;
		uint16_t	cw_max;
		uint16_t	txop;
		uint16_t	aifs;
		uint8_t		guardtime;
		uint8_t		acm;
	} __packed;
	struct mt7921_mcu_tx {
		struct mt7921_edca	edca[IEEE80211_NUM_ACS];
		uint8_t			bss_idx;
		uint8_t			qos;
		uint8_t			wmm_idx;
		uint8_t			pad;
	} __packed req = {
		.bss_idx = mvif->idx,
		.qos = /* vif->bss_conf.qos */ 0,
		.wmm_idx = mvif->wmm_idx,
	};
#ifdef NOTYET
	struct mu_edca {
		uint8_t		cw_min;
		uint8_t		cw_max;
		uint8_t		aifsn;
		uint8_t		acm;
		uint8_t		timer;
		uint8_t		padding[3];
	};
	struct mt7921_mcu_mu_tx {
		uint8_t		ver;
		uint8_t		pad0;
		uint16_t	len;
		uint8_t		bss_idx;
		uint8_t		qos;
		uint8_t		wmm_idx;
		uint8_t		pad1;
		struct mu_edca	edca[IEEE80211_NUM_ACS];
		uint8_t		pad3[32];
	} __packed req_mu = {
		.bss_idx = mvif->mt76.idx,
		.qos = vif->bss_conf.qos,
		.wmm_idx = mvif->mt76.wmm_idx,
	};
#endif
	static const int to_aci[] = { 1, 0, 2, 3 };
	int ac, rv;

	for (ac = 0; ac < IEEE80211_NUM_ACS; ac++) {
		//struct ieee80211_tx_queue_params *q = &mvif->queue_params[ac];
		struct mt7921_edca *e = &req.edca[to_aci[ac]];

		e->aifs = htole16(/* q->aifs */ 2);
		e->txop = htole16(/* q->txop */ 0);

#ifdef NOTYET
		if (q->cw_min)
			e->cw_min = htole16(q->cw_min);
		else
#endif
			e->cw_min = htole16(5);

#ifdef NOTYET
		if ( q->cw_max)
			e->cw_max = htole16(q->cw_max);
		else
#endif
			e->cw_max = htole16(10);
	}

	rv = mwx_mcu_send_msg(sc, MCU_CE_CMD_SET_EDCA_PARMS, &req,
	    sizeof(req), NULL);

#ifdef NOTYET
	if (rv)
		return rv;
	if (!vif->bss_conf.he_support)
		return 0;

	for (ac = 0; ac < IEEE80211_NUM_ACS; ac++) {
		struct ieee80211_he_mu_edca_param_ac_rec *q;
		struct mu_edca *e;

		if (!mvif->queue_params[ac].mu_edca)
			break;

		q = &mvif->queue_params[ac].mu_edca_param_rec;
		e = &(req_mu.edca[to_aci[ac]]);

		e->cw_min = q->ecw_min_max & 0xf;
		e->cw_max = (q->ecw_min_max & 0xf0) >> 4;
		e->aifsn = q->aifsn;
		e->timer = q->mu_edca_timer;
	}

	rv = mt76_mcu_send_msg(&dev->mt76, MCU_CE_CMD(SET_MU_EDCA_PARMS),
	    &req_mu, sizeof(req_mu), false);
#endif
	return rv;
}

int
mt7925_mcu_set_tx(struct mwx_softc *sc, struct mwx_vif *mvif)
{
	struct mt7925_edca {
		uint16_t	tag;
		uint16_t	len;
		uint8_t		queue;
		uint8_t		set;
		uint8_t		cw_min;
		uint8_t		cw_max;
		uint16_t	txop;
		uint8_t		aifs;
		uint8_t		rsv;
	} __packed;
	struct mt7925_mcu_tx {
		uint8_t			bss_idx;
		uint8_t			pad[3];
		struct mt7925_edca	edca[IEEE80211_NUM_ACS];
	} __packed req_mt7925 = {
		.bss_idx = mvif->idx,
	};
	const uint8_t wmm_param_set = 0x0f;
	int ac, rv;

	for (ac = 0; ac < IEEE80211_NUM_ACS; ac++) {
		struct mt7925_edca *e = &req_mt7925.edca[ac];

		e->tag = htole16(0);
		e->len = htole16(sizeof(*e));
		e->queue = ac;
		e->set = wmm_param_set;
		e->cw_min = 5;
		e->cw_max = 10;
		e->txop = 0;
		e->aifs = 2;
	}
	rv = mwx_mcu_send_msg(sc, MCU_UNI_CMD_EDCA_UPDATE,
	    &req_mt7925, sizeof(req_mt7925), NULL);
	return rv;
}

int
mwx_mcu_set_hif_suspend(struct mwx_softc *sc, int suspend)
{
	struct {
		struct {
			uint8_t	hif_type;	/* 0x2: HIF_PCIE */
			uint8_t	pad[3];
		} __packed hdr;
		struct {
			uint16_t	tag;
			uint16_t	len;
			uint8_t		suspend;
			uint8_t		pad[7];
		} __packed hif_suspend;
	} req = {
		.hdr = {
			.hif_type = 0x2,
		},
		.hif_suspend = {
			.tag = htole16(UNI_HIF_CTRL_BASIC),
			.len = htole16(sizeof(req.hif_suspend)),
			.suspend = suspend
		},
	};

	return mwx_mcu_send_wait(sc, MCU_UNI_CMD_HIF_CTRL, &req, sizeof(req));
}

int
mt7921_mac_fill_rx(struct mwx_softc *sc, struct mbuf *m,
    struct ieee80211_rxinfo *rxi)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t *rxd, *rxv = NULL;
	uint32_t rxd0, rxd1, rxd2, rxd3, rxd4;
//	uint32_t mode = 0;
	uint16_t hdr_gap /*, seq_ctrl = 0, fc = 0 */;
	uint8_t chfnum, remove_pad /*, qos_ctl = 0, amsdu_info */;
	int idx, unicast, num_rxd = 6;
	int i;
//	bool insert_ccmp_hdr = false;

	if (mbuf_len(m) < num_rxd * sizeof(uint32_t))
		return -1;

	rxd = mtod(m, uint32_t *);
	rxd0 = le32toh(rxd[0]);
	rxd1 = le32toh(rxd[1]);
	rxd2 = le32toh(rxd[2]);
	rxd3 = le32toh(rxd[3]);
	rxd4 = le32toh(rxd[4]);

	if (rxd1 & MT_RXD1_NORMAL_BAND_IDX)
		return -1;

	if (rxd2 & MT_RXD2_NORMAL_AMSDU_ERR)
		return -1;

	if (rxd2 & MT_RXD2_NORMAL_HDR_TRANS)
		return -1;

	/* ICV error or CCMP/BIP/WPI MIC error */
	if (rxd1 & MT_RXD1_NORMAL_ICV_ERR) {
		ic->ic_stats.is_rx_decryptcrc++;
		return -1;
	}

	if (rxd1 & MT_RXD1_NORMAL_FCS_ERR)
		return -1;

	if (rxd1 & MT_RXD1_NORMAL_TKIP_MIC_ERR) {
		/* report MIC failures to net80211 for TKIP */
		ic->ic_stats.is_rx_locmicfail++;
		ieee80211_michael_mic_failure(ic, 0/* XXX */);
		return -1;
	}


	chfnum = (rxd3 & MT_RXD3_NORMAL_CH_NUM_MASK) >>
	    MT_RXD3_NORMAL_CH_NUM_SHIFT;
	unicast = (rxd3 & MT_RXD3_NORMAL_ADDR_TYPE_MASK) == MT_RXD3_NORMAL_U2M;
	idx = rxd1 & MT_RXD1_NORMAL_WLAN_IDX_MASK;

#if 0
	status->wcid = mt7921_rx_get_wcid(dev, idx, unicast);
	if (status->wcid) {
		struct mt7921_sta *msta;

		msta = container_of(status->wcid, struct mt7921_sta, wcid);
		spin_lock_bh(&dev->sta_poll_lock);
		if (list_empty(&msta->poll_list))
			list_add_tail(&msta->poll_list, &dev->sta_poll_list);
		spin_unlock_bh(&dev->sta_poll_lock);
	}
#endif

#if NOTYET
	if ((rxd0 & (MT_RXD0_NORMAL_IP_SUM | MT_RXD0_NORMAL_UDP_TCP_SUM)) ==
	    (MT_RXD0_NORMAL_IP_SUM | MT_RXD0_NORMAL_UDP_TCP_SUM)) {
		m->m_pkthdr.csum_flags = M_IPV4_CSUM_IN_OK |
		    M_TCP_CSUM_IN_OK | M_UDP_CSUM_IN_OK;
	}
#endif

	if ((rxd1 & MT_RXD1_NORMAL_SEC_MODE_MASK) != 0 &&
	    !(rxd1 & (MT_RXD1_NORMAL_CLM | MT_RXD1_NORMAL_CM)))
		rxi->rxi_flags |= IEEE80211_RXI_HWDEC;

	remove_pad = (rxd2 & MT_RXD2_NORMAL_HDR_OFFSET_MASK) >>
	    MT_RXD2_NORMAL_HDR_OFFSET_SHIFT;

	if (rxd2 & MT_RXD2_NORMAL_MAX_LEN_ERROR)
		return -EINVAL;

	rxd += 6;

	if (rxd1 & MT_RXD1_NORMAL_GROUP_4) {
		num_rxd += 4;
		rxd += 4;
	}
	if (rxd1 & MT_RXD1_NORMAL_GROUP_1) {
		num_rxd += 4;
		rxd += 4;
	}
	if (rxd1 & MT_RXD1_NORMAL_GROUP_2) {
		num_rxd += 2;
		rxd += 2;
	}
	if (rxd1 & MT_RXD1_NORMAL_GROUP_3) {
		uint32_t v0, v1;
		int8_t chain[4], signal;

		rxv = rxd;
		num_rxd += 2;
		rxd += 2;

		v0 = le32toh(rxv[0]); /* XXX not implemented yet */
		v1 = le32toh(rxv[1]);

		if (rxd1 & MT_RXD1_NORMAL_GROUP_5) {
			num_rxd += 18;
			rxd += 6;
			rxv = rxd;
			v1 = le32toh(rxv[0]);
			rxd += 12;
		}

		chain[0] = rcpi_to_rssi(MT_PRXV_RCPI0_SHIFT, v1);
		chain[1] = rcpi_to_rssi(MT_PRXV_RCPI1_SHIFT, v1);
		chain[2] = rcpi_to_rssi(MT_PRXV_RCPI2_SHIFT, v1);
		chain[3] = rcpi_to_rssi(MT_PRXV_RCPI3_SHIFT, v1);

		signal = -128;
		for (i = 0; i < sc->sc_capa.num_streams; i++) {
			if (!(sc->sc_capa.antenna_mask & (1U << i)))
				continue;
			if (chain[i] >= 0)
				continue;
			signal = MAX(signal, chain[i]);
		}
		rxi->rxi_rssi = signal;
	}

	if (mbuf_len(m) < num_rxd * sizeof(uint32_t))
		return -1;

#if 0
	if (rxd1 & MT_RXD1_NORMAL_GROUP_4) {
		uint32_t v0 = le32toh(rxd[0]);
		uint32_t v2 = le32toh(rxd[2]);

		fc = htole16(FIELD_GET(MT_RXD6_FRAME_CONTROL, v0));
		seq_ctrl = FIELD_GET(MT_RXD8_SEQ_CTRL, v2);
		qos_ctl = FIELD_GET(MT_RXD8_QOS_CTL, v2);

		rxd += 4;
	}

	if (rxd1 & MT_RXD1_NORMAL_GROUP_1) {
		u8 *data = (u8 *)rxd;

		if (status->flag & RX_FLAG_DECRYPTED) {
			switch (FIELD_GET(MT_RXD1_NORMAL_SEC_MODE, rxd1)) {
			case MT_CIPHER_AES_CCMP:
			case MT_CIPHER_CCMP_CCX:
			case MT_CIPHER_CCMP_256:
				insert_ccmp_hdr =
					(rxd2 & MT_RXD2_NORMAL_FRAG);
				/* FALLTHROUGH */
			case MT_CIPHER_TKIP:
			case MT_CIPHER_TKIP_NO_MIC:
			case MT_CIPHER_GCMP:
			case MT_CIPHER_GCMP_256:
				status->iv[0] = data[5];
				status->iv[1] = data[4];
				status->iv[2] = data[3];
				status->iv[3] = data[2];
				status->iv[4] = data[1];
				status->iv[5] = data[0];
				break;
			default:
				break;
			}
		}
		rxd += 4;
	}

	if (rxd1 & MT_RXD1_NORMAL_GROUP_2) {
		status->timestamp = le32_to_cpu(rxd[0]);
		status->flag |= RX_FLAG_MACTIME_START;

		if (!(rxd2 & MT_RXD2_NORMAL_NON_AMPDU)) {
			status->flag |= RX_FLAG_AMPDU_DETAILS;

			/* all subframes of an A-MPDU have the same timestamp */
			if (phy->rx_ampdu_ts != status->timestamp) {
				if (!++phy->ampdu_ref)
					phy->ampdu_ref++;
			}
			phy->rx_ampdu_ts = status->timestamp;

			status->ampdu_ref = phy->ampdu_ref;
		}

		rxd += 2;
	}

	/* RXD Group 3 - P-RXV */
	if (rxd1 & MT_RXD1_NORMAL_GROUP_3) {
		u8 stbc, gi;
		u32 v0, v1;
		bool cck;

		rxv = rxd;
		rxd += 2;

		v0 = le32_to_cpu(rxv[0]);
		v1 = le32_to_cpu(rxv[1]);

		if (v0 & MT_PRXV_HT_AD_CODE)
			status->enc_flags |= RX_ENC_FLAG_LDPC;

		status->chains = mphy->antenna_mask;
		status->chain_signal[0] = to_rssi(MT_PRXV_RCPI0, v1);
		status->chain_signal[1] = to_rssi(MT_PRXV_RCPI1, v1);
		status->chain_signal[2] = to_rssi(MT_PRXV_RCPI2, v1);
		status->chain_signal[3] = to_rssi(MT_PRXV_RCPI3, v1);
		status->signal = -128;
		for (i = 0; i < hweight8(mphy->antenna_mask); i++) {
			if (!(status->chains & BIT(i)) ||
			    status->chain_signal[i] >= 0)
				continue;

			status->signal = max(status->signal,
					     status->chain_signal[i]);
		}

		stbc = FIELD_GET(MT_PRXV_STBC, v0);
		gi = FIELD_GET(MT_PRXV_SGI, v0);
		cck = false;

		idx = i = FIELD_GET(MT_PRXV_TX_RATE, v0);
		mode = FIELD_GET(MT_PRXV_TX_MODE, v0);

		switch (mode) {
		case MT_PHY_TYPE_CCK:
			cck = true;
			fallthrough;
		case MT_PHY_TYPE_OFDM:
			i = mt76_get_rate(&dev->mt76, sband, i, cck);
			break;
		case MT_PHY_TYPE_HT_GF:
		case MT_PHY_TYPE_HT:
			status->encoding = RX_ENC_HT;
			if (i > 31)
				return -EINVAL;
			break;
		case MT_PHY_TYPE_VHT:
			status->nss =
				FIELD_GET(MT_PRXV_NSTS, v0) + 1;
			status->encoding = RX_ENC_VHT;
			if (i > 9)
				return -EINVAL;
			break;
		case MT_PHY_TYPE_HE_MU:
		case MT_PHY_TYPE_HE_SU:
		case MT_PHY_TYPE_HE_EXT_SU:
		case MT_PHY_TYPE_HE_TB:
			status->nss =
				FIELD_GET(MT_PRXV_NSTS, v0) + 1;
			status->encoding = RX_ENC_HE;
			i &= GENMASK(3, 0);

			if (gi <= NL80211_RATE_INFO_HE_GI_3_2)
				status->he_gi = gi;

			status->he_dcm = !!(idx & MT_PRXV_TX_DCM);
			break;
		default:
			return -EINVAL;
		}

		status->rate_idx = i;

		switch (FIELD_GET(MT_PRXV_FRAME_MODE, v0)) {
		case IEEE80211_STA_RX_BW_20:
			break;
		case IEEE80211_STA_RX_BW_40:
			if (mode & MT_PHY_TYPE_HE_EXT_SU &&
			    (idx & MT_PRXV_TX_ER_SU_106T)) {
				status->bw = RATE_INFO_BW_HE_RU;
				status->he_ru =
					NL80211_RATE_INFO_HE_RU_ALLOC_106;
			} else {
				status->bw = RATE_INFO_BW_40;
			}
			break;
		case IEEE80211_STA_RX_BW_80:
			status->bw = RATE_INFO_BW_80;
			break;
		case IEEE80211_STA_RX_BW_160:
			status->bw = RATE_INFO_BW_160;
			break;
		default:
			return -EINVAL;
		}

		status->enc_flags |= RX_ENC_FLAG_STBC_MASK * stbc;
		if (mode < MT_PHY_TYPE_HE_SU && gi)
			status->enc_flags |= RX_ENC_FLAG_SHORT_GI;

		if (rxd1 & MT_RXD1_NORMAL_GROUP_5) {
			rxd += 18;
		}
	}

	amsdu_info = FIELD_GET(MT_RXD4_NORMAL_PAYLOAD_FORMAT, rxd4);
	status->amsdu = !!amsdu_info;
	if (status->amsdu) {
		status->first_amsdu = amsdu_info == MT_RXD4_FIRST_AMSDU_FRAME;
		status->last_amsdu = amsdu_info == MT_RXD4_LAST_AMSDU_FRAME;
	}
#endif

	hdr_gap = num_rxd * sizeof(uint32_t) + 2 * remove_pad;
	m_adj(m, hdr_gap);
#if 0
	if (status->amsdu) {
		memmove(skb->data + 2, skb->data,
			ieee80211_get_hdrlen_from_skb(skb));
		skb_pull(skb, 2);
	}

	struct ieee80211_hdr *hdr;

	if (insert_ccmp_hdr) {
		u8 key_id = FIELD_GET(MT_RXD1_NORMAL_KEY_ID, rxd1);

		mt76_insert_ccmp_hdr(skb, key_id);
	}

	hdr = mt76_skb_get_hdr(skb);
	fc = hdr->frame_control;
	if (ieee80211_is_data_qos(fc)) {
		seq_ctrl = le16_to_cpu(hdr->seq_ctrl);
		qos_ctl = *ieee80211_get_qos_ctl(hdr);
	}

	if (!status->wcid || !ieee80211_is_data_qos(fc))
		return 0;

	status->aggr = unicast && !ieee80211_is_qos_nullfunc(fc);
	status->seqno = IEEE80211_SEQ_TO_SN(seq_ctrl);
	status->qos_ctl = qos_ctl;
#endif
	rxi->rxi_chan = chfnum;

	if (rxi->rxi_flags & IEEE80211_RXI_HWDEC) {
		struct ieee80211_frame *wh;
		u_int hdrlen;

		wh = mtod(m, struct ieee80211_frame *);
		hdrlen = ieee80211_get_hdrlen(wh);

		/*
		 * The firmware normally decrypts the frame and removes the
		 * CCMP/TKIP IV and MIC, leaving a plaintext payload.  Some
		 * firmware/AP combinations leave the 8-byte CCMP header in
		 * place instead.  Detect that from the ExtIV bit and, in that
		 * case, keep the Protected bit so ieee80211_input_hwdecrypt()
		 * strips the IV and validates the packet number.  A stripped
		 * frame starts with the LLC/SNAP header (aa aa 03 00 ...), so
		 * ExtIV is never set for one.
		 */
		if (mbuf_len(m) < hdrlen + IEEE80211_CCMP_HDRLEN ||
		    (((uint8_t *)wh)[hdrlen + 3] & IEEE80211_WEP_EXTIV) == 0)
			wh->i_fc[1] &= ~IEEE80211_FC1_PROTECTED;
	}

	return 0;
}

int
mt7925_mac_fill_rx(struct mwx_softc *sc, struct mbuf *m,
    struct ieee80211_rxinfo *rxi)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint8_t *base;
	uint32_t *rxd, *rxv;
	uint32_t rxd1, rxd2, rxd3;
	uint8_t chfreq, remove_pad;
	uint16_t hdr_gap;
	int8_t chain[4], signal;
	int hdr_trans, i, idx, unicast;

	if (mbuf_len(m) < 8 * sizeof(uint32_t))
		return -1;

	base = mtod(m, uint8_t *);
	rxd = (uint32_t *)base;
	rxd1 = le32toh(rxd[1]);
	rxd2 = le32toh(rxd[2]);
	rxd3 = le32toh(rxd[3]);

	if (rxd2 & MT_RXD2_NORMAL_AMSDU_ERR)
		return -1;

	hdr_trans = !!(rxd2 & MT7925_RXD2_NORMAL_HDR_TRANS);
	if (hdr_trans && (rxd1 & MT_RXD1_NORMAL_CM))
		return -1;

	/* ICV error or CCMP/BIP/WPI MIC error. */
	if (rxd1 & MT_RXD1_NORMAL_ICV_ERR) {
		ic->ic_stats.is_rx_decryptcrc++;
		return -1;
	}

	if (rxd3 & MT7925_RXD3_NORMAL_FCS_ERR)
		return -1;

	if (rxd1 & MT_RXD1_NORMAL_TKIP_MIC_ERR) {
		ic->ic_stats.is_rx_locmicfail++;
		ieee80211_michael_mic_failure(ic, 0 /* XXX */);
		return -1;
	}

	chfreq = (rxd3 & MT7925_RXD3_NORMAL_CH_FREQ_MASK) >>
	    MT7925_RXD3_NORMAL_CH_FREQ_SHIFT;
	unicast = ((rxd3 & MT7925_RXD3_NORMAL_ADDR_TYPE_MASK) >> 16) ==
	    MT7925_RXD3_NORMAL_U2M;
	idx = rxd1 & MT7925_RXD1_NORMAL_WLAN_IDX_MASK;
	(void)idx;
	(void)unicast;

	remove_pad = (rxd2 & MT7925_RXD2_NORMAL_HDR_OFFSET_MASK) >>
	    MT7925_RXD2_NORMAL_HDR_OFFSET_SHIFT;
	if (rxd2 & MT_RXD2_NORMAL_MAX_LEN_ERROR)
		return -1;

	rxd += 8;
	if ((size_t)((uint8_t *)rxd - base) > mbuf_len(m))
		return -1;

	if (rxd1 & MT7925_RXD1_NORMAL_GROUP_4) {
		rxd += 4;
		if ((size_t)((uint8_t *)rxd - base) > mbuf_len(m))
			return -1;
	}
	if (rxd1 & MT7925_RXD1_NORMAL_GROUP_1) {
		rxd += 4;
		if ((size_t)((uint8_t *)rxd - base) > mbuf_len(m))
			return -1;
	}
	if (rxd1 & MT7925_RXD1_NORMAL_GROUP_2) {
		rxd += 4;
		if ((size_t)((uint8_t *)rxd - base) > mbuf_len(m))
			return -1;
	}
	if (rxd1 & MT7925_RXD1_NORMAL_GROUP_3) {
		uint32_t v3;

		rxv = rxd;
		rxd += 4;
		if ((size_t)((uint8_t *)rxd - base) > mbuf_len(m))
			return -1;

		v3 = le32toh(rxv[3]);
		chain[0] = rcpi_to_rssi(MT_PRXV_RCPI0_SHIFT, v3);
		chain[1] = rcpi_to_rssi(MT_PRXV_RCPI1_SHIFT, v3);
		chain[2] = rcpi_to_rssi(MT_PRXV_RCPI2_SHIFT, v3);
		chain[3] = rcpi_to_rssi(MT_PRXV_RCPI3_SHIFT, v3);

		signal = -128;
		for (i = 0; i < sc->sc_capa.num_streams; i++) {
			if (!(sc->sc_capa.antenna_mask & (1U << i)))
				continue;
			if (chain[i] >= 0)
				continue;
			signal = MAX(signal, chain[i]);
		}
		rxi->rxi_rssi = signal;

		if (rxd1 & MT7925_RXD1_NORMAL_GROUP_5) {
			rxd += 24;
			if ((size_t)((uint8_t *)rxd - base) > mbuf_len(m))
				return -1;
		}
	}

	hdr_gap = (uint16_t)((uint8_t *)rxd - base) + 2 * remove_pad;
	if (hdr_gap > mbuf_len(m))
		return -1;

	/*
	 * Header-translated MT7925 frames still need a real reverse-translate
	 * path before they can be handed to net80211.
	 */
	if (hdr_trans)
		return -1;

	m_adj(m, hdr_gap);
	rxi->rxi_chan = chfreq;

	return 0;
}

uint32_t
mt7921_mac_tx_rate_val(struct mwx_softc *sc)
{
	int rateidx = 0, offset = 4;
	uint32_t rate, mode;

	/* XXX TODO basic_rates
	rateidx = ffs(vif->bss_conf.basic_rates) - 1;
	*/

	if (IEEE80211_IS_CHAN_2GHZ(sc->sc_ic.ic_bss->ni_chan))
		offset = 0;
	/* pick the lowest rate for hidden nodes */
	if (rateidx < 0)
		rateidx = 0;

	rateidx += offset;

	if (rateidx >= nitems(mt76_rates))
		rateidx = offset;

	rate =  mt76_rates[rateidx].hw_value;
	mode = (rate >> 8) << MT_TX_RATE_MODE_SHIFT;
	rate &= 0xff;

	return (rate & MT_TX_RATE_IDX_MASK) | (mode & MT_TX_RATE_MODE_MASK);
}

/*
 * True if the encapsulated frame carries an EAPOL payload.
 *
 * EAPOL must not be left to firmware rate adaptation.  Message 2 of the
 * 4-way handshake is one small unicast frame that the AP expects within a
 * few milliseconds, and rate control starts by probing upward; if the first
 * attempt lands on a rate this station cannot reach (busy 5 GHz channel,
 * narrow beam, far AP) the AP sees nothing, retransmits msg1 and finally
 * deauthenticates.  That is the observed pattern: the first association
 * attempt dies after the handshake timeout and the retry, starting from a
 * lower rate, succeeds.
 *
 * mac80211 avoids this by setting IEEE80211_TX_CTL_USE_MINRATE for
 * control-port protocol frames; mt76 then sets MT_TXD2_FIX_RATE, programs
 * the lowest basic rate and disables block-ack aggregation.  Apply the same
 * treatment here, on top of the existing rule for management, control and
 * group-addressed frames.
 */
static int
mwx_tx_is_eapol(struct mbuf *m, u_int hdrlen)
{
	const uint8_t *llc;

	if (mbuf_pkthdr_len(m) < hdrlen + LLC_SNAPFRAMELEN ||
	    mbuf_len(m) < hdrlen + LLC_SNAPFRAMELEN)
		return 0;

	llc = (const uint8_t *)mtod(m, const void *) + hdrlen;

	return llc[0] == 0xaa && llc[1] == 0xaa && llc[2] == 0x03 &&
	    llc[6] == (ETHERTYPE_PAE >> 8) &&
	    llc[7] == (ETHERTYPE_PAE & 0xff);
}

void
mt7921_mac_write_txwi_80211(struct mwx_softc *sc, struct mbuf *m,
    struct ieee80211_node *ni, struct mt76_txwi *txp)
{
	struct ieee80211_frame *wh;
	uint32_t val;
	uint8_t type, subtype, tid = 0;
	u_int hdrlen;
	int multicast, eapol;


	wh = mtod(m, struct ieee80211_frame *);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	type >>= IEEE80211_FC0_TYPE_SHIFT;
	subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
	subtype >>= IEEE80211_FC0_SUBTYPE_SHIFT;
	multicast = IEEE80211_IS_MULTICAST(wh->i_addr1);

	if (type == IEEE80211_FC0_TYPE_CTL)
		hdrlen = sizeof(struct ieee80211_frame_min);
	else
		hdrlen = ieee80211_get_hdrlen(wh);

	eapol = type == IEEE80211_FC0_TYPE_DATA && mwx_tx_is_eapol(m, hdrlen);

	/* Put QoS frames on the data queue which maps to their TID. */
	if (ieee80211_has_qos(wh)) {
		uint16_t qos = ieee80211_get_qos(wh);

		tid = qos & IEEE80211_QOS_TID;
	}

#ifdef NOTYET
	if (ieee80211_is_action(fc) &&
	    mgmt->u.action.category == WLAN_CATEGORY_BACK &&
	    mgmt->u.action.u.addba_req.action_code == WLAN_ACTION_ADDBA_REQ) {
		u16 capab = le16_to_cpu(mgmt->u.action.u.addba_req.capab);

		txp->txwi[5] |= htole32(MT_TXD5_ADD_BA);
		tid = (capab >> 2) & IEEE80211_QOS_CTL_TID_MASK;
	} else if (ieee80211_is_back_req(hdr->frame_control)) {
		struct ieee80211_bar *bar = (struct ieee80211_bar *)hdr;
		u16 control = le16_to_cpu(bar->control);

		tid = FIELD_GET(IEEE80211_BAR_CTRL_TID_INFO_MASK, control);
	}
#endif

	val = MT_HDR_FORMAT_802_11 | MT_TXD1_HDR_INFO(hdrlen / 2) |
	    MT_TXD1_TID(tid);
	txp->txwi[1] |= htole32(val);

	val = MT_TXD2_FRAME_TYPE(type) | MT_TXD2_SUB_TYPE(subtype);
	if (multicast)
		val |= MT_TXD2_MULTICAST;

#ifdef NOTYET
	if (key && multicast && ieee80211_is_robust_mgmt_frame(skb) &&
	    key->cipher == WLAN_CIPHER_SUITE_AES_CMAC) {
		val |= MT_TXD2_BIP;
		txp->txwi[3] &= ~htole32(MT_TXD3_PROTECT_FRAME);
	}
#endif

	if (multicast || type != IEEE80211_FC0_TYPE_DATA || eapol) {
		/* Fixed rata is available just for 802.11 txd */
		uint32_t rate, val6;

		val |= MT_TXD2_FIX_RATE;
		/* hardware won't add HTC for mgmt/ctrl frame */
		val |= htole32(MT_TXD2_HTC_VLD);

		rate = mt7921_mac_tx_rate_val(sc);

		val6 = MT_TXD6_FIXED_BW;
		val6 |= (rate << MT_TXD6_TX_RATE_SHIFT) & MT_TXD6_TX_RATE_MASK;
		txp->txwi[6] |= htole32(val6);
		txp->txwi[3] |= htole32(MT_TXD3_BA_DISABLE);
	}

	txp->txwi[2] |= htole32(val);

#ifdef NOTYET
	if (ieee80211_is_beacon(fc)) {
		txp->txwi[3] &= ~htole32(MT_TXD3_SW_POWER_MGMT);
		txp->txwi[3] |= htole32(MT_TXD3_REM_TX_COUNT);
	}

	if (info->flags & IEEE80211_TX_CTL_INJECTED) {
		u16 seqno = le16_to_cpu(hdr->seq_ctrl);

		if (ieee80211_is_back_req(hdr->frame_control)) {
			struct ieee80211_bar *bar;

			bar = (struct ieee80211_bar *)skb->data;
			seqno = le16_to_cpu(bar->start_seq_num);
		}

		val = MT_TXD3_SN_VALID |
			FIELD_PREP(MT_TXD3_SEQ, IEEE80211_SEQ_TO_SN(seqno));
		txp->txwi[3] |= htole32(val);
		txp->txwi[7] &= ~htole32(MT_TXD7_HW_AMSDU);
	}
#endif

	val = MT_TXD7_TYPE(type) | MT_TXD7_SUB_TYPE(subtype);
	txp->txwi[7] |= htole32(val);

#if NBPFILTER > 0
	if (__predict_false(sc->sc_drvbpf != NULL)) {
		struct ieee80211com *ic = &sc->sc_ic;
		struct mwx_tx_radiotap_header *tap = &sc->sc_txtap;
		uint16_t chan_flags;

		tap->wt_flags = 0;
		tap->wt_chan_freq = htole16(ni->ni_chan->ic_freq);
		chan_flags = ni->ni_chan->ic_flags;
		if (ic->ic_curmode != IEEE80211_MODE_11N &&
			ic->ic_curmode != IEEE80211_MODE_11AC) {
			chan_flags &= ~IEEE80211_CHAN_HT;
			chan_flags &= ~IEEE80211_CHAN_40MHZ;
		}
		if (ic->ic_curmode != IEEE80211_MODE_11AC)
			chan_flags &= ~IEEE80211_CHAN_VHT;
		tap->wt_chan_flags = htole16(chan_flags);
#ifdef NOTYET
		if ((ni->ni_flags & IEEE80211_NODE_HT) &&
		    !IEEE80211_IS_MULTICAST(wh->i_addr1) &&
		    type == IEEE80211_FC0_TYPE_DATA &&
		    rinfo->ht_plcp != IWX_RATE_HT_SISO_MCS_INV_PLCP) {
			tap->wt_rate = (0x80 | rinfo->ht_plcp);
		} else
			tap->wt_rate = rinfo->rate;
#endif
		tap->wt_rate = 2;
		if ((ic->ic_flags & IEEE80211_F_WEPON) &&
		    (wh->i_fc[1] & IEEE80211_FC1_PROTECTED))
			tap->wt_flags |= IEEE80211_RADIOTAP_F_WEP;

		bpf_mtap_hdr(sc->sc_drvbpf, tap, sc->sc_txtap_len,
		    m, BPF_DIRECTION_OUT);
	}
#endif

}

static inline uint8_t
mt7921_lmac_mapping(uint8_t ac)
{
	/* LMAC uses the reverse order of mac80211 AC indexes */
	return 3 - ac;
}

void
mt7921_mac_write_txwi(struct mwx_softc *sc, struct mbuf *m,
    struct ieee80211_node *ni, struct mt76_txwi *txp)
{
	struct mwx_node *mn = (struct mwx_node *)ni;
	struct ieee80211_frame *wh;
	uint32_t val, p_fmt, omac_idx;
	uint8_t q_idx, wmm_idx, band_idx;
	uint8_t phy_idx = 0;
	uint16_t wcid;
	/* XXX hardcoded and wrong */
	int pid = MT_PACKET_ID_FIRST;
	enum mt76_txq_id qid = MT_TXQ_BE;

	wh = mtod(m, struct ieee80211_frame *);
	omac_idx = sc->sc_vif.omac_idx << MT_TXD1_OWN_MAC_SHIFT;
	wmm_idx = sc->sc_vif.wmm_idx;
	band_idx = sc->sc_vif.band_idx;

	/*
	 * Every frame is addressed to the AP, including Ethernet
	 * broadcasts: in a ToDS frame the destination is addr3, while
	 * addr1 - the address the hardware keys off - is always the BSSID.
	 * IEEE 802.11 therefore protects a group-addressed frame sent by a
	 * station with the pairwise key (the group key protects traffic the
	 * AP sends us), which is exactly what mac80211 does when it selects
	 * the AP's PTK for such a frame.  So the transit through the AP's
	 * WTBL entry is right for unicast and multicast alike.
	 */
	wcid = mn->wcid;

	if (qid >= MT_TXQ_PSD) {
		p_fmt = MT_TX_TYPE_CT;
		q_idx = MT_LMAC_ALTX0;
	} else {
		p_fmt = MT_TX_TYPE_CT;
		q_idx = wmm_idx * MWX_MAX_WMM_SETS +
		    mt7921_lmac_mapping(/* skb_get_queue_mapping(skb) */ 0);

#ifdef NOTYET
		/* counting non-offloading skbs */
		wcid->stats.tx_bytes += skb->len;
		wcid->stats.tx_packets++;
#endif
	}

	val = ((mbuf_pkthdr_len(m) + MT_TXD_SIZE) & MT_TXD0_TX_BYTES_MASK) |
		p_fmt | MT_TXD0_Q_IDX(q_idx);
	txp->txwi[0] = htole32(val);

	val = MT_TXD1_LONG_FORMAT | (wcid & MT_TXD1_WLAN_IDX_MASK) |
	    (omac_idx & MT_TXD1_OWN_MAC_MASK);
	if (phy_idx || band_idx)
		val |= MT_TXD1_TGID;
	txp->txwi[1] = htole32(val);
	txp->txwi[2] = 0;

	val = 15 << MT_TXD3_REM_TX_COUNT_SHIFT;
	if (wh->i_fc[1] & IEEE80211_FC1_PROTECTED)
		val |= MT_TXD3_PROTECT_FRAME;
#ifdef NOTYET
	if (info->flags & IEEE80211_TX_CTL_NO_ACK)
		val |= MT_TXD3_NO_ACK;
#endif
	txp->txwi[3] = htole32(val);
	txp->txwi[4] = 0;

	val = pid & MT_TXD5_PID;
	if (pid >= MT_PACKET_ID_FIRST)
		val |= MT_TXD5_TX_STATUS_HOST;
	txp->txwi[5] = htole32(val);
	txp->txwi[6] = 0;
	txp->txwi[7] = /* XXX wcid->amsdu ? htole32(MT_TXD7_HW_AMSDU) : */ 0;

#ifdef NOTYET
	if (is_8023)
		mt76_connac2_mac_write_txwi_8023(txp, m, wcid);
	else
#endif
		mt7921_mac_write_txwi_80211(sc, m, ni, txp);
}

static uint8_t
mt7925_mac_basic_rates_idx(struct ieee80211_node *ni)
{
	if (IEEE80211_IS_CHAN_2GHZ(ni->ni_chan))
		return 11;
	return 15;
}

void
mt7925_mac_write_txwi_80211(struct mwx_softc *sc, struct mbuf *m,
    struct ieee80211_node *ni, struct mt76_txwi *txp)
{
	struct ieee80211_frame *wh;
	uint32_t val;
	uint8_t type, subtype, tid = 0;
	u_int hdrlen;
	int multicast, eapol;

	wh = mtod(m, struct ieee80211_frame *);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	type >>= IEEE80211_FC0_TYPE_SHIFT;
	subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
	subtype >>= IEEE80211_FC0_SUBTYPE_SHIFT;
	multicast = IEEE80211_IS_MULTICAST(wh->i_addr1);

	if (type == IEEE80211_FC0_TYPE_CTL)
		hdrlen = sizeof(struct ieee80211_frame_min);
	else
		hdrlen = ieee80211_get_hdrlen(wh);

	eapol = type == IEEE80211_FC0_TYPE_DATA && mwx_tx_is_eapol(m, hdrlen);

	if (ieee80211_has_qos(wh)) {
		uint16_t qos = ieee80211_get_qos(wh);

		tid = qos & IEEE80211_QOS_TID;
	} else if (type == IEEE80211_FC0_TYPE_MGT) {
		tid = 0;
	}

	val = MT7925_HDR_FORMAT_802_11 | MT7925_TXD1_HDR_INFO(hdrlen / 2) |
	    MT7925_TXD1_TID(tid);
	if (type != IEEE80211_FC0_TYPE_DATA || multicast || eapol)
		val |= MT7925_TXD1_FIXED_RATE;
	txp->txwi[1] |= htole32(val);

	if (multicast)
		val |= MT_TXD2_MULTICAST;

	val = MT_TXD2_FRAME_TYPE(type) | MT_TXD2_SUB_TYPE(subtype);
	txp->txwi[2] |= htole32(val);

	if (multicast)
		txp->txwi[3] |= htole32(MT7925_TXD3_BCM);

	val = MT7925_TXD6_DAS | MT7925_TXD6_MSDU_CNT(1) |
	    MT7925_TXD6_DIS_MAT;

	if (letoh32(txp->txwi[1]) & MT7925_TXD1_FIXED_RATE) {
		val |= MT7925_TXD6_TX_RATE(mt7925_mac_basic_rates_idx(ni));
		txp->txwi[3] |= htole32(MT_TXD3_BA_DISABLE);
	}
	txp->txwi[6] = htole32(val);
	txp->txwi[7] = 0;

#if NBPFILTER > 0
	if (__predict_false(sc->sc_drvbpf != NULL)) {
		struct ieee80211com *ic = &sc->sc_ic;
		struct mwx_tx_radiotap_header *tap = &sc->sc_txtap;
		uint16_t chan_flags;

		tap->wt_flags = 0;
		tap->wt_chan_freq = htole16(ni->ni_chan->ic_freq);
		chan_flags = ni->ni_chan->ic_flags;
		if (ic->ic_curmode != IEEE80211_MODE_11N &&
			ic->ic_curmode != IEEE80211_MODE_11AC) {
			chan_flags &= ~IEEE80211_CHAN_HT;
			chan_flags &= ~IEEE80211_CHAN_40MHZ;
		}
		if (ic->ic_curmode != IEEE80211_MODE_11AC)
			chan_flags &= ~IEEE80211_CHAN_VHT;
		tap->wt_chan_flags = htole16(chan_flags);
#ifdef NOTYET
		if ((ni->ni_flags & IEEE80211_NODE_HT) &&
		    !IEEE80211_IS_MULTICAST(wh->i_addr1) &&
		    type == IEEE80211_FC0_TYPE_DATA &&
		    rinfo->ht_plcp != IWX_RATE_HT_SISO_MCS_INV_PLCP) {
			tap->wt_rate = (0x80 | rinfo->ht_plcp);
		} else
			tap->wt_rate = rinfo->rate;
#endif
		tap->wt_rate = 2;
		if ((ic->ic_flags & IEEE80211_F_WEPON) &&
		    (wh->i_fc[1] & IEEE80211_FC1_PROTECTED))
			tap->wt_flags |= IEEE80211_RADIOTAP_F_WEP;

		bpf_mtap_hdr(sc->sc_drvbpf, tap, sc->sc_txtap_len,
		    m, BPF_DIRECTION_OUT);
	}
#endif
}

void
mt7925_mac_write_txwi(struct mwx_softc *sc, struct mbuf *m,
    struct ieee80211_node *ni, struct mt76_txwi *txp)
{
	struct mwx_node *mn = (struct mwx_node *)ni;
	struct ieee80211_frame *wh;
	uint8_t p_fmt, q_idx, omac_idx, wmm_idx, band_idx;
	uint32_t val;
	uint8_t type;

	omac_idx = sc->sc_vif.omac_idx;
	wmm_idx = sc->sc_vif.wmm_idx;
	band_idx = sc->sc_vif.band_idx;
	wh = mtod(m, struct ieee80211_frame *);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;

	p_fmt = MT_TX_TYPE_CT;
	if (type != IEEE80211_FC0_TYPE_DATA)
		q_idx = MT_LMAC_ALTX0;
	else
		q_idx = wmm_idx * MWX_MAX_WMM_SETS + mt7921_lmac_mapping(0);

	val = ((mbuf_pkthdr_len(m) + MT_TXD_SIZE) & MT_TXD0_TX_BYTES_MASK) |
	    p_fmt | MT_TXD0_Q_IDX(q_idx);
	txp->txwi[0] = htole32(val);

	val = MT7925_TXD1_WLAN_IDX(mn->wcid) |
	    MT7925_TXD1_OWN_MAC(omac_idx);
	if (band_idx)
		val |= MT7925_TXD1_TGID(band_idx);
	txp->txwi[1] = htole32(val);
	txp->txwi[2] = 0;

	val = 15 << MT_TXD3_REM_TX_COUNT_SHIFT;
	txp->txwi[3] = htole32(val);
	txp->txwi[4] = 0;

	val = MT_PACKET_ID_FIRST & MT_TXD5_PID;
	val |= MT_TXD5_TX_STATUS_HOST;
	txp->txwi[3] |= htole32(MT_TXD3_BA_DISABLE);
	txp->txwi[5] = htole32(val);
	txp->txwi[6] = 0;
	txp->txwi[7] = 0;

	mt7925_mac_write_txwi_80211(sc, m, ni, txp);
}

void
mt7921_mac_tx_free(struct mwx_softc *sc, struct mbuf *m)
{
	struct mwx_txwi *mt;
	uint32_t *txfree;
	uint32_t  txval;
	int count, i;

	/* first cleanup the TX dma rings */
	mwx_dma_tx_cleanup(sc, &sc->sc_txq);

	if ((m = m_pullup(m, mbuf_pkthdr_len(m))) == NULL)
		return;

	txfree = mtod(m, uint32_t *);
	txval = le32toh(txfree[0]);
	m_adj(m, 2 * sizeof(txval));

	count = MT_TX_FREE0_MSDU_CNT_GET(txval);

	if (count * sizeof(txval) > mbuf_len(m))
		goto out;

	txfree = mtod(m, uint32_t *);
	for (i = 0; i < count; i++) {
		uint16_t msdu;

		txval = le32toh(txfree[i]);
		if (txval & MT_TX_FREE_PAIR) {
			count++;
			if (count * sizeof(txval) > mbuf_len(m))
				goto out;
			/* TODO any wcid fumbling */
			/* wcid = MT_TX_FREE_WLAN_ID_GET(txval); */
			continue;
		}

#if NOTYET
		if (wcid != NULL) {
			status = !!(txval & MT_TX_FREE_STATUS_MASK);
			retries = txval & MT_TX_FREE_COUNT_MASK;
		}
#endif

		msdu = MT_TX_FREE_MSDU_ID_GET(txval);
		if (msdu >= sc->sc_txwi.mt_count)
			continue;
		mt = &sc->sc_txwi.mt_data[msdu];
		mwx_txwi_put(sc, mt);
	}

 out:
	m_freem(m);
}

void
mt7925_mac_tx_free(struct mwx_softc *sc, struct mbuf *m)
{
	struct mwx_txwi *mt;
	uint32_t *txfree, *txend;
	uint32_t  txval;
	int total, count, i;

	/* first cleanup the TX dma rings */
	mwx_dma_tx_cleanup(sc, &sc->sc_txq);

	if ((m = m_pullup(m, mbuf_pkthdr_len(m))) == NULL)
		return;

	txfree = mtod(m, uint32_t *);
	txval = le32toh(txfree[0]);

	if (MT_TX_FREE1_VER_GET(le32toh(txfree[1])) < 4) {
		printf("%s: %s: bad version in txfree1 %08x\n",
		    DEVNAME(sc), __func__, le32toh(txfree[1]));
		goto out;
	}

	m_adj(m, 2 * sizeof(txval));

	total = MT_TX_FREE0_MSDU_CNT_GET(txval);

	txfree = mtod(m, uint32_t *);
	txend = txfree + mbuf_len(m) / sizeof(txval);
	count = 0;
	while (txfree < txend) {
		uint16_t msdu;

		if (count >= total)
			break;

		txval = le32toh(*txfree++);
		if (txval & MT7925_TX_FREE_INFO_PAIR) {
			continue;
		}
		if (txval & MT7925_TX_FREE_INFO_HEADER) {
			continue;
		}

		for (i = 0; i < 2; i++) {
			msdu = (txval >> (15 * i)) &
			    MT7925_TX_FREE_INFO_MSDU_ID;
			if (msdu == MT7925_TX_FREE_INFO_MSDU_ID)
				continue;
			count++;
			if (msdu >= sc->sc_txwi.mt_count)
				continue;
			mt = &sc->sc_txwi.mt_data[msdu];
			mwx_txwi_put(sc, mt);
		}
	}

 out:
	m_freem(m);
}

int
mt7921_set_channel(struct mwx_softc *sc)
{
	int rv;

	/* stop queues, block other configs (MT76_RESET) */
	// XXX NOTYET mt76_set_channel(sc);

	rv = mt7921_mcu_set_chan_info(sc, MCU_EXT_CMD_CHANNEL_SWITCH);
	if (rv)
		return rv;
	mt7921_mac_set_timing(sc);
	mwx_mac_reset_counters(sc);

	/* restart queues */
	return 0;
}

int
mt7925_set_channel(struct mwx_softc *sc)
{
	struct ieee80211_node *ni = sc->sc_ic.ic_bss;
	int rv;

	if (ni == NULL || ni->ni_chan == NULL)
		return EINVAL;

	rv = mt7925_mcu_set_chctx(sc, ni, ni->ni_chan);
	if (rv)
		return rv;
	mt7921_mac_set_timing(sc);
	return mt7925_mcu_add_bss_info(sc, ni, 1);
}

int
mt7925_mcu_add_bss_info(struct mwx_softc *sc, struct ieee80211_node *ni,
    int enable)
{
	struct mwx_vif *mvif = &sc->sc_vif;
	struct mwx_node *mn = (struct mwx_node *)ni;
	struct mwx_chanctx ctx;
	uint8_t hw_bss_idx;
	uint8_t basic_rate_idx;
	struct {
		struct {
			uint8_t		bss_idx;
			uint8_t		pad[3];
		} __packed hdr;
		struct mt76_connac_bss_basic_tlv basic;
		struct {
			uint16_t	tag;
			uint16_t	len;
			uint8_t		mode;
			uint8_t		status;
			uint8_t		cipher;
			uint8_t		pad;
		} __packed sec;
		struct {
			uint16_t	tag;
			uint16_t	len;
			uint8_t		pad1[2];
			uint16_t	basic_rate;
			uint16_t	bc_trans;
			uint16_t	mc_trans;
			uint8_t		short_preamble;
			uint8_t		bc_fixed_rate;
			uint8_t		mc_fixed_rate;
			uint8_t		pad2;
		} __packed rate;
		struct {
			uint16_t	tag;
			uint16_t	len;
			uint8_t		qos;
			uint8_t		pad[3];
		} __packed qos;
		struct {
			uint16_t	tag;
			uint16_t	len;
			uint8_t		slot_valid;
			uint8_t		sifs_valid;
			uint8_t		rifs_valid;
			uint8_t		eifs_valid;
			uint16_t	slot_time;
			uint16_t	sifs_time;
			uint16_t	rifs_time;
			uint16_t	eifs_time;
			uint8_t		eifs_cck_valid;
			uint8_t		pad;
			uint16_t	eifs_cck_time;
		} __packed ifs;
	} __packed req = {
		.hdr = {
			.bss_idx = mvif->idx,
		},
		.basic = {
			.tag = htole16(UNI_BSS_INFO_BASIC),
			.len = htole16(sizeof(req.basic)),
			.active = 1,
			.omac_idx = mvif->omac_idx,
			.band_idx = mvif->band_idx,
			.wmm_idx = mvif->wmm_idx,
			.link_idx = mvif->idx,
			.conn_type = htole32(STA_TYPE_STA | NETWORK_INFRA),
			.bmc_tx_wlan_idx = htole16(mvif->vif_mn.wcid),
			.sta_idx = htole16(mn ? mn->wcid : 0),
		},
		.sec = {
			.tag = htole16(UNI_BSS_INFO_SEC),
			.len = htole16(sizeof(req.sec)),
			.mode = 0,
			.status = 1,
		},
		.rate = {
			.tag = htole16(UNI_BSS_INFO_RATE),
			.len = htole16(sizeof(req.rate)),
		},
		.qos = {
			.tag = htole16(UNI_BSS_INFO_QBSS),
			.len = htole16(sizeof(req.qos)),
		},
		.ifs = {
			.tag = htole16(UNI_BSS_INFO_IFS_TIME),
			.len = htole16(sizeof(req.ifs)),
			.slot_valid = 1,
			.slot_time = htole16(IEEE80211_DUR_DS_SHSLOT),
		},
	};
	struct {
		struct {
			uint8_t		bss_idx;
			uint8_t		pad[3];
		} __packed hdr;
		struct mt76_connac_bss_basic_tlv basic;
		struct {
			uint16_t	tag;
			uint16_t	len;
			uint8_t		mode;
			uint8_t		status;
			uint8_t		cipher;
			uint8_t		pad;
		} __packed sec;
		struct {
			uint16_t	tag;
			uint16_t	len;
			uint8_t		pad1[2];
			uint16_t	basic_rate;
			uint16_t	bc_trans;
			uint16_t	mc_trans;
			uint8_t		short_preamble;
			uint8_t		bc_fixed_rate;
			uint8_t		mc_fixed_rate;
			uint8_t		pad2;
		} __packed rate;
		struct {
			uint16_t	tag;
			uint16_t	len;
			uint8_t		qos;
			uint8_t		pad[3];
		} __packed qos;
		struct {
			uint16_t	tag;
			uint16_t	len;
			uint8_t		control_channel;
			uint8_t		center_chan;
			uint8_t		center_chan2;
			uint8_t		bw;
			uint8_t		tx_streams;
			uint8_t		rx_streams;
			uint8_t		short_st;
			uint8_t		ht_op_info;
			uint8_t		sco;
			uint8_t		band;
			uint8_t		pad[2];
		} __packed rlm;
		struct {
			uint16_t	tag;
			uint16_t	len;
			uint8_t		slot_valid;
			uint8_t		sifs_valid;
			uint8_t		rifs_valid;
			uint8_t		eifs_valid;
			uint16_t	slot_time;
			uint16_t	sifs_time;
			uint16_t	rifs_time;
			uint16_t	eifs_time;
			uint8_t		eifs_cck_valid;
			uint8_t		pad;
			uint16_t	eifs_cck_time;
		} __packed ifs;
	} __packed req_rlm = {
		.hdr = {
			.bss_idx = mvif->idx,
		},
		.basic = {
			.tag = htole16(UNI_BSS_INFO_BASIC),
			.len = htole16(sizeof(req_rlm.basic)),
			.active = 1,
			.omac_idx = mvif->omac_idx,
			.band_idx = mvif->band_idx,
			.wmm_idx = mvif->wmm_idx,
			.link_idx = mvif->idx,
			.conn_type = htole32(STA_TYPE_STA | NETWORK_INFRA),
				.bmc_tx_wlan_idx = htole16(mvif->vif_mn.wcid),
				.sta_idx = htole16(mn ? mn->wcid : 0),
			},
			.sec = {
				.tag = htole16(UNI_BSS_INFO_SEC),
				.len = htole16(sizeof(req_rlm.sec)),
				.mode = 0,
				.status = 1,
			},
		.rate = {
			.tag = htole16(UNI_BSS_INFO_RATE),
			.len = htole16(sizeof(req_rlm.rate)),
		},
		.qos = {
			.tag = htole16(UNI_BSS_INFO_QBSS),
			.len = htole16(sizeof(req_rlm.qos)),
		},
		.rlm = {
			.tag = htole16(UNI_BSS_INFO_RLM),
			.len = htole16(sizeof(req_rlm.rlm)),
			.tx_streams = 1,
			.rx_streams = 1,
			.short_st = 1,
			.band = 1,
			.bw = CMD_CBW_20MHZ,
		},
		.ifs = {
			.tag = htole16(UNI_BSS_INFO_IFS_TIME),
			.len = htole16(sizeof(req_rlm.ifs)),
			.slot_valid = 1,
			.slot_time = htole16(IEEE80211_DUR_DS_SHSLOT),
		},
	};

	if (ni == NULL || ni->ni_chan == NULL)
		return EINVAL;

	hw_bss_idx = mvif->omac_idx > EXT_BSSID_START ? HW_BSSID_0 :
	    mvif->omac_idx;
	req.basic.hw_bss_idx = hw_bss_idx;
	req.basic.conn_state = !enable;
	req.basic.bcn_interval = htole16(ni->ni_intval);
	req.basic.dtim_period = ni->ni_dtimperiod ? ni->ni_dtimperiod : 1;
	req.basic.phymode = mt7921_get_phy_mode_v2(sc, ni);
	memcpy(req.basic.bssid, ni->ni_bssid, sizeof(req.basic.bssid));
	mwx_get_chanctx(sc, ni, ni->ni_chan, &ctx);
	req_rlm.rlm.control_channel = ctx.control_ch;
	req_rlm.rlm.center_chan = ctx.center_ch;
	req_rlm.rlm.bw = ctx.bw;
	req_rlm.rlm.ht_op_info = ctx.ht_op_info;
	req_rlm.rlm.sco = ctx.sco;
	req_rlm.rlm.band = ctx.rlm_band;
	req_rlm.rlm.tx_streams = sc->sc_capa.num_streams ?
	    sc->sc_capa.num_streams : 1;
	req_rlm.rlm.rx_streams = sc->sc_capa.num_streams ?
	    sc->sc_capa.num_streams : 1;

	if (IEEE80211_IS_CHAN_2GHZ(ni->ni_chan)) {
		req.basic.nonht_basic_phy = htole16(1);
		req.rate.basic_rate = htole16(0x000f);
		req.rate.short_preamble = 1;
		basic_rate_idx = 11;
	} else {
		req.basic.nonht_basic_phy = htole16(3);
		req.rate.basic_rate = htole16((1 << 6) | (1 << 8) | (1 << 10));
		basic_rate_idx = 15;
	}

	req.rate.bc_fixed_rate = basic_rate_idx;
	req.rate.mc_fixed_rate = basic_rate_idx;
	req.qos.qos = (ni->ni_flags & IEEE80211_NODE_QOS) != 0;
	req_rlm.basic = req.basic;
	req_rlm.sec.mode = req.sec.mode;
	req_rlm.sec.status = req.sec.status;
	req_rlm.sec.cipher = req.sec.cipher;
	req_rlm.rate.basic_rate = req.rate.basic_rate;
	req_rlm.rate.bc_trans = req.rate.bc_trans;
	req_rlm.rate.mc_trans = req.rate.mc_trans;
	req_rlm.rate.short_preamble = req.rate.short_preamble;
	req_rlm.rate.bc_fixed_rate = req.rate.bc_fixed_rate;
	req_rlm.rate.mc_fixed_rate = req.rate.mc_fixed_rate;
	req_rlm.qos.qos = req.qos.qos;
	req_rlm.ifs.slot_valid = req.ifs.slot_valid;
	req_rlm.ifs.sifs_valid = req.ifs.sifs_valid;
	req_rlm.ifs.rifs_valid = req.ifs.rifs_valid;
	req_rlm.ifs.eifs_valid = req.ifs.eifs_valid;
	req_rlm.ifs.slot_time = req.ifs.slot_time;
	req_rlm.ifs.sifs_time = req.ifs.sifs_time;
	req_rlm.ifs.rifs_time = req.ifs.rifs_time;
	req_rlm.ifs.eifs_time = req.ifs.eifs_time;
	req_rlm.ifs.eifs_cck_valid = req.ifs.eifs_cck_valid;
	req_rlm.ifs.eifs_cck_time = req.ifs.eifs_cck_time;

	if (DEVDEBUG(sc))
		printf("%s: add bss info bss %u bssid %s chan %u "
		    "esslen %u enable %d\n", DEVNAME(sc), req.hdr.bss_idx,
		    ether_sprintf(ni->ni_bssid),
		    ieee80211_mhz2ieee(ni->ni_chan->ic_freq, ni->ni_chan->ic_flags),
		    ni->ni_esslen, enable);

	if (enable)
		return mwx_mcu_send_wait(sc, MCU_UNI_CMD_BSS_INFO_UPDATE,
		    &req_rlm, sizeof(req_rlm));
	return mwx_mcu_send_wait(sc, MCU_UNI_CMD_BSS_INFO_UPDATE, &req,
	    sizeof(req));
}


uint8_t
mt7921_get_phy_mode_v2(struct mwx_softc *sc, struct ieee80211_node *ni)
{
	uint8_t mode = 0;

	if (ni == NULL)
		ni = sc->sc_ic.ic_bss;

	if (IEEE80211_IS_CHAN_2GHZ(ni->ni_chan)) {
		mode |= PHY_TYPE_BIT_HR_DSSS | PHY_TYPE_BIT_ERP;
		if (ieee80211_node_supports_ht(ni))
			mode |= PHY_TYPE_BIT_HT;
#ifdef NOTYET
		if (ieee80211_node_supports_he(ni))
			mode |= PHY_TYPE_BIT_HE;
#endif
	} else if (IEEE80211_IS_CHAN_5GHZ(ni->ni_chan) /* || CHAN_6GHZ */) {
		mode |= PHY_TYPE_BIT_OFDM;
		if (ieee80211_node_supports_ht(ni))
			mode |= PHY_TYPE_BIT_HT;
		if (ieee80211_node_supports_vht(ni))
			mode |= PHY_TYPE_BIT_VHT;
#ifdef NOTYET
		if (ieee80211_node_supports_he(ni))
			mode |= PHY_TYPE_BIT_HE;
#endif
	}
	return mode;
}

/*
 * Allocate an mbuf and reserve len bytes at the start for the header.
 * After that use mwx_append_tlv or mwx_append_len to add sub-tlv and
 * mwx_fill_sta_req_hdr to finalize the hdr.
 */
struct mbuf *
mwx_alloc_sta_req_tlv(int len)
{
	struct mbuf *m;

	/* Allocate mbuf cluster with enough space */
	m = m_clget(NULL, M_DONTWAIT, MCLBYTES);
	if (m == NULL)
		return NULL;

	/* align to have space for the mcu header */
	mbuf_setdata(m, (caddr_t)mbuf_data(m) + sizeof(struct mwx_mcu_txd) + len, 0);
	mbuf_setlen(m, 0);
	mbuf_pkthdr_setlen(m, 0);

	return m;
}

void
mwx_fill_sta_req_hdr(struct mbuf *m, struct mwx_vif *mvif,
    uint8_t muar_idx, uint16_t wcid, uint16_t tlvnum)
{
	struct sta_req_hdr *hdr;

	KASSERT(m_leadingspace(m) >= sizeof(*hdr));
	m = m_prepend(m, sizeof(*hdr), M_DONTWAIT);
	hdr = mtod(m, struct sta_req_hdr *);
	memset(hdr, 0, sizeof(*hdr));

	hdr->bss_idx = mvif->idx;
	hdr->wlan_idx_lo = wcid & 0xff;
	hdr->wlan_idx_hi = wcid >> 8;
	hdr->muar_idx = muar_idx;
	hdr->is_tlv_append = 1;
	hdr->tlv_num = htole16(tlvnum);
}

caddr_t
mwx_append_len(struct mbuf *m, int len)
{
	caddr_t p;

	KASSERT(m_trailingspace(m) >= len);

	p = mtod(m, caddr_t) + mbuf_len(m);
	mbuf_setlen(m, mbuf_len(m) + len);
	mbuf_pkthdr_setlen(m, mbuf_len(m));
	memset(p, 0, len);

	return p;
}

/*
 * Reserve len bytes at the end of mbuf m, return to start of that area
 * after initializing the data. It also sets the tag and len hdr.
 */
void *
mwx_append_tlv(struct mbuf *m, uint16_t *tlvnum, int tag, int len)
{
	struct {
		uint16_t	tag;
		uint16_t	len;
	} tlv = {
		.tag = htole16(tag),
		.len = htole16(len),
	};
	caddr_t p;

	p = mwx_append_len(m, len);
	memcpy(p, &tlv, sizeof(tlv));

	*tlvnum += 1;
	return p;
}

/*
 * Value for STA_REC_STATE.
 *
 * The firmware distinguishes a peer that merely exists (the AP while the
 * station is still authenticating, so the association ID is 0) from one
 * that is associated.  net80211 records the AID when association
 * completes, so the node itself is the authority here and there is no
 * second copy that could fall out of sync with it.
 */
static uint8_t
mwx_sta_rec_state(struct ieee80211_node *ni)
{
	if (ni != NULL && ni->ni_associd != 0)
		return MT76_STA_INFO_STATE_ASSOC;
	return MT76_STA_INFO_STATE_NONE;
}

void
mt7921_mcu_add_basic_tlv(struct mbuf *m, uint16_t *tlvnum, struct mwx_softc *sc,
    struct ieee80211_node *ni, int add, int newsta)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct sta_rec_basic *basic;

	basic = (struct sta_rec_basic *)mwx_append_tlv(m, tlvnum, STA_REC_BASIC, sizeof(*basic));

	basic->extra_info = htole16(EXTRA_INFO_VER);
	if (add) {
		if (newsta)
			basic->extra_info |= htole16(EXTRA_INFO_NEW);
		basic->conn_state = CONN_STATE_PORT_SECURE;
	} else {
		basic->conn_state = CONN_STATE_DISCONNECT;
	}

	if (ni == NULL) {
		basic->conn_type = htole32(STA_TYPE_BC | NETWORK_INFRA);
		memset(basic->peer_addr, 0xff, sizeof(basic->peer_addr));
		return;
	}

	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		basic->conn_type = htole32(STA_TYPE_AP | NETWORK_INFRA);
		break;
	case IEEE80211_M_MONITOR:
		panic("mt7921_mcu_sta_basic_tlv unexpected operation mode");
#ifndef IEEE80211_STA_ONLY
	case IEEE80211_M_HOSTAP:
		basic->conn_type = htole32(STA_TYPE_STA | NETWORK_INFRA);
		break;
	case IEEE80211_M_IBSS:
	case IEEE80211_M_AHDEMO:
		basic->conn_type = htole32(STA_TYPE_ADHOC | NETWORK_IBSS);
		break;
#endif
	}

	basic->aid = htole16(IEEE80211_AID(ni->ni_associd));
	memcpy(basic->peer_addr, ni->ni_macaddr, IEEE80211_ADDR_LEN);
	basic->qos = (ni->ni_flags & IEEE80211_NODE_QOS) != 0;
}

void
mt7921_mcu_add_sta_tlv(struct mbuf *m, uint16_t *tlvnum, struct mwx_softc *sc,
    struct ieee80211_node *ni, int add, int newsta)
{
	//struct ieee80211com *ic = &sc->sc_ic;
	struct sta_rec_ra_info *ra_info;
	struct sta_rec_state *state;
	struct sta_rec_phy *phy;
	uint16_t supp_rates;

	/* sta rec ht */
	if (ni != NULL && ieee80211_node_supports_ht(ni)) {
		struct sta_rec_ht *ht;

		ht = (struct sta_rec_ht *)mwx_append_tlv(m, tlvnum, STA_REC_HT, sizeof(*ht));
		ht->ht_cap = htole16(ni->ni_htcaps);
	}

	/* sta rec vht */
	if (ni != NULL && ieee80211_node_supports_vht(ni)) {
		struct sta_rec_vht *vht;

		vht = (struct sta_rec_vht *)mwx_append_tlv(m, tlvnum, STA_REC_VHT,
		    sizeof(*vht));
		vht->vht_cap = htole32(ni->ni_vhtcaps);
		vht->vht_rx_mcs_map = htole16(ni->ni_vht_rxmcs);
		vht->vht_tx_mcs_map = htole16(ni->ni_vht_txmcs);
	}

#ifdef NOTYET
	if (sta->deflink.ht_cap.ht_supported || sta->deflink.he_cap.has_he)
		mt76_connac_mcu_sta_amsdu_tlv(skb, sta, vif);

	/* sta rec he */
	if (sta->deflink.he_cap.has_he) {
		mt76_connac_mcu_sta_he_tlv(skb, sta);
		if (band == NL80211_BAND_6GHZ &&
		    sta_state == MT76_STA_INFO_STATE_ASSOC) {
			struct sta_rec_he_6g_capa *he_6g_capa;

			he_6g_capa = (struct sta_rec_he_6g_capa *)mwx_append_tlv(m, tlvnum,
			    STA_REC_HE_6G, sizeof(*he_6g_capa));
			he_6g_capa->capa = sta->deflink.he_6ghz_capa.capa;
		}
	}
#endif

	phy = (struct sta_rec_phy *)mwx_append_tlv(m, tlvnum, STA_REC_PHY, sizeof(*phy));
	/* XXX basic_rates: bitmap of basic rates, each bit stands for an
	 *      index into the rate table configured by the driver in
	 *      the current band.
	 */
	phy->basic_rate = htole16(0x0150); /* XXX */
	phy->phy_type = mt7921_get_phy_mode_v2(sc, ni);
#ifdef NOTYET
	phy->ampdu = FIELD_PREP(IEEE80211_HT_AMPDU_PARM_FACTOR,
	    sta->deflink.ht_cap.ampdu_factor) |
	    FIELD_PREP(IEEE80211_HT_AMPDU_PARM_DENSITY,
	    sta->deflink.ht_cap.ampdu_density);
#endif
	// XXX phy->rcpi = rssi_to_rcpi(-ewma_rssi_read(&sc->sc_vif.rssi));
	phy->rcpi = 0xdc;	/* XXX STOLEN FROM LINUX DUMP */

#ifdef HACK
	supp_rates = sta->deflink.supp_rates[band];
	if (band == NL80211_BAND_2GHZ)
		supp_rates = FIELD_PREP(RA_LEGACY_OFDM, supp_rates >> 4) |
		FIELD_PREP(RA_LEGACY_CCK, supp_rates & 0xf);
	else
		supp_rates = FIELD_PREP(RA_LEGACY_OFDM, supp_rates);
#else
	supp_rates = RA_LEGACY_OFDM;
#endif

	ra_info = (struct sta_rec_ra_info *)mwx_append_tlv(m, tlvnum, STA_REC_RA,
	    sizeof(*ra_info));
	ra_info->legacy = htole16(supp_rates);
	if (ni != NULL && ieee80211_node_supports_ht(ni))
		memcpy(ra_info->rx_mcs_bitmask, ni->ni_rxmcs,
		    HT_MCS_MASK_NUM);

	state = (struct sta_rec_state *)mwx_append_tlv(m, tlvnum, STA_REC_STATE, sizeof(*state));
	state->state = mwx_sta_rec_state(ni);
	if (ni != NULL && ieee80211_node_supports_vht(ni)) {
		uint8_t width = ieee80211_node_vht_channel_width(ni);
		uint8_t opmode = 0;

		switch (width) {
		case IEEE80211_VHTOP0_CHAN_WIDTH_80:
			opmode = 2;
			break;
		case IEEE80211_VHTOP0_CHAN_WIDTH_160:
		case IEEE80211_VHTOP0_CHAN_WIDTH_8080:
			opmode = 3;
			break;
		default:
			opmode = 0;	/* 20/40 MHz */
			break;
		}
		/* Operating Mode Notification: RX NSS in bits 4-6. */
		if (ni->ni_vht_ss > 1)
			opmode |= (ni->ni_vht_ss - 1) << 4;
		state->vht_opmode = opmode;
	}
}

int
mt7921_mcu_wtbl_generic_tlv(struct mbuf *m, uint16_t *tlvnum,
    struct mwx_softc *sc, struct ieee80211_node *ni)
{
	struct wtbl_generic *generic;
	struct wtbl_rx *rx;
	int rca1 = 1;

	generic = (struct wtbl_generic *)mwx_append_tlv(m, tlvnum, WTBL_GENERIC,
	    sizeof(*generic));

	if (ni) {
		generic->partial_aid = htole16(IEEE80211_AID(ni->ni_associd));
		memcpy(generic->peer_addr, ni->ni_macaddr, IEEE80211_ADDR_LEN);
		generic->muar_idx = sc->sc_vif.omac_idx;
		generic->qos = (ni->ni_flags & IEEE80211_NODE_QOS) != 0;
	} else {
		memset(generic->peer_addr, 0xff, IEEE80211_ADDR_LEN);
		generic->muar_idx = 0xe;
	}

	rx = (struct wtbl_rx *)mwx_append_tlv(m, tlvnum, WTBL_RX, sizeof(*rx));
#ifndef IEEE80211_STA_ONLY
	if (ni != NULL)
		rca1 = sc->sc_ic.ic_opmode != IEEE80211_M_HOSTAP;
#endif

	rx->rca1 = rca1;
	rx->rca2 = 1;
	rx->rv = 1;

	return sizeof(*generic) + sizeof(*rx);
}

int
mt7921_mcu_wtbl_hdr_trans_tlv(struct mbuf *m, uint16_t *tlvnum,
    struct mwx_softc *sc, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct wtbl_hdr_trans *htr;

	htr = (struct wtbl_hdr_trans *)mwx_append_tlv(m, tlvnum, WTBL_HDR_TRANS, sizeof(*htr));

	/* no hdr decapsulation offload */
	htr->no_rx_trans = 1;

	if (ic->ic_opmode == IEEE80211_M_STA)
		htr->to_ds = 1;
	else
		htr->from_ds = 1;

	return sizeof(*htr);
}

int
mt7921_mcu_wtbl_ht_tlv(struct mbuf *m, uint16_t *tlvnum,
    struct mwx_softc *sc, struct ieee80211_node *ni)
{
	struct wtbl_smps *smps;

	/* XXX lots missing here */

	smps = (struct wtbl_smps *)mwx_append_tlv(m, tlvnum, WTBL_SMPS, sizeof(*smps));
	/* spatial multiplexing power save mode, off for now */
	//smps->smps = (sta->deflink.smps_mode == IEEE80211_SMPS_DYNAMIC);

	return sizeof(*smps);
}

int
mt7921_mac_sta_update(struct mwx_softc *sc, struct ieee80211_node *ni,
    int add, int newsta)
{
	struct mwx_node *mn = (struct mwx_node *)ni;
	struct mwx_vif *mvif = &sc->sc_vif;
	struct sta_req_hdr *hdr;
	struct sta_rec_wtbl *wtbl;
	struct mbuf *m;
	uint16_t tlvnum = 0, wnum = 0;
	int wlen = 0;

	m = mwx_alloc_sta_req_tlv(sizeof(*hdr));
	if (m == NULL)
		return ENOBUFS;

	if (ni != NULL)
		mt7921_mcu_add_basic_tlv(m, &tlvnum, sc, ni, add, newsta);

	if (ni != NULL && add)
		mt7921_mcu_add_sta_tlv(m, &tlvnum, sc, ni, add, newsta);

	wtbl = (struct sta_rec_wtbl *)mwx_append_tlv(m, &tlvnum, STA_REC_WTBL,
	    sizeof(*wtbl));
	wtbl->wlan_idx_lo = mn ? mn->wcid & 0xff : 0,
	wtbl->wlan_idx_hi = mn ? mn->wcid >> 8 : 0,
	wtbl->operation = WTBL_RESET_AND_SET;

	if (add) {
		wlen += mt7921_mcu_wtbl_generic_tlv(m, &wnum, sc, ni);
		wlen += mt7921_mcu_wtbl_hdr_trans_tlv(m, &wnum, sc, ni);

		if (ni)
			wlen += mt7921_mcu_wtbl_ht_tlv(m, &wnum, sc, ni);
	}

	wtbl->tlv_num = htole16(wnum);
	wtbl->len = htole16(le16toh(wtbl->len) + wlen);

	mwx_fill_sta_req_hdr(m, mvif, mn ? mvif->omac_idx : 0,
	    mn ? mn->wcid : 0, tlvnum);
	return mwx_mcu_send_mbuf_wait(sc, MCU_UNI_CMD_STA_REC_UPDATE, m);
}

int
mt7925_mac_sta_update(struct mwx_softc *sc, struct ieee80211_node *ni,
    int add, int newsta)
{
	struct {
		uint16_t	tag;
		uint16_t	len;
		uint8_t		state;
		uint8_t		rsv1[3];
		uint32_t	flags;
		uint8_t		vht_opmode;
		uint8_t		action;
		uint8_t		rsv2[2];
	} __packed *state;
	struct sta_rec_phy *phy;
	struct sta_rec_ra_info *ra_info;
	struct sta_rec_hdr_trans *hdr_trans;
	struct mwx_node *mn = (struct mwx_node *)ni;
	struct mwx_vif *mvif = &sc->sc_vif;
	struct sta_req_hdr *hdr;
	struct mbuf *m;
	uint16_t tlvnum = 0;
	uint16_t supp_rates;

	if (ni == NULL)
		return EINVAL;

	m = mwx_alloc_sta_req_tlv(sizeof(*hdr));
	if (m == NULL)
		return ENOBUFS;

	if (add)
		mt7921_mcu_add_basic_tlv(m, &tlvnum, sc, ni, add, newsta);

	/* HT capabilities, mandatory for 11n operation. */
	if (ieee80211_node_supports_ht(ni)) {
		struct sta_rec_ht *ht;

		ht = (struct sta_rec_ht *)mwx_append_tlv(m, &tlvnum, STA_REC_HT, sizeof(*ht));
		ht->ht_cap = htole16(ni->ni_htcaps);
	}

	/* VHT capabilities, mandatory for 11ac operation on 5 GHz. */
	if (ieee80211_node_supports_vht(ni)) {
		struct sta_rec_vht *vht;

		vht = (struct sta_rec_vht *)mwx_append_tlv(m, &tlvnum, STA_REC_VHT,
		    sizeof(*vht));
		vht->vht_cap = htole32(ni->ni_vhtcaps);
		vht->vht_rx_mcs_map = htole16(ni->ni_vht_rxmcs);
		vht->vht_tx_mcs_map = htole16(ni->ni_vht_txmcs);
	}

	phy = (struct sta_rec_phy *)mwx_append_tlv(m, &tlvnum, STA_REC_PHY, sizeof(*phy));
	if (IEEE80211_IS_CHAN_2GHZ(ni->ni_chan))
		phy->basic_rate = htole16(0x000f);
	else
		phy->basic_rate = htole16((1 << 6) | (1 << 8) | (1 << 10));
	phy->phy_type = mt7921_get_phy_mode_v2(sc, ni);
	phy->rcpi = 0xdc;

	if (IEEE80211_IS_CHAN_2GHZ(ni->ni_chan))
		supp_rates = RA_LEGACY_OFDM | RA_LEGACY_CCK;
	else
		supp_rates = RA_LEGACY_OFDM;

	ra_info = (struct sta_rec_ra_info *)mwx_append_tlv(m, &tlvnum, STA_REC_RA,
	    sizeof(*ra_info));
	ra_info->legacy = htole16(supp_rates);
	if (ieee80211_node_supports_ht(ni))
		memcpy(ra_info->rx_mcs_bitmask, ni->ni_rxmcs,
		    HT_MCS_MASK_NUM);

	state = (typeof(state))mwx_append_tlv(m, &tlvnum, STA_REC_STATE,
	    sizeof(*state));
	state->state = mwx_sta_rec_state(ni);
	if (ieee80211_node_supports_vht(ni)) {
		uint8_t width = ieee80211_node_vht_channel_width(ni);
		uint8_t opmode = 0;

		switch (width) {
		case IEEE80211_VHTOP0_CHAN_WIDTH_80:
			opmode = 2;
			break;
		case IEEE80211_VHTOP0_CHAN_WIDTH_160:
		case IEEE80211_VHTOP0_CHAN_WIDTH_8080:
			opmode = 3;
			break;
		default:
			opmode = 0;
			break;
		}
		if (ni->ni_vht_ss > 1)
			opmode |= (ni->ni_vht_ss - 1) << 4;
		state->vht_opmode = opmode;
	}

	hdr_trans = (struct sta_rec_hdr_trans *)mwx_append_tlv(m, &tlvnum, STA_REC_HDR_TRANS,
	    sizeof(*hdr_trans));
	hdr_trans->to_ds = 1;
	hdr_trans->dis_rx_hdr_tran = 1;

	mwx_fill_sta_req_hdr(m, mvif, mn ? mvif->omac_idx : 0,
	    mn ? mn->wcid : 0, tlvnum);

	return mwx_mcu_send_mbuf_wait(sc, MCU_UNI_CMD_STA_REC_UPDATE, m);
}

static int
mt7921_key_to_cipher_id(struct ieee80211_key *k)
{
	switch (k->k_cipher) {
	case IEEE80211_CIPHER_CCMP:
		return MCU_CIPHER_AES_CCMP;
	case IEEE80211_CIPHER_TKIP:
		return MCU_CIPHER_TKIP;
	case IEEE80211_CIPHER_WEP40:
		return MCU_CIPHER_WEP40;
	case IEEE80211_CIPHER_WEP104:
		return MCU_CIPHER_WEP104;
	case IEEE80211_CIPHER_BIP:
		return MCU_CIPHER_BIP_CMAC_128;
	default:
		return MCU_CIPHER_NONE;
	}
}

static int
mt7925_key_to_cipher_id(struct ieee80211_key *k)
{
	switch (k->k_cipher) {
	case IEEE80211_CIPHER_CCMP:
		return MT7925_CIPHER_AES_CCMP;
	case IEEE80211_CIPHER_TKIP:
		return MT7925_CIPHER_TKIP;
	case IEEE80211_CIPHER_WEP40:
		return MT7925_CIPHER_WEP40;
	case IEEE80211_CIPHER_WEP104:
		return MT7925_CIPHER_WEP104;
	case IEEE80211_CIPHER_BIP:
		return MT7925_CIPHER_BIP_CMAC_128;
	default:
		return MT7925_CIPHER_NONE;
	}
}

void
mt7921_mcu_add_key_tlv(struct mbuf *m, uint16_t *tlvnum,
    struct ieee80211_key *k, int add)
{
	struct sta_rec_sec *sec;
	struct sta_rec_sec_key *key;

	sec = (struct sta_rec_sec *)mwx_append_tlv(m, tlvnum, STA_REC_KEY, sizeof(*sec));

	if (add) {
		sec->n_cipher = 1;
		sec->add = 0;	/* connac2 has inverted logic */
		sec->len = htole16(sizeof(*sec) + sizeof(*key));

		key = (struct sta_rec_sec_key *)mwx_append_len(m, sizeof(*key));
		key->cipher_id = mt7921_key_to_cipher_id(k);
		key->cipher_len = sizeof(*key);
		key->key_id = k->k_id;
		key->key_len = k->k_len;

		CTASSERT(sizeof(k->k_key) == sizeof(key->key));
		memcpy(key->key, k->k_key, k->k_len);

		if (k->k_cipher == IEEE80211_CIPHER_TKIP &&
		    k->k_len == sizeof(key->key)) {
			/* Rx/Tx MIC keys are swapped */
			memcpy(key->key + 16, k->k_key + 24, 8);
			memcpy(key->key + 24, k->k_key + 16, 8);
		}
	} else {
		sec->add = 1;
		sec->n_cipher = 0;
	}
}

void
mt7925_mcu_add_key_tlv(struct mbuf *m, uint16_t *tlvnum,
    struct ieee80211_key *k, struct ieee80211_node *ni, uint16_t wcid, int add)
{
	struct sta_rec_sec_v3 *sec;
	int is_unicast;

	sec = (struct sta_rec_sec_v3 *)mwx_append_tlv(m, tlvnum, STA_REC_KEY_V3, sizeof(*sec));
	is_unicast = (k->k_flags & IEEE80211_KEY_GROUP) == 0;

	sec->tx_key = is_unicast ? 1 : 0;
	sec->key_type = 1;
	if (is_unicast && ni != NULL)
		memcpy(sec->peer_addr, ni->ni_macaddr,
		    IEEE80211_ADDR_LEN);
	else if (ni != NULL)
		memcpy(sec->peer_addr, ni->ni_bssid,
		    IEEE80211_ADDR_LEN);
	sec->key_id = k->k_id;
	sec->wlan_idx = wcid;
	sec->mgmt_prot = 1;

	if (add) {
		sec->add = 1;

		sec->cipher_id = mt7925_key_to_cipher_id(k);
		sec->key_len = k->k_len;

		CTASSERT(sizeof(k->k_key) == sizeof(sec->key));
		memcpy(sec->key, k->k_key, k->k_len);

		if (k->k_cipher == IEEE80211_CIPHER_TKIP &&
		    k->k_len == sizeof(sec->key)) {
			/* Rx/Tx MIC keys are swapped */
			memcpy(sec->key + 16, k->k_key + 24, 8);
			memcpy(sec->key + 24, k->k_key + 16, 8);
		}
	} else {
		sec->add = 0;
	}
}

int
mwx_mcu_sta_key_update(struct mwx_softc *sc, struct ieee80211_node *ni,
    struct ieee80211_key *k)
{
	struct mwx_vif *mvif = &sc->sc_vif;
	struct mwx_node *mn = (struct mwx_node *)ni;
	struct sta_req_hdr *hdr;
	struct mbuf *m;
	int rv;
	uint32_t status = 0;
	uint16_t tlvnum = 0, wcid = 0;
	uint8_t muar_idx = 0;

	if ((k->k_flags & IEEE80211_KEY_GROUP) != 0) {
		wcid = mvif->vif_mn.wcid;
		muar_idx = 0x0e;
	} else {
		wcid = mn->wcid;
	}

	m = mwx_alloc_sta_req_tlv(sizeof(*hdr));
	if (m == NULL)
		return ENOBUFS;

	DPRINTF("%s: %s: ni %p, k_id %d, k_flags %x k_cipher %d wcid %d\n",
	    DEVNAME(sc), "add key", ni, k->k_id, k->k_flags, k->k_cipher, wcid);

	if (sc->sc_hwtype == MWX_HW_MT7925)
		mt7925_mcu_add_key_tlv(m, &tlvnum, k, ni, wcid, 1);
	else
		mt7921_mcu_add_key_tlv(m, &tlvnum, k, 1);
	mwx_fill_sta_req_hdr(m, mvif, muar_idx, wcid, tlvnum);

	rv = mwx_mcu_send_mbuf_wait_status(sc, MCU_UNI_CMD_STA_REC_UPDATE,
	    m, &status, MWX_MCU_KEY_TIMEOUT_MS);
	if (rv != 0) {
		printf("%s: %s: no reply for wcid %d key %d: error %d\n",
		    DEVNAME(sc), "add key", wcid, k->k_id, rv);
		return rv;
	}
	if (status != 0) {
		/*
		 * The firmware refused the key.  Report it instead of
		 * pretending the port is secured: net80211 treats a
		 * synchronous success as "link up, port valid", and a
		 * firmware that never got the key drops every protected
		 * frame in both directions.
		 */
		printf("%s: %s: wcid %d key %d rejected by firmware, "
		    "status %u\n", DEVNAME(sc), "add key", wcid, k->k_id,
		    status);
		return EIO;
	}

	/*
	 * Do not touch ni_port_valid or the link state here.
	 *
	 * net80211 owns that decision: mwx_set_key() installs the key
	 * synchronously and returns the firmware's answer, and
	 * ieee80211_recv_4way_msg3() then marks the port valid and
	 * brings the link up itself.  Doing it again from the driver
	 * would make the link depend on the AP sending the group key
	 * inside message 3 -- an AP that delivers the GTK later (or not
	 * at all, which is legal) would then never get a working link,
	 * and DHCP would never start.
	 */
	return 0;
}

void
mwx_mcu_sta_key_delete(struct mwx_softc *sc, struct ieee80211_node *ni,
    struct ieee80211_key *k)
{
	struct mwx_vif *mvif = &sc->sc_vif;
	struct mwx_node *mn = (struct mwx_node *)ni;
	struct sta_req_hdr *hdr;
	struct mbuf *m;
	uint16_t tlvnum = 0, wcid = 0;
	uint8_t muar_idx = 0;

	if ((k->k_flags & IEEE80211_KEY_GROUP) != 0) {
		wcid = mvif->vif_mn.wcid;
		muar_idx = 0x0e;
	} else {
		wcid = mn->wcid;
	}

	m = mwx_alloc_sta_req_tlv(sizeof(*hdr));
	if (m == NULL)
		return;

	DPRINTF("%s: %s: ni %p, k_id %d, k_flags %x k_cipher %d wcid %d\n",
	    DEVNAME(sc), "delete key", ni, k->k_id, k->k_flags, k->k_cipher,
	    wcid);

	if (sc->sc_hwtype == MWX_HW_MT7925)
		mt7925_mcu_add_key_tlv(m, &tlvnum, k, ni, wcid, 0);
	else
		mt7921_mcu_add_key_tlv(m, &tlvnum, k, 0);
	mwx_fill_sta_req_hdr(m, mvif, muar_idx, wcid, tlvnum);

	mwx_mcu_send_mbuf(sc, MCU_UNI_CMD_STA_REC_UPDATE, m, NULL);
}
