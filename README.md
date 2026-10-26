# AirportMtwlm

A native macOS driver for MediaTek's Wi-Fi 6/6E PCIe chips, derived from
OpenBSD's `mwx(4)`.  It is an `IO80211Controller`, so macOS treats the
card like a built-in Apple wireless adapter: it shows up in the menu bar
Wi-Fi picker and works with the system's own supplicant.  No Heliport,
no user-space helper.

## Supported hardware

| PCI ID    | Chip                        |
|-----------|-----------------------------|
| `0x7961`  | MT7921                      |
| `0x0608`  | MT7921K                     |
| `0x7920`  | MT7920                      |
| `0x7922`  | MT7922                      |
| `0x0616`  | RZ616 (MT7922 rebrand)      |
| `0x7925`  | MT7925                      |
| `0x0717`  | RZ717 (MT7925 rebrand)      |

All seven IDs are matched by `Info.plist` and share the same driver; the
chip is identified at attach time and selects the matching firmware
image.  2.4 GHz and 5 GHz are supported, including 80 MHz VHT on 5 GHz
and 160 MHz on MT7922/MT7925.  6 GHz is not supported.

## Building

Requirements: macOS with the Xcode command line tools, and
[MacKernelSDK](https://github.com/acidanthera/MacKernelSDK) checked out
next to this directory (or pass `MKS=/path/to/MacKernelSDK`).

IO80211Family changed shape in Sonoma, so there are two builds of the
same driver:

| macOS            | Command              |
|------------------|----------------------|
| Big Sur – Ventura| `make` (or `make OSVER=13` for the Ventura API level) |
| Sonoma 14.0+     | `make V2=1`          |

Both produce `AirportMtwlm.kext` with the same identifier and IOClass;
only the controller/interface sources and `Info.plist` differ.  The
classic build uses `Info.plist` and attaches straight to `IOPCIDevice`;
the V2 build uses `Info-V2.plist`, which attaches `IOPCIEDeviceWrapper`
to the PCI device and `AirportMtwlm` to that wrapper, in match category
`WiFiDriver` with `IONetworkRootType = airport`.

```sh
make                     # classic, host architecture
make ARCH=arm64          # Apple silicon
make CONFIG=Debug        # -O0, extra logging
make V2=1                # Sonoma 14.0 and newer
```

```sh
sudo cp -R AirportMtwlm.kext /Library/Extensions/
sudo chown -R root:wheel /Library/Extensions/AirportMtwlm.kext
sudo kmutil install --update-all
# reboot
```

`make install` does the copy and cache rebuild for you.  A reboot is
required the first time; after that `make load` / `make unload` are
usually enough while iterating.

Loading an unsigned or ad-hoc-signed kext requires that SIP is relaxed
(`csrutil disable` in Recovery, or at least `CSR_ALLOW_UNTRUSTED_KEXTS`)
and, on Apple silicon, that the boot policy allows third-party kexts.
This is the same requirement as any other Hackintosh Wi-Fi kext.

### Installing the right variant

Both variants produce a bundle called `AirportMtwlm.kext`, so build and
install one at a time into the same tree — a later `make V2=1` replaces
the bundle built by `make`:

| Your macOS        | Build             | Install                       |
|-------------------|-------------------|-------------------------------|
| Big Sur 11        | `make`            | `make install`                |
| Monterey 12       | `make OSVER=12`   | `make install`                |
| Ventura 13        | `make OSVER=13`   | `make install`                |
| Sonoma 14.0–14.3  | `make V2=1`       | `make V2=1 install`           |
| Sonoma 14.4+      | `make V2=1`       | `make V2=1 install`           |
| Sequoia / Tahoe   | `make V2=1`       | `make V2=1 install`           |

The Sonoma build targets the 14.4 API (`__IO80211_TARGET=__MAC_14_4`);
14.0–14.3 are covered by the 14.0 additions it still carries.  The
classic build defaults to the Big Sur API level and only adds the
availability shims newer systems need.

### Installing through OpenCore

The kext can also be injected by OpenCore instead of living in
`/Library/Extensions` — the same mechanism used for AirportItlwm:

1. copy `AirportMtwlm.kext` to `EFI/OC/Kexts/`;
2. add one entry under `Kernel -> Add`:

   ```
   BundlePath      AirportMtwlm.kext
   ExecutablePath  Contents/MacOS/AirportMtwlm
   PlistPath       Contents/Info.plist
   Enabled         true
   ```

   leave `MinKernel`/`MaxKernel` empty.

There is no Lilu-style plugin and no extra kext to load: the plist
already declares `IO80211Family`, `IONetworkingFamily`, `IOPCIFamily`
and the KPI libraries.  Keep exactly one copy of the driver — do not
also install it into `/Library/Extensions`.

Injection does not require the kext to be signed by Apple, but a boot
policy that forbids third-party code will still block it; if the kext
never shows up in `kmutil showloaded`, check `csr-active-config` and
`SecureBootModel`.

### First boot on hardware

Watch, in this order:

```sh
log show --last 5m --predicate 'process == "kernel"' | grep -iE 'mwx|Mtwlm'
```

1. The device matches, the kext loads, and `mwx0: attach succeed.`
   appears.  No `mwx0:` line at all means the PCI ID/plist or the kext
   load failed (`kmutil showloaded --list-only | grep -i mtwlm`).
   Firmware download happens *inside* attach (`mwx_preinit()` →
   `mwx_init_hardware()` → `mwx_load_firmware()`), so attach success
   already means the patch/RAM images were accepted and the MCU
   answered the capability query.  A failure before that point prints
   `loadfirmware error`, `firmware ... failed` or `command <x> timeout`
   and ends in `mwx0: attach failed`.
2. The interface exists (`ifconfig -l`, or the Wi-Fi menu shows the
   card).
3. A scan lists 2.4 and 5 GHz networks.
4. Association and the RSN handshake: the classic build hands this to
   Apple's supplicant, the Sonoma build does it in-kernel.

## Project layout

```
kmod_info.c               kmod_info record + _start/_stop glue (compiled as C)
AirportMtwlm.cpp/.hpp     classic IO80211Controller: probe, start/stop, power
AirportMtwlmInterface.*   classic IO80211Interface subclass (output path)
AirportMtwlmSTAIOCTL.cpp  classic SIOCGA80211 / SIOCSA80211 (scan, assoc, keys)
AirportMtwlmVirtualIOCTL.cpp, AirportMtwlmAWDL.cpp   classic virtual ifaces
AirportMtwlmV2.cpp/.hpp   Sonoma IO80211Controller (Skywalk)
AirportMtwlmSkywalkInterface.*  Sonoma IO80211InfraProtocol + ioctls
AirportMtwlmEthernetInterface.* Sonoma BSD-facing IOEthernetInterface
IOPCIEDeviceWrapper.*     Sonoma PCI shim, owns the HAL service
ItlMwx.cpp/.hpp           HAL service: interrupt source, power, fw info
mwx_glue.cpp              printf/free/sleep, firmware inflate, mbuf clusters
hal_mwx/                  OpenBSD driver body + structs + compat header
include/                  vendored HAL/Airport headers, FwData, firmware
itl80211/                 vendored net80211, crypto and BSD shims
firmware/                 raw MediaTek images (inputs to the embedding)
scripts/syntax-check.sh   build every TU on Linux (no Xcode needed)
```

Sources are compiled as C++ — that is what the reference AirportItlwm
project does, because the vendored net80211 headers have no `extern "C"`
guards.  The one exception is `kmod_info.c`, which must stay C: it
defines the unmangled `_kmod_info` symbol the kext loader and OpenCore's
prelinker look for, and `_start`/`_stop` must keep C linkage to bind to
libkmod.  Its version string comes from `VERSION` in the Makefile.

The link embeds the selected `Info.plist` as `__TEXT,__info_plist`, the
same way Xcode does for a kext target.

`scripts/syntax-check.sh [V2=1] [OSVER=13]` compiles every translation
unit to a Mach-O object with a stock clang, checks for duplicate or
unresolved internal symbols, and validates the plist.  It cannot link a
kext, but it catches most porting mistakes without a Mac.

## Debugging

```sh
# driver log lines
log show --last 2m --predicate 'process == "kernel"' | grep -i mwx

# kext load diagnostics
sudo dmesg | tail -50
kmutil showloaded --list-only | grep -i mtwlm
```

Driver messages are prefixed with `mwx0:`.  The driver body is built
with `MWX_DEBUG 1` (same default as OpenBSD's `mwx(4)`), so `DPRINTF()`
is active in both configurations; set it to `0` in
`hal_mwx/if_mwx.c` for a quiet production build.  `CONFIG=Debug` only
changes optimisation level.

## Status

The driver works on real hardware.  Development and testing have been
on an MT7922 (RZ616, PCI ID `14c3:0616`) with the `V2=1` (Sonoma)
build: the card attaches, scans, associates to a WPA2 network,
completes the 4-way handshake, obtains a DHCP lease and passes traffic
in both directions.  The classic (Big Sur – Ventura) controller and the
MT7921/MT7925 code paths compile but have not been exercised on
hardware by the author.

The station now advertises VHT when the channel supports it and hands
the firmware the peer's HT/VHT capability records, so an 11ac AP is
used at its negotiated width (up to 80 MHz) instead of falling back to
an HT-rate link.  This lives entirely in the shared `hal_mwx/` code, so
the classic and Sonoma builds behave identically.

Known limitations:

* TX aggregation (ADDBA/A-MPDU) is still disabled.  VHT and 80 MHz are
  negotiated, but without aggregation the peak throughput remains below
  what the link rate suggests; enabling it is the next step.
* While associated, cache scans requested by `airportd` are refused so
  the radio is never pulled off-channel mid-connection.  The link stays
  stable, but the Wi-Fi menu's network list is not refreshed until the
  connection is dropped.
* MT7925 header-translated receive frames are dropped; if the firmware
  enables translation in station mode this needs a reverse-translate
  path.
* Sleep/wake is wired through `mwx_activate()` but untested.
* 6 GHz is not supported.

## Licence

AirportMtwlm as a whole is distributed under the GNU General Public
License, version 2 — see [LICENSE](LICENSE).

The controller, interface glue and net80211/BSD compatibility layer are
derived from [itlwm](https://github.com/OpenIntelWireless/itlwm)
(Copyright (C) 2020 钟先耀, GPLv2).  The driver body in `hal_mwx/` is
derived from OpenBSD's `mwx(4)` (Copyright (c) 2021-2022 Claudio Jeker,
MediaTek, Lorenzo Bianconi, Stefan Sperling; ISC licence), which is
compatible with the GPL; its per-file notice is retained.  The
MediaTek firmware images in `firmware/` remain under their own
licence terms.
