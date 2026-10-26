# ---------------------------------------------------------------------
# AirportMtwlm -- MediaTek MT792x 802.11ax driver for macOS
#
# Builds a native IO80211Family kext, so the card appears in the system
# Wi-Fi menu the same way Apple's own drivers do (no Heliport).
#
#   make                build AirportMtwlm.kext for the host architecture
#   make ARCH=arm64     build for Apple silicon
#   make CONFIG=Debug   unoptimised build with debug prints
#   make V2=1           build the Sonoma 14.0+ (Skywalk) variant
#   make OSVER=13       classic variant, Ventura API level (default 11)
#   make clean
#   make load           load the kext (SIP must allow it)
#   make unload
#   make install        copy into /Library/Extensions and rebuild caches
#
# There are two mutually exclusive controller implementations, because
# IO80211Family changed shape in Sonoma:
#
#   V2=0 (default)  Big Sur .. Ventura   IO80211Controller + IO80211Interface
#   V2=1            Sonoma 14.0 .. 15+   IO80211Controller + Skywalk
#
# Both produce the same kext identifier and IOClass name; only the source
# set and Info.plist differ, so a system only ever loads the matching one.
#
# Requirements: Xcode command line tools, MacKernelSDK checked out at
# ../MacKernelSDK (or pass MKS=/path/to/MacKernelSDK).
# ---------------------------------------------------------------------

PRODUCT  := AirportMtwlm
BUNDLE   := $(PRODUCT).kext
PKGID    := com.nvme.AirportMtwlm
VERSION  := 1.0.0

MKS      ?= MacKernelSDK
ARCH     ?= $(shell uname -m)
CONFIG   ?= Release
V2       ?= 0
# Classic variant's API level: 11 (Big Sur, default), 12 or 13.
OSVER    ?= 11

# The vendored net80211 tree is compiled as C++, exactly like the
# reference AirportItlwm project (GCC_INPUT_FILETYPE=sourcecode.cpp.cpp).
ifeq ($(ARCH),arm64)
TARGET   := arm64-apple-macos11.0
else
TARGET   := x86_64-apple-macos10.15
endif

XC       := xcrun -sdk macosx
CXX      := $(XC) clang++
CC       := $(XC) clang

B      := build
ifeq ($(V2),1)
OBJDIR := $(B)/v2/obj
INFOPLIST := Info-V2.plist
STAMP := $(B)/.variant-sonoma
else
OBJDIR := $(B)/obj-$(OSVER)
INFOPLIST := Info.plist
STAMP := $(B)/.variant-classic-$(OSVER)
endif
BINARY := $(BUNDLE)/Contents/MacOS/$(PRODUCT)

# ---------------------------------------------------------------------
# sources
#
# SRCS below is everything except the controller/interface pair; those
# are selected per variant at the end of this block.
# ---------------------------------------------------------------------

SRCS := \
	kmod_info.c \
	ItlMwx.cpp \
	mwx_glue.cpp \
	hal_mwx/if_mwx.c \
	include/HAL/ItlHalService.cpp \
	include/MtwlmFwBinary.cpp \
	itl80211/compat.cpp \
	itl80211/zutil.c \
	itl80211/openbsd/net80211/CTimeout.cpp \
	itl80211/openbsd/net80211/_string.c \
	itl80211/openbsd/net80211/timeout.c \
	itl80211/openbsd/net80211/ieee80211.c \
	itl80211/openbsd/net80211/ieee80211_amrr.c \
	itl80211/openbsd/net80211/ieee80211_crypto.c \
	itl80211/openbsd/net80211/ieee80211_crypto_bip.c \
	itl80211/openbsd/net80211/ieee80211_crypto_ccmp.c \
	itl80211/openbsd/net80211/ieee80211_crypto_tkip.c \
	itl80211/openbsd/net80211/ieee80211_crypto_wep.c \
	itl80211/openbsd/net80211/ieee80211_input.c \
	itl80211/openbsd/net80211/ieee80211_ioctl.c \
	itl80211/openbsd/net80211/ieee80211_mira.c \
	itl80211/openbsd/net80211/ieee80211_node.c \
	itl80211/openbsd/net80211/ieee80211_output.c \
	itl80211/openbsd/net80211/ieee80211_pae_input.c \
	itl80211/openbsd/net80211/ieee80211_pae_output.c \
	itl80211/openbsd/net80211/ieee80211_proto.c \
	itl80211/openbsd/net80211/ieee80211_ra.c \
	itl80211/openbsd/net80211/ieee80211_regdomain.c \
	itl80211/openbsd/net80211/ieee80211_rssadapt.c \
	itl80211/openbsd/sys/_ifq.cpp \
	itl80211/openbsd/sys/_mbuf.cpp \
	itl80211/openbsd/sys/_task.cpp \
	itl80211/openbsd/sys/arp.c \
	itl80211/openbsd/crypto/aes.c \
	itl80211/openbsd/crypto/arc4.c \
	itl80211/openbsd/crypto/blf.c \
	itl80211/openbsd/crypto/cast.c \
	itl80211/openbsd/crypto/chachapoly.c \
	itl80211/openbsd/crypto/cmac.c \
	itl80211/openbsd/crypto/ecb3_enc.c \
	itl80211/openbsd/crypto/ecb_enc.c \
	itl80211/openbsd/crypto/gmac.c \
	itl80211/openbsd/crypto/hmac.c \
	itl80211/openbsd/crypto/idgen.c \
	itl80211/openbsd/crypto/key_wrap.c \
	itl80211/openbsd/crypto/md5.c \
	itl80211/openbsd/crypto/michael.c \
	itl80211/openbsd/crypto/poly1305.c \
	itl80211/openbsd/crypto/rijndael.c \
	itl80211/openbsd/crypto/rmd160.c \
	itl80211/openbsd/crypto/set_key.c \
	itl80211/openbsd/crypto/sha1.c \
	itl80211/openbsd/crypto/sha1-pbkdf2.c \
	itl80211/openbsd/crypto/sha2.c

# The controller/interface pair is the only thing that differs between
# the pre-Sonoma and Sonoma+ builds.
ifeq ($(V2),1)
SRCS += \
	AirportMtwlmV2.cpp \
	AirportMtwlmSkywalkInterface.cpp \
	AirportMtwlmEthernetInterface.cpp \
	IOPCIEDeviceWrapper.cpp
else
SRCS += \
	AirportMtwlm.cpp \
	AirportMtwlmInterface.cpp \
	AirportMtwlmSTAIOCTL.cpp \
	AirportMtwlmVirtualIOCTL.cpp \
	AirportMtwlmAWDL.cpp
endif

OBJS := $(addprefix $(OBJDIR)/,$(SRCS:.c=.o))
OBJS := $(OBJS:.cpp=.o)

# ---------------------------------------------------------------------
# flags
#
# AIRPORT / USE_APPLE_SUPPLICANT make the vendored net80211 hand RSN
# processing to Apple's supplicant instead of doing its own 4-way
# handshake, which is what IO80211Family expects.
# ---------------------------------------------------------------------

DEFINES := \
	-DKERNEL -DKERNEL_PRIVATE \
	-D__APPLE__ \
	-D__PRIVATE_SPI__ \
	-DAIRPORT \
	-DIEEE80211_STA_ONLY \
	-D__MAC_11_0=110000 -D__MAC_12_0=120000 \
	-D__MAC_13_0=130000 -D__MAC_14_0=140000 -D__MAC_14_4=140400

ifeq ($(V2),1)
# Sonoma+: Skywalk controller/interface.  The reference build does not
# define USE_APPLE_SUPPLICANT here; net80211 does its own RSN handshake
# because the Skywalk supplicant path is different.
DEFINES += -DIO80211FAMILY_V2 -D__IO80211_TARGET=__MAC_14_4
else
# Big Sur .. Ventura: Apple's supplicant handles RSN.
DEFINES += -DUSE_APPLE_SUPPLICANT
DEFINES += -D__IO80211_TARGET=__MAC_$(OSVER)_0
endif

INCLUDES := \
	-I$(MKS)/Headers \
	-I. \
	-Iinclude \
	-Iinclude/Airport \
	-Iinclude/HAL \
	-Ihal_mwx \
	-Iitl80211 \
	-Iitl80211/openbsd \
	-Iitl80211/linux

WARNINGS := \
	-Wall \
	-Wno-unused-parameter \
	-Wno-unused-variable \
	-Wno-unused-function \
	-Wno-sign-compare \
	-Wno-missing-field-initializers \
	-Wno-c++11-narrowing \
	-Wno-deprecated-register \
	-Wno-unknown-pragmas \
	-Wno-address-of-packed-member \
	-Wno-parentheses \
	-Wno-unused-value \
	-Wno-format

CPPFLAGS := $(DEFINES) $(INCLUDES)
BASE_FLAGS := -target $(TARGET) -mkernel -fno-builtin -fno-common \
	-fno-stack-protector
CXXFLAGS := $(BASE_FLAGS) -fno-exceptions -fno-rtti -nostdinc++ \
	-std=gnu++14 $(WARNINGS)
# kmod_info.c is the only C translation unit (see the rule below); it must
# not see -std=gnu++14 / -fno-rtti.
CFLAGS := $(BASE_FLAGS) -DMTWLM_KMOD_VERSION='"$(VERSION)"'

ifeq ($(CONFIG),Debug)
CXXFLAGS += -O0 -g -DDEBUG=1
CFLAGS   += -O0 -g -DDEBUG=1
else
CXXFLAGS += -O2 -g
CFLAGS   += -O2 -g
endif

# libkmod supplies the kext start/stop glue. MacKernelSDK ships an
# x86_64 build that also targets older kernels; Apple silicon links
# against the SDK's own copy.
KMOD_DIR := $(MKS)/Library/$(ARCH)
ifeq ($(wildcard $(KMOD_DIR)/libkmod.a),)
KMOD_DIR := $(shell $(XC) --show-sdk-path)/usr/lib
endif

LDFLAGS := -target $(TARGET) -mkernel -nostdlib \
	-Wl,-kext \
	-Wl,-undefined,dynamic_lookup \
	-Wl,-no_compact_unwind \
	-Wl,-sectcreate,__TEXT,__info_plist,$(INFOPLIST)
LDLIBS  := -L$(KMOD_DIR) -lkmod

# ---------------------------------------------------------------------
# rules
# ---------------------------------------------------------------------

.DEFAULT_GOAL := all

.PHONY: all clean load unload install check sign print-cxxflags print-srcs print-ldflags print-infoplist

# Introspection targets, used by scripts/syntax-check.sh on non-macOS
# hosts.  They print what the build would use without running xcrun.
print-cxxflags:
	@echo $(CXXFLAGS) $(CPPFLAGS)
print-cflags:
	@echo $(CFLAGS) $(CPPFLAGS)
print-srcs:
	@echo $(SRCS)
print-ldflags:
	@echo $(LDFLAGS) $(LDLIBS)
print-infoplist:
	@echo $(INFOPLIST)

all: sign
	@echo "built $(BUNDLE)"

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

# .c sources are built as C++: the vendored net80211 headers have no
# extern "C" guards and the driver body relies on C++ type checking.
$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -x c++ -c $< -o $@

# kmod_info.c is compiled as C on purpose.  Everything else is C++ for
# the vendored net80211 headers, but this file must define the unmangled
# `_kmod_info` symbol and keep `_start`/`_stop` in C linkage so they bind
# to libkmod.  Without it the kext links (dynamic_lookup) but has no
# kmod_info record, and OpenCore refuses to prelink it.
$(OBJDIR)/kmod_info.o: kmod_info.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BINARY): $(OBJS) $(STAMP) $(INFOPLIST)
	@mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

# Both variants build the same $(BINARY) path, and make would happily keep
# a binary that merely looks newer than the other variant's objects.  A
# stamp per variant forces the relink when the configuration changes.
$(STAMP):
	@mkdir -p $(B)
	@rm -f $(B)/.variant-*
	@touch $@

$(BUNDLE)/Contents/Info.plist: $(INFOPLIST) FORCE
	@mkdir -p $(dir $@)
	@cp -f $(INFOPLIST) $@
	@plutil -lint $@ >/dev/null

# Both variants share one bundle name, so the plist must be re-copied even
# when the other variant's plist is newer than the destination.
.PHONY: FORCE
FORCE:

sign: $(BINARY) $(BUNDLE)/Contents/Info.plist
	@codesign --force --sign - --timestamp=none $(BUNDLE) 2>/dev/null || \
	    echo "warning: ad-hoc codesign failed; kext may not load"

check: all
	kextutil -n -t $(BUNDLE)

load: all
	sudo kmutil load -p $(BUNDLE)

unload:
	sudo kmutil unload -b $(PKGID)

install: all
	sudo rm -rf /Library/Extensions/$(BUNDLE)
	sudo cp -R $(BUNDLE) /Library/Extensions/
	sudo chown -R root:wheel /Library/Extensions/$(BUNDLE)
	sudo kmutil install --update-all

clean:
	rm -rf $(B) $(BUNDLE)
