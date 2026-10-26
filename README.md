# AirportMtwlm

**A native macOS Wi-Fi driver for MediaTek MT7921, MT7922/RZ616 and
MT7925/RZ717, based on OpenBSD's `mwx(4)`.**

AirportMtwlm is an `IO80211Controller`, so macOS treats the card like a
built-in Apple wireless adapter. It appears in the menu bar Wi-Fi picker
and uses the system's own supplicant — no Heliport and no user-space
helper.

## Supported hardware

| PCI ID  | Chip                   |
|---------|------------------------|
| `0x7961` | MT7921                |
| `0x0608` | MT7921K               |
| `0x7920` | MT7920                |
| `0x7922` | MT7922                |
| `0x0616` | RZ616 (MT7922)        |
| `0x7925` | MT7925                |
| `0x0717` | RZ717 (MT7925)        |

2.4 GHz and 5 GHz are supported, including 80 MHz VHT on 5 GHz and
160 MHz on MT7922/MT7925. 6 GHz is not supported.

## Download

Grab `AirportMtwlm.kext` from the
[latest release](https://github.com/HackSucks/mtwlm/releases/latest).

The uploaded bundle is an x86_64 build made with Xcode 15.4. It should
work from Big Sur through Sonoma; I have not tested Sequoia or Tahoe.

## Installation (OpenCore)

Installing a Wi-Fi kext into `/Library/Extensions` is not recommended on
a Hackintosh. Inject it through OpenCore instead:

1. Copy `AirportMtwlm.kext` to `EFI/OC/Kexts/`.
2. Add the kext to your `config.plist`. In ProperTree, the **OC Refresh**
   option adds the `Kernel > Add` entry for you, so you do not have to
   create one by hand.
3. Reboot.

Keep exactly one copy of the driver. Do not also put it in
`/Library/Extensions`.

If the card does not appear, or does not discover your network after it
loads, toggle Wi-Fi off and on from the menu bar. That is usually enough
to make the card scan.

OpenCore injection does not require an Apple-signed kext, but a boot
policy that forbids third-party code will still block it. If the kext
never shows up in `kmutil showloaded`, check `csr-active-config` and
`SecureBootModel`.

## Building

`MacKernelSDK` is included as a submodule, so clone recursively:

```sh
git clone --recursive https://github.com/HackSucks/mtwlm.git
cd mtwlm
```

IO80211Family changed shape in Sonoma, so there are two builds of the
same driver:

| macOS             | Command     |
|-------------------|-------------|
| Big Sur – Ventura | `make`      |
| Sonoma 14 and newer | `make V2=1` |

```sh
make                     # classic, host architecture
make OSVER=13            # classic, Ventura API level
make V2=1                # Sonoma and newer
make ARCH=arm64          # Apple silicon
```

Both produce `AirportMtwlm.kext`, so build one variant at a time. The
classic build uses Apple's supplicant; the Sonoma build does the RSN
handshake in-kernel.

`scripts/syntax-check.sh [V2=1]` compiles every translation unit with a
stock clang on Linux and checks for duplicate or unresolved symbols. I
use it to catch porting mistakes without a Mac.

## Status

The driver works on real hardware. I have tested it on an MT7922 (RZ616,
`14c3:0616`) with the Sonoma build: the card attaches, scans, associates
to a WPA2 network, completes the 4-way handshake, gets a DHCP lease and
passes traffic in both directions. The classic controller and the
MT7921/MT7925 paths compile, but I have not run them on hardware.

VHT is advertised when the channel supports it and the firmware receives
the peer's HT/VHT records, so an 11ac AP is used at its negotiated width
(up to 80 MHz) instead of falling back to HT. This lives in the shared
`hal_mwx/` code, so the classic and Sonoma builds behave the same.

Known limitations:

* TX aggregation (A-MPDU/ADDBA) is still disabled, so peak throughput is
  below what the link rate suggests.
* While associated, cache scans requested by `airportd` are refused. The
  link stays stable, but the Wi-Fi menu's network list is not refreshed
  until the connection drops.
* MT7925 header-translated receive frames are dropped; if the firmware
  enables translation in station mode, this needs a reverse-translate
  path.
* Sleep/wake is wired through `mwx_activate()` but untested.
* 6 GHz is not supported.

## Debugging

```sh
log show --last 2m --predicate 'process == "kernel"' | grep -i mwx
kmutil showloaded --list-only | grep -i mtwlm
```

Driver messages are prefixed with `mwx0:`. `MWX_DEBUG` is `1` by
default, so `DPRINTF()` is active; set it to `0` in `hal_mwx/if_mwx.c`
for a quiet build.

## Credits

- [OpenBSD](https://openbsd.org) for `mwx(4)` and net80211
- [itlwm](https://github.com/OpenIntelWireless/itlwm) for the macOS
  IO80211 glue and the net80211/BSD compatibility layer
- [Acidanthera](https://github.com/acidanthera) for
  [MacKernelSDK](https://github.com/acidanthera/MacKernelSDK)
- [Apple](https://www.apple.com) for macOS

## License

AirportMtwlm is distributed under the GNU General Public License,
version 2 — see [LICENSE](LICENSE).

The driver body in `hal_mwx/` is derived from OpenBSD's `mwx(4)` and
keeps its ISC notice. The firmware images in `firmware/` remain under
their own terms.
