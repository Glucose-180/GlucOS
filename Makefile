# -----------------------------------------------------------------------
# Project Information
# -----------------------------------------------------------------------

PROJECT_IDX	= 6
VERSION_IDX = 0

OS_NAME = GlucOS
USER_NAME = glucose180

# -----------------------------------------------------------------------
# Host Linux Variables
# -----------------------------------------------------------------------

SHELL       = /bin/sh
DISK        = /dev/sdb
TTYUSB1     = /dev/ttyUSB1
DIR_OSLAB   = $(HOME)/os-2023-tools
DIR_QEMU    = $(DIR_OSLAB)/qemu
DIR_UBOOT   = $(DIR_OSLAB)/u-boot

# -----------------------------------------------------------------------
# FPGA and minicom variables
# -----------------------------------------------------------------------
# delay after "make minicom"
MC_DELAY	= 13
FPGA_LOG_FILE   = ./glucos-fpga.log

# -----------------------------------------------------------------------
# Build and Debug Tools
# -----------------------------------------------------------------------

HOST_CC         = gcc
CROSS_PREFIX    = riscv64-unknown-elf-
CC              = $(CROSS_PREFIX)gcc
AR              = $(CROSS_PREFIX)ar
OBJDUMP         = $(CROSS_PREFIX)objdump
GDB             = $(CROSS_PREFIX)gdb
QEMU            = $(DIR_QEMU)/riscv64-softmmu/qemu-system-riscv64
UBOOT           = $(DIR_UBOOT)/u-boot
MINICOM         = minicom

# -----------------------------------------------------------------------
# Build/Debug Flags and Variables
# -----------------------------------------------------------------------
# Whether debug mode is enabled in GlucOS
DEBUG		= 1
# Whether sys_yield() is invoked in user process
YIELD_EN		= 0
# Whether multithreading is supported in GlucOS
MTHREAD			= 1
# Timer interval (ms) used for scheduler
TINTERVAL		= 10
# Number of CPU
NCPU			= 2
# Number of pages in swap partition
NPSWAP			= 128
# Other flags
CFOTHER			= -DNO_OTHER_FLAGS

CFLAGS0         = -fno-builtin -nostdlib -nostdinc -Wall -mcmodel=medany -DOS_NAME=\"$(OS_NAME)\" -DUSER_NAME=\"$(USER_NAME)\" -fmax-errors=5 $(CFOTHER)

ifneq ($(DEBUG), 0)
	CFLAGS		= $(CFLAGS0) -ggdb3 -DDEBUG_EN=1 -O0
else
	CFLAGS		= $(CFLAGS0) -DDEBUG_EN=0 -O2
endif

BOOT_INCLUDE    = -I$(DIR_ARCH)/include
BOOT_CFLAGS     = $(CFLAGS) $(BOOT_INCLUDE) -Wl,--defsym=TEXT_START=$(BOOTLOADER_ENTRYPOINT) -T riscv.lds

KERNEL_INCLUDE  = -I$(DIR_ARCH)/include -Iinclude -Idrivers
KERNEL_CFLAGS   = $(CFLAGS) $(KERNEL_INCLUDE) -DMULTITHREADING=$(MTHREAD) -DTIMER_INTERVAL_MS=$(TINTERVAL) -DNCPU=$(NCPU) -DNPSWAP=$(NPSWAP) -Wl,--defsym=TEXT_START=$(KERNEL_ENTRYPOINT) -T riscv.lds

USER_INCLUDE    = -I$(DIR_TINYLIBC)/include
USER_CFLAGS     = $(CFLAGS) $(USER_INCLUDE) -DYIELD_EN=$(YIELD_EN)
USER_LDFLAGS    = -L$(DIR_BUILD) -ltinyc

#QEMU_LOG_FILE   = $(DIR_OSLAB)/oslab-log.txt
QEMU_LOG_FILE   = ./glucos-qemu.log
QEMU_OPTS       = -nographic -machine virt -m 256M -kernel $(UBOOT) -bios none \
                     -drive if=none,format=raw,id=image,file=${ELF_IMAGE} \
                     -device virtio-blk-device,drive=image \
                     -monitor telnet::45454,server,nowait -serial mon:stdio \
                     -D $(QEMU_LOG_FILE) -d oslab
QEMU_DEBUG_OPT  = -s -S
QEMU_SMP_OPT	= -smp 2
QEMU_NET_OPT    = -netdev tap,id=mytap,ifname=tap0,script=${DIR_QEMU}/etc/qemu-ifup,downscript=${DIR_QEMU}/etc/qemu-ifdown \
                    -device e1000,netdev=mytap

# -----------------------------------------------------------------------
# UCAS-OS Entrypoints and Variables
# -----------------------------------------------------------------------

DIR_ARCH        = ./arch/riscv
DIR_BUILD       = ./build
DIR_DRIVERS     = ./drivers
DIR_INIT        = ./init
DIR_KERNEL      = ./kernel
DIR_LIBS        = ./libs
DIR_TINYLIBC    = ./tiny_libc
DIR_TEST        = ./test
#DIR_TEST_PROJ   = $(DIR_TEST)/test_project$(PROJECT_IDX)
DIR_TEST_PROJ   = $(DIR_TEST)/test_v$(VERSION_IDX)

BOOTLOADER_ENTRYPOINT   = 0x50200000
KERNEL_ENTRYPOINT       = 0xffffffc050202000
#USER_ENTRYPOINT         = 0x200000
USER_ENTRYPOINT         = 0x10000	# A/C-Core

# -----------------------------------------------------------------------
# UCAS-OS Kernel Source Files
# -----------------------------------------------------------------------

SRC_BOOT    = $(wildcard $(DIR_ARCH)/boot/*.S)
SRC_ARCH    = $(wildcard $(DIR_ARCH)/kernel/*.S)
SRC_BIOS    = $(wildcard $(DIR_ARCH)/bios/*.c)
SRC_DRIVER  = $(wildcard $(DIR_DRIVERS)/*.c)
SRC_INIT    = $(wildcard $(DIR_INIT)/*.c)
SRC_KERNEL  = $(wildcard $(DIR_KERNEL)/*/*.c)
SRC_LIBS    = $(wildcard $(DIR_LIBS)/*.c)
SRC_START   = $(wildcard $(DIR_ARCH)/kernel/*.c)

SRC_MAIN    = $(SRC_ARCH) $(SRC_START) $(SRC_INIT) $(SRC_BIOS) $(SRC_DRIVER) $(SRC_KERNEL) $(SRC_LIBS) 

ELF_BOOT    = $(DIR_BUILD)/bootblock
ELF_MAIN    = $(DIR_BUILD)/main
ELF_IMAGE	= $(HOME)/Ktemp/glucos-img
#ELF_IMAGE   = $(DIR_BUILD)/image

# -----------------------------------------------------------------------
# UCAS-OS User Source Files
# -----------------------------------------------------------------------

SRC_CRT0    = $(wildcard $(DIR_ARCH)/crt0/*.S)
OBJ_CRT0    = $(DIR_BUILD)/$(notdir $(SRC_CRT0:.S=.o))

SRC_LIBC    = $(wildcard ./tiny_libc/*.c)
OBJ_LIBC    = $(patsubst %.c, %.o, $(foreach file, $(SRC_LIBC), $(DIR_BUILD)/$(notdir $(file))))
LIB_TINYC   = $(DIR_BUILD)/libtinyc.a

#SRC_SHELL	= $(DIR_TEST)/shell.c
#SRC_USER    = $(SRC_SHELL) $(wildcard $(DIR_TEST_PROJ)/*.c)
SRC_USER    = $(wildcard $(DIR_TEST_PROJ)/*.c)
ELF_USER    = $(patsubst %.c, %, $(foreach file, $(SRC_USER), $(DIR_BUILD)/$(notdir $(file))))

# -----------------------------------------------------------------------
# Host Linux Tools Source Files
# -----------------------------------------------------------------------

SRC_CREATEIMAGE = ./tools/createimage/createimage.c
ELF_CREATEIMAGE = $(DIR_BUILD)/$(notdir $(SRC_CREATEIMAGE:.c=))

# -----------------------------------------------------------------------
# Top-level Rules
# -----------------------------------------------------------------------

# Glucose180: Do not do objdump
all: dirs elf image #asm  floppy

dirs:
	@mkdir -p $(DIR_BUILD)

clean:
	rm -rf $(DIR_BUILD)

# Glucose180: Wipe data in the SD card
floppy0:
	sudo fdisk -l $(DISK)
	sudo dd if=/dev/zero of=$(DISK)3 conv=notrunc bs=4MiB count=2

floppy:
	sudo fdisk -l $(DISK)
	sudo dd if=$(DIR_BUILD)/image of=$(DISK)3 conv=notrunc

asm: $(ELF_BOOT) $(ELF_MAIN) $(ELF_USER)
	for elffile in $^; do $(OBJDUMP) -d $$elffile > $(notdir $$elffile).txt; done

gdb:
	$(GDB) $(ELF_MAIN) -ex "target remote:1234" -q

run:
	$(QEMU) $(QEMU_OPTS)

run-smp:
	$(QEMU) $(QEMU_OPTS) $(QEMU_SMP_OPT)

run-net:
	-@sudo kill `sudo lsof | grep tun | awk '{print $$2}'`
	sudo $(QEMU) $(QEMU_OPTS) $(QEMU_NET_OPT) $(QEMU_SMP_OPT)

debug:
	$(QEMU) $(QEMU_OPTS) $(QEMU_DEBUG_OPT)

debug-smp:
	$(QEMU) $(QEMU_OPTS) $(QEMU_SMP_OPT) $(QEMU_DEBUG_OPT)

debug-net:
	-@sudo kill `sudo lsof | grep tun | awk '{print $$2}'`
	sudo $(QEMU) $(QEMU_OPTS) $(QEMU_DEBUG_OPT) $(QEMU_NET_OPT) $(QEMU_SMP_OPT)

viewlog:
	@if [ ! -e $(QEMU_LOG_FILE) ]; then touch $(QEMU_LOG_FILE); fi;
	@tail -f $(QEMU_LOG_FILE)

minicom:
	@echo "Delay $(MC_DELAY) s to skip long text..."
	sleep $(MC_DELAY)
	sudo $(MINICOM) -D $(TTYUSB1) -X $(FPGA_LOG_FILE)

.PHONY: all dirs clean floppy0 floppy asm gdb run debug viewlog minicom run-net debug-net

# -----------------------------------------------------------------------
# UCAS-OS Rules
# -----------------------------------------------------------------------

$(ELF_BOOT): $(SRC_BOOT) riscv.lds
	$(CC) $(BOOT_CFLAGS) -o $@ $(SRC_BOOT) -e main

$(ELF_MAIN): $(SRC_MAIN) riscv.lds
	$(CC) $(KERNEL_CFLAGS) -o $@ $(SRC_MAIN) -e _boot

$(OBJ_CRT0): $(SRC_CRT0)
	$(CC) $(USER_CFLAGS) -I$(DIR_ARCH)/include -c $< -o $@

$(LIB_TINYC): $(OBJ_LIBC)
	$(AR) rcs $@ $^

$(DIR_BUILD)/%.o: $(DIR_TINYLIBC)/%.c
	$(CC) $(USER_CFLAGS) -c $< -o $@

#$(DIR_BUILD)/%: $(DIR_TEST_PROJ)/%.c $(OBJ_CRT0) $(LIB_TINYC) riscv.lds
#	$(CC) $(USER_CFLAGS) -o $@ $(OBJ_CRT0) $< $(USER_LDFLAGS) -Wl,--defsym=TEXT_START=$(USER_ENTRYPOINT) -T riscv.lds
#	$(eval USER_ENTRYPOINT := $(shell python3 -c "print(hex(int('$(USER_ENTRYPOINT)', 16) + int('0x10000', 16)))"))

$(DIR_BUILD)/%: $(DIR_TEST_PROJ)/%.c $(OBJ_CRT0) $(LIB_TINYC) riscv.lds
	$(CC) $(USER_CFLAGS) -o $@ $(OBJ_CRT0) $< $(USER_LDFLAGS) -Wl,--defsym=TEXT_START=$(USER_ENTRYPOINT) -T riscv.lds

elf: $(ELF_BOOT) $(ELF_MAIN) $(LIB_TINYC) $(ELF_USER)

.PHONY: elf

# -----------------------------------------------------------------------
# Host Linux Rules
# -----------------------------------------------------------------------

$(ELF_CREATEIMAGE): $(SRC_CREATEIMAGE)
	$(HOST_CC) $(SRC_CREATEIMAGE) -o $@ -ggdb -Wall
	
image: $(ELF_CREATEIMAGE) $(ELF_BOOT) $(ELF_MAIN) $(ELF_USER)
	cd $(DIR_BUILD) && ./$(<F) --extended $(filter-out $(<F), $(^F)) 

.PHONY: image

swap:
	dd if=/dev/zero of=build/image oflag=append conv=notrunc bs=4KiB count=$(NPSWAP)
# Add one more page as the last page may not be complete
	dd if=/dev/zero of=build/image oflag=append conv=notrunc bs=4KiB count=1

.PHONY: swap

gfs:
	cp $(DIR_BUILD)/image $(ELF_IMAGE)
	dd if=/dev/zero of=$(ELF_IMAGE) oflag=append conv=notrunc bs=4KiB count=$(NPSWAP)
	dd if=/dev/zero of=$(ELF_IMAGE) oflag=append conv=notrunc bs=4KiB count=1
	dd if=/dev/zero of=$(ELF_IMAGE) oflag=append conv=notrunc bs=4MiB count=10

.PHONY: gfs