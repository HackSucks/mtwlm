/*
 * kmod_info.c -- the kext's kmod_info record.
 *
 * This file must be compiled as C, not C++.  The kernel and OpenCore
 * look up the *unmangled* Mach-O symbol `_kmod_info`, and `_start` /
 * `_stop` have to keep C linkage so that the references here bind to
 * libkmod's implementations.  Compiled as C++ they would come out as
 * `__Z6_startP11kmod_info_tPv`, and the prelinker would reject the kext
 * ("no kmod_info" / unresolved symbols).
 *
 * KMOD_EXPLICIT_DECL(name, version, start, stop) stringifies `name`, so
 * the identifier is written unquoted.  It must match CFBundleIdentifier
 * (PKGID in the Makefile).
 *
 * `_realmain` / `_antimain` are referenced by libkmod's c_start/c_stop.
 * They are intentionally 0: on x86_64/arm64 the kernel runs the C++
 * runtime init itself, the legacy 32-bit libkmodc++ entry points are not
 * used.  `_kext_apple_cc` is part of the standard Xcode-generated record
 * and tells the loader which compiler built the kext.
 */

#include <mach/mach_types.h>

extern kern_return_t _start(kmod_info_t *ki, void *data);
extern kern_return_t _stop(kmod_info_t *ki, void *data);

/* Set by the Makefile from VERSION; the fallback keeps a plain
 * `clang kmod_info.c` working. */
#ifndef MTWLM_KMOD_VERSION
#define MTWLM_KMOD_VERSION "1.0.1"
#endif

KMOD_EXPLICIT_DECL(com.nvme.AirportMtwlm, MTWLM_KMOD_VERSION, _start, _stop)

__private_extern__ kmod_start_func_t *_realmain = 0;
__private_extern__ kmod_stop_func_t  *_antimain = 0;

/* Apple clang defines __APPLE_CC__; the stock clang used for the Linux
 * syntax check does not.  The value is only a compatibility marker. */
#ifndef __APPLE_CC__
#define __APPLE_CC__ 6000
#endif

__private_extern__ int _kext_apple_cc = __APPLE_CC__;
