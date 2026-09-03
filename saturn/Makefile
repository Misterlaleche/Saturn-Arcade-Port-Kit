THIS_ROOT:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

ifeq ($(strip $(YAUL_INSTALL_ROOT)),)
  $(error Undefined YAUL_INSTALL_ROOT (install root directory))
endif

include $(YAUL_INSTALL_ROOT)/share/build.pre.mk

BUILTIN_ASSETS=

SH_PROGRAM:= saturn-arcade-port-kit
SH_SRCS:= \
	src/main.c \
	src/portkit.c

SH_CFLAGS+= -O2 -Iinclude -Wall -Wextra
SH_LDFLAGS+=

IP_VERSION:= V0.100
IP_RELEASE_DATE:= 20260903
IP_AREAS:= JTUBKAEL
IP_PERIPHERALS:= JAMKST
IP_TITLE:= Saturn Arcade Port Kit v0.1
IP_MASTER_STACK_ADDR:= 0x06004000
IP_SLAVE_STACK_ADDR:= 0x06001E00
IP_1ST_READ_ADDR:= 0x06004000
IP_1ST_READ_SIZE:= 0

include $(YAUL_INSTALL_ROOT)/share/build.post.iso-cue.mk
