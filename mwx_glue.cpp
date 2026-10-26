/*
 * mwx_glue.cpp - glue between the OpenBSD-derived mwx driver body and
 * the macOS IOKit environment.
 *
 * Implements the small set of kernel services the driver body needs:
 * printf routing, sleeping/waking, and the cluster/mbuf allocation
 * helpers emulated over the macOS mbuf KPI.
 *
 * Copyright (c) 2026 NVMe
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#include <libkern/libkern.h>
#include <FwData.h>
#include <IOKit/IOLib.h>
#include <sys/kpi_mbuf.h>

#include "mwx_compat.h"
#include "ItlMwx.hpp"

/* --------------------------------------------------------------------
 * printf
 * -------------------------------------------------------------------- */

void
mwx_printf(const char *fmt, ...)
{
	va_list ap;

	kprintf("mwx0: ");
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

/* --------------------------------------------------------------------
 * sleeping and waking
 *
 * mwx_tsleep_nsec() is forwarded to the attached ItlMwx service, which
 * owns the lock the sleepers wait on (ItlHalService::tsleep_nsec()).
 * wakeup() is a plain XNU service and is called directly by the driver
 * body; it pairs with the msleep() inside ItlHalService.
 * -------------------------------------------------------------------- */

ItlMwx *g_mwx_service = NULL;

int
mwx_tsleep_nsec(void *ident, int priority, const char *wmesg, uint64_t timo)
{
	if (g_mwx_service == NULL)
		return 0;
	return g_mwx_service->mwxSleepNsec(ident, priority, wmesg, timo);
}

/*
 * --------------------------------------------------------------------
 * allocation helpers
 *
 * The vendored itl80211 allocator keeps its own length header, so
 * mwx_free() only needs the pointer (see mwx_compat.h).
 * --------------------------------------------------------------------
 */

void
mwx_free(void *p)
{
	free(p);
}

/*
 * --------------------------------------------------------------------
 * embedded firmware table
 *
 * fwList/fwNumber are generated into include/MtwlmFwBinary.cpp from the
 * MediaTek images in firmware/ (MT7961 for MT7920/MT7921/MT7921K,
 * MT7922 for MT7922/RZ616, MT7925 for MT7925/RZ717), zlib-compressed by
 * itlwm's scripts/zlib_compress_fw.py.
 * -------------------------------------------------------------------- */

/*
 * --------------------------------------------------------------------
 * firmware loading
 *
 * OpenBSD's loadfirmware() is replaced by the embedded-blob mechanism
 * itlwm uses (FwData.h): look the firmware name up in fwList, inflate it
 * and hand back the decompressed buffer.  The MediaTek streams files
 * are whole files (their patch/RAM images are expected verbatim), so
 * the buffer contains exactly the decompressed bytes.
 * The table carries the eight images for MT7920/MT7921/MT7921K (the
 * MT7961 set), MT7922/RZ616 and MT7925/RZ717.
 * --------------------------------------------------------------------
 */

int
loadfirmware(const char *fwname, u_char **bufp, size_t *buflenp)
{
	OSData *data = getFWDescByName(fwname);
	z_stream stream;
	u_char *buf;
	size_t cap;

	if (data == NULL)
		return ENOENT;

	/* Decompressed size is unknown; grow on demand. */
	cap = data->getLength() * 8 + 65536;
	buf = (u_char *)malloc(cap, M_DEVBUF, M_WAIT | M_ZERO);
	if (buf == NULL)
		return ENOMEM;

	for (;;) {
		memset(&stream, 0, sizeof(stream));
		stream.next_in = (Bytef *)data->getBytesNoCopy();
		stream.avail_in = data->getLength();
		stream.next_out = (Bytef *)buf;
		stream.avail_out = (uInt)cap;
		stream.zalloc = zcalloc;
		stream.zfree = zcfree;

		if (inflateInit(&stream) != Z_OK) {
			mwx_free(buf);
			return EINVAL;
		}
		int rv = inflate(&stream, Z_FINISH);
		inflateEnd(&stream);

		if (rv == Z_STREAM_END)
			break;
		if (rv != Z_OK && rv != Z_BUF_ERROR) {
			mwx_free(buf);
			return EINVAL;
		}
		/* Output buffer full: double and retry. */
		mwx_free(buf);
		cap *= 2;
		buf = (u_char *)malloc(cap, M_DEVBUF, M_WAIT | M_ZERO);
		if (buf == NULL)
			return ENOMEM;
	}

	*bufp = buf;
	*buflenp = stream.total_out;
	return 0;
}

/* --------------------------------------------------------------------
 * mbuf helpers
 *
 * OpenBSD m_clget()/MCLGETL allocate a bare cluster of the requested
 * size and attach it to a new (or the given) packet-header mbuf.  The
 * macOS KPI splits this into mbuf_alloccluster()/mbuf_attachcluster(),
 * which is what is used here so that the driver can keep its "place
 * the data region at the end of the buffer" discipline through
 * mwx_m_align().
 * -------------------------------------------------------------------- */

static void
mwx_extfree(caddr_t addr, u_int size, caddr_t arg)
{
	mbuf_freecluster(addr, size);
}

mbuf_t
mwx_mclgetl(mbuf_t m, int how, int len)
{
	size_t size;
	caddr_t buf;
	bool created = false;

	if (len <= 0)
		len = 2048;
	/*
	 * kpi supports 2048, 4096 and 16384 byte clusters.  Note that the
	 * 16 KB (jumbo) class is not pre-populated at boot and m_clalloc()
	 * refuses to grow it for non-blocking requests, so an MBUF_DONTWAIT
	 * allocation larger than 4096 can fail with ENOMEM early in boot.
	 * The firmware loader therefore keeps its 4096 byte scatter chunks
	 * inside a big cluster by not reserving a txd header.
	 */
	if (len <= 2048)
		size = 2048;
	else if (len <= 4096)
		size = 4096;
	else
		size = 16384;

	if (mbuf_alloccluster((mbuf_how_t)how, &size, &buf) != 0)
		return NULL;

	if (m == NULL) {
		/* OpenBSD's m_clget(NULL, ..) creates the header mbuf. */
		if (mbuf_gethdr((mbuf_how_t)how, MBUF_TYPE_DATA, &m) != 0) {
			mbuf_freecluster(buf, size);
			return NULL;
		}
		created = true;
	}

	if (mbuf_attachcluster((mbuf_how_t)how, MBUF_TYPE_DATA, &m, buf,
	    mwx_extfree, size, NULL) != 0) {
		mbuf_freecluster(buf, size);
		/* Never free an mbuf owned by the caller. */
		if (created)
			mbuf_freem(m);
		return NULL;
	}

	return m;
}

mbuf_t
mwx_m_clget(mbuf_t m, int how, int len)
{
	return mwx_mclgetl(m, how, len);
}

mbuf_t
mwx_m_gethdr(int how, int type)
{
	mbuf_t m = NULL;

	if (mbuf_gethdr((mbuf_how_t)how, (mbuf_type_t)type, &m) != 0)
		return NULL;
	return m;
}

void
mwx_m_align(mbuf_t m, int len)
{
	/*
	 * OpenBSD's m_align() places `len` bytes at the end of a freshly
	 * allocated buffer, long-word aligned.  At the call sites in
	 * this driver m_len is still 0 and m_data still points at the
	 * start of the external buffer, so the buffer size can be
	 * recovered from the trailing space.
	 */
	size_t size;

	if (m == NULL || mbuf_len(m) != 0)
		return;

	size = mbuf_trailingspace(m);
	if (size < (size_t)len)
		return;

	mbuf_setdata(m, (caddr_t)mbuf_data(m) +
	    ((size - (size_t)len) & ~(sizeof(long) - 1)), 0);
}

mbuf_t
mwx_m_pullup(mbuf_t m, int len)
{
	if (m == NULL)
		return NULL;
	if (mbuf_len(m) >= len)
		return m;
	if (mbuf_pullup(&m, len) != 0)
		return NULL;
	return m;
}

mbuf_t
mwx_m_prepend(mbuf_t m, int len, int how)
{
	if (m == NULL)
		return NULL;
	if (mbuf_prepend(&m, len, (mbuf_how_t)how) != 0)
		return NULL;
	return m;
}
