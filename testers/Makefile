#$Id: Makefile,v 1.2 2004/05/01 14:53:48 morsiani Exp morsiani $
# Makefile for mipsel-linux
#
XT_PRG_PREFIX = riscv64-unknown-elf-
CC = $(XT_PRG_PREFIX)gcc
LD = $(XT_PRG_PREFIX)ld
AS = $(XT_PRG_PREFIX)as
OBJCOPY = $(XT_PRG_PREFIX)objcopy
OBJDUMP = $(XT_PRG_PREFIX)objdump

# uRISCV-related paths: kept in sync with the values already proven to
# work in the top-level CMakeLists.txt (URISCV_SRC / URISCV_INC).
URISCV_DATA_DIR = /usr/local/share/uriscv
URISCV_INCLUDE_DIR = /usr/include
URISCV_LIB_DIR = /usr/local/lib/uriscv

TDEFS = h/print.h h/tconst.h $(URISCV_INCLUDE_DIR)/uriscv/liburiscv.e Makefile

# CFLAGS = -ffreestanding -ansi -c -mips1 -mabi=32 -mfp32 -mno-gpopt -G 0 -I$(URISCV_INCLUDE_DIR) -std=gnu99 -fno-pic -mno-abicalls
CFLAGS_LANG = -ffreestanding -static -c -nostdlib
CFLAGS = $(CFLAGS_LANG) -I$(URISCV_INCLUDE_DIR) -Wall -O0 -march=rv32imafd -mabi=ilp32d
# -Wall

# NOTE: the original (2004-era, MIPS) template linked against precompiled
# crti.o/liburiscv.o via raw `ld -G 0 ...`. On this uRISC-V install there
# is no precompiled lib/ directory at all (mirrors how the kernel's own
# CMakeLists.txt works): the C runtime startup and the library are
# distributed as assembly *sources* (crtsi.S, liburiscv.S) next to the
# linker scripts in URISCV_DATA_DIR, and are compiled/linked together with
# gcc itself (not invoked separately via `ld`).
CRTSI = $(URISCV_DATA_DIR)/crtsi.S
LIBURISCV_S = $(URISCV_DATA_DIR)/liburiscv.S

LDAOUTFLAGS = -nostdlib -static -T $(URISCV_DATA_DIR)/uriscvaout.ldscript -march=rv32imafd -mabi=ilp32d

EF = uriscv-elf2uriscv
UDEV = uriscv-mkdev

# main target
all: shell.uriscv fibEight.uriscv echo.uriscv fibEleven.uriscv uname.uriscv calc.uriscv date.uriscv sl.uriscv

%.o: %.c $(TDEFS)
	$(CC) $(CFLAGS) $<

%.t: %.o print.o
	$(CC) $(LDAOUTFLAGS) $(CRTSI) $< print.o $(LIBURISCV_S) -o $@

%.t.aout.uriscv: %.t
	$(EF) -a $<

%.uriscv: %.t.aout.uriscv
	$(UDEV) -f $@ $<

clean:
	rm -f *.o *.t *.uriscv

