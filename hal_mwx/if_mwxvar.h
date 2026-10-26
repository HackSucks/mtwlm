/*
 * if_mwxvar.h - driver-visible mwx(4) state.
 *
 * Shared by the OpenBSD driver body and the AirportMtwlm IOKit glue so
 * that `struct mwx_softc` can be embedded in the HAL class.
 */

#ifndef _MWXVAR_H_
#define _MWXVAR_H_

struct mwx_txwi_desc {
	struct mt76_txwi		*mt_desc;
	struct mwx_txwi			*mt_data;

	u_int				mt_count;
	bus_dmamap_t			mt_map;
	bus_dma_segment_t		mt_seg;
	LIST_HEAD(, mwx_txwi)		mt_freelist;
};

struct mwx_queue_data {
	struct mbuf			*md_mbuf;
	struct mwx_txwi			*md_txwi;
	bus_dmamap_t			md_map;
};

struct mwx_queue {
	uint32_t			mq_regbase;
	u_int				mq_count;
	u_int				mq_prod;
	u_int				mq_cons;

	struct mwx_desc			*mq_desc;
	struct mwx_queue_data		*mq_data;
	struct mbuf			*mq_mbuf;
	struct mbuf			*mq_mtail;

	bus_dmamap_t			mq_map;
	bus_dma_segment_t		mq_seg;
	int				mq_wakeme;
};

struct mwx_hw_capa {
	int8_t		has_2ghz;
	int8_t		has_5ghz;
	int8_t		has_6ghz;
	uint8_t		antenna_mask;
	uint8_t		num_streams;
};

struct mwx_node {
	struct ieee80211_node	ni;
	uint16_t		wcid;
};

struct mwx_vif {
	struct mwx_node		vif_mn;
	uint8_t			idx;
	uint8_t			omac_idx;
	uint8_t			band_idx;
	uint8_t			wmm_idx;
	uint8_t			scan_seq_num;
};

enum mwx_hw_type {
	MWX_HW_MT7920,
	MWX_HW_MT7921,
	MWX_HW_MT7922,
	MWX_HW_MT7925,
};

struct mwx_softc {
	struct device		sc_dev;
	struct ieee80211com	sc_ic;

	enum mwx_hw_type	sc_hwtype;

	struct mwx_queue	sc_txq;
	struct mwx_queue	sc_txmcuq;
	struct mwx_queue	sc_txfwdlq;

	struct mwx_queue	sc_rxq;
	struct mwx_queue	sc_rxmcuq;
	struct mwx_queue	sc_rxfwdlq;

	struct mwx_txwi_desc	sc_txwi;

	bus_space_tag_t		sc_st;
	bus_space_handle_t	sc_memh;
	bus_size_t		sc_mems;
	bus_dma_tag_t		sc_dmat;
	pcitag_t		sc_tag;
	pci_chipset_tag_t	sc_pc;
	void			*sc_ih;

	int			(*sc_newstate)(struct ieee80211com *,
				    enum ieee80211_state, int);

	struct taskq		*sc_nswq;;
	struct task		sc_newstate_task;
	struct task		sc_bgscan_done_task;
	struct task		sc_scan_task;
	struct task		sc_bmiss_task;
	struct task		sc_reset_task;
	CTimeout		*sc_reset_to;
	u_int			sc_flags;
#define MWX_FLAG_SCANNING		0x01
#define MWX_FLAG_BGSCAN			0x02
	int			sc_coredump_cnt;
	int8_t			sc_resetting;
	int8_t			sc_fw_loaded;
	/* set by DVACT_QUIESCE, consumed by the matching DVACT_WAKEUP */
	int8_t			sc_pm_suspended;
	uint32_t		sc_intr_count;
	/* counted in the primary filter, to tell "no MSI" from "no action" */
	uint32_t		sc_intr_filter;
	/*
	 * WFDMA ring serialisation.
	 *
	 * The interrupt action and any thread that waits for the device
	 * poll the rings (see mwx_dma_poll()), and a ring must never be
	 * consumed by two of them at once.  sc_ring_owner names the thread
	 * holding the lock so that the polling helpers can re-enter from
	 * the interrupt action (an MCU command issued while processing a
	 * received event) without deadlocking on the non-recursive IOLock.
	 */
	void			*sc_ring_lock;
	void			*sc_ring_owner;

	/*
	 * Producer side of the transmit path.  mwx_tx() and the MCU send
	 * path can run concurrently on the network output thread, on the
	 * state task queue and in the receive path (the 4-way handshake
	 * answer is transmitted from there), so the ring producer index and
	 * the TXWI freelist need a lock of their own.  It is only ever held
	 * for the duration of a single enqueue and never together with
	 * sc_ring_lock, so the two cannot deadlock.
	 */
	void			*sc_tx_lock;

	/*
	 * Frames taken off the receive rings with sc_ring_lock held,
	 * waiting to be handed to net80211.  The receive path must not run
	 * under sc_ring_lock: it calls if_input(), which takes the workloop
	 * gate, while the workloop interrupt action can itself be waiting
	 * for sc_ring_lock.  See mwx_dma_rx_deliver().
	 */
	struct mbuf_list	sc_rx_pending;
	/*
	 * Data frames that were split off a batch while net80211 was
	 * already running (a wait loop polled the rings from inside
	 * if_input()).  The outermost delivery loop picks them up after
	 * the current frame has been fully processed.
	 */
	struct mbuf_list	sc_rx_deferred;
	int			sc_rx_delivering;

	enum ieee80211_state	sc_ns_state;
	int			sc_ns_arg;

	/*
	 * Background scan pacing, in milliseconds of monotonic uptime.
	 *
	 * A background scan takes the radio off-channel for the whole
	 * channel list (the firmware does not do an off-channel scan while
	 * associated), so a scan that starts while DHCP is still running
	 * simply starves it: the request goes out, the reply arrives while
	 * the radio is parked on another channel, and the lease never
	 * completes.  sc_bgscan_hold keeps the radio on the associated
	 * channel for a while after link-up, and sc_bgscan_last spaces
	 * later scans out so airportd cannot turn scanning into a
	 * continuous off-channel state.
	 */
	uint64_t		sc_bgscan_last;
	uint64_t		sc_bgscan_hold;

	/*
	 * When the scan currently recorded in MWX_FLAG_SCANNING was
	 * started, in milliseconds of monotonic uptime.  The flag is
	 * cleared by the scan-done event, which is an unsolicited
	 * message like any other: if it is lost the flag stays set and
	 * the state machine refuses to start another scan, so a stale
	 * flag has to be distinguishable from a running scan.
	 */
	uint64_t		sc_scan_start;

	struct ieee80211_node_switch_bss_arg	*sc_bgscan_arg;
	size_t			sc_bgscan_arg_size;

#if NBPFILTER > 0
	caddr_t			sc_drvbpf;
	union {
		struct mwx_rx_radiotap_header th;
		uint8_t pad[IEEE80211_RADIOTAP_HDRLEN];
	}			sc_rxtapu;
	int			sc_rxtap_len;
	union {
		struct mwx_tx_radiotap_header th;
		uint8_t pad[IEEE80211_RADIOTAP_HDRLEN];
	}			sc_txtapu;
	int			sc_txtap_len;
#define	sc_rxtap	sc_rxtapu.th
#define	sc_txtap	sc_txtapu.th
#endif

	struct mwx_node		global_mn;
	struct mwx_vif		sc_vif;
	uint32_t		sc_wcid_mask;

	uint32_t		sc_intr_mask;

	/* mcu */
	uint32_t		sc_mcu_seq;
	struct {
		struct mbuf	*mcu_m;
		uint32_t	 mcu_cmd;
		uint32_t	 mcu_int;
	}			sc_mcu_wait[16];
	uint8_t			sc_scan_seq_num;

	/* phy / hw */
	struct mwx_hw_capa	sc_capa;
	uint8_t			sc_lladdr[ETHER_ADDR_LEN];
	char			sc_alpha2[4]; /* regulatory-domain */

	int16_t			sc_coverage_class;
	uint8_t			sc_slottime;
};

#endif /* _MWXVAR_H_ */
