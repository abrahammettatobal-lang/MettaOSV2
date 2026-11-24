# MettaOS — freestanding x86_64 build (SA-03)
# Compila boot/*.S y fuentes kernel/**/*.c cuando existan.

SHELL := /bin/bash
ROOT  := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

BUILD_DIR := $(ROOT)/build
OBJ_DIR   := $(BUILD_DIR)/obj
ISO_DIR   := $(BUILD_DIR)/isodir
BIN       := $(BUILD_DIR)/mettaos.bin
ISO       := $(BUILD_DIR)/mettaos.iso

CC   := gcc
LD   := ld
NASM := nasm

ARCH_CFLAGS := -m64 -mcmodel=kernel -mno-red-zone -mno-sse -mno-sse2 \
               -mno-mmx -mno-80387 -mno-avx

CFLAGS := -std=gnu11 -ffreestanding -fno-stack-protector -fno-pic \
          -fno-builtin -fno-asynchronous-unwind-tables \
          -include stddef.h \
          -Wall -Wextra -Wpedantic -O2 $(ARCH_CFLAGS) \
          -I$(ROOT)/kernel/include \
          -I$(ROOT)/kernel/security/include \
          -I$(ROOT)/mde/include \
          -I$(ROOT)/mde/theme \
          -I$(ROOT)/kernel/drivers/block

# 1 = enlazar escritorio MDE; 0 = solo boot + kernel (make kernel-only)
BUILD_MDE ?= 1

# MDE usa float (animaciones); el resto del kernel sigue sin SSE/AVX.
MDE_ARCH_CFLAGS := -m64 -mcmodel=kernel -mno-red-zone -msse -msse2 -mno-avx
MDE_CFLAGS := -std=gnu11 -ffreestanding -fno-stack-protector -fno-pic \
              -fno-builtin -fno-asynchronous-unwind-tables \
              -include stddef.h \
              -Wall -Wextra -Wpedantic -O2 $(MDE_ARCH_CFLAGS) \
              -I$(ROOT)/kernel/include \
              -I$(ROOT)/kernel/security/include \
              -I$(ROOT)/mde/include \
              -I$(ROOT)/mde/theme \
              -I$(ROOT)/kernel/drivers/block

ASFLAGS := -f elf64

LDFLAGS := -nostdlib -static -z max-page-size=0x1000 \
           -T $(ROOT)/linker.ld

BOOT_SRCS := $(sort $(wildcard $(ROOT)/boot/*.S))
KERNEL_SRCS := $(wildcard $(ROOT)/kernel/*.c) \
               $(wildcard $(ROOT)/kernel/*/*.c) \
               $(wildcard $(ROOT)/kernel/*/*/*.c)
# userland/ se enlaza con gcc del host y entra por incbin, no como objeto del kernel.
KERNEL_SRCS := $(foreach src,$(KERNEL_SRCS),$(if $(findstring /linux/userland/,$(subst \,/,$(src))),,$(src)))

BOOT_OBJS := $(patsubst $(ROOT)/boot/%.S,$(OBJ_DIR)/boot/%.o,$(BOOT_SRCS))
KERNEL_OBJS := $(patsubst $(ROOT)/kernel/%.c,$(OBJ_DIR)/kernel/%.o,$(KERNEL_SRCS))

# mde/**/*.c sin mde/apps/* (userspace / stubs hasta más adelante)
MDE_ALL := $(wildcard $(ROOT)/mde/*.c) \
           $(wildcard $(ROOT)/mde/*/*.c) \
           $(wildcard $(ROOT)/mde/*/*/*.c) \
           $(wildcard $(ROOT)/mde/*/*/*/*.c)
MDE_SRCS := $(foreach src,$(MDE_ALL),$(if $(findstring mde/apps/,$(subst \,/,$(src))),,$(src)))
MDE_OBJS := $(patsubst $(ROOT)/mde/%.c,$(OBJ_DIR)/mde/%.o,$(MDE_SRCS))

# multiboot2.o debe ir primero en el enlace (provee _start y cabecera)
BOOT_LINK_FIRST := $(OBJ_DIR)/boot/multiboot2.o
BOOT_LINK_REST := $(filter-out $(BOOT_LINK_FIRST),$(BOOT_OBJS))
USER_BLOB := $(OBJ_DIR)/kernel/linux/user_blob.o
HOSTLIBS_OBJ := $(OBJ_DIR)/kernel/linux/hostlibs.gen.o
KERNEL_LINK_OBJS := $(BOOT_LINK_FIRST) $(BOOT_LINK_REST) $(KERNEL_OBJS) $(USER_BLOB) $(HOSTLIBS_OBJ)

$(BUILD_DIR)/user/hello.elf: $(ROOT)/kernel/linux/userland/hello.asm $(ROOT)/kernel/linux/userland/user.ld
	@mkdir -p $(BUILD_DIR)/user
	$(NASM) -f elf64 -o $(BUILD_DIR)/user/hello.o $<
	$(LD) -nostdlib -static -T $(ROOT)/kernel/linux/userland/user.ld -o $@ $(BUILD_DIR)/user/hello.o

# Hello dinámico del host. Sin $(CFLAGS): son freestanding y rompen el PIE.
$(BUILD_DIR)/user/dynhello: $(ROOT)/kernel/linux/userland/dynhello.c
	@mkdir -p $(BUILD_DIR)/user
	gcc -O2 -o $@ $<

$(BUILD_DIR)/user/ldlinux.so:
	@mkdir -p $(BUILD_DIR)/user
	cp -L /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 $@

$(BUILD_DIR)/user/libc.so:
	@mkdir -p $(BUILD_DIR)/user
	cp -L /usr/lib/x86_64-linux-gnu/libc.so.6 $@

$(BUILD_DIR)/user/libdl.so:
	@mkdir -p $(BUILD_DIR)/user
	cp -L /usr/lib/x86_64-linux-gnu/libdl.so.2 $@

$(BUILD_DIR)/user/libpthread.so:
	@mkdir -p $(BUILD_DIR)/user
	cp -L /usr/lib/x86_64-linux-gnu/libpthread.so.0 $@

$(BUILD_DIR)/user/libz.so:
	@mkdir -p $(BUILD_DIR)/user
	cp -L /lib/x86_64-linux-gnu/libz.so.1 $@

$(BUILD_DIR)/user/libm.so:
	@mkdir -p $(BUILD_DIR)/user
	cp -L /lib/x86_64-linux-gnu/libm.so.6 $@

$(BUILD_DIR)/user/libgcc_s.so:
	@mkdir -p $(BUILD_DIR)/user
	cp -L /lib/x86_64-linux-gnu/libgcc_s.so.1 $@

$(BUILD_DIR)/user/libstdcxx.so:
	@mkdir -p $(BUILD_DIR)/user
	cp -L /usr/lib/x86_64-linux-gnu/libstdc++.so.6 $@

$(BUILD_DIR)/user/librt.so:
	@mkdir -p $(BUILD_DIR)/user
	cp -L /lib/x86_64-linux-gnu/librt.so.1 $@

# The optional GRUB diagnostic runs only the ELF runtime. Desktop imports use
# the original, complete file through scripts/run-mde.py.
APPIMAGE ?=
ifeq ($(strip $(APPIMAGE)),)
APP_RUNTIME := $(BUILD_DIR)/user/appruntime-empty
$(APP_RUNTIME):
	@mkdir -p $(BUILD_DIR)/user
	@: > "$@"
else
APP_RUNTIME := $(BUILD_DIR)/user/appruntime
$(APP_RUNTIME): $(APPIMAGE) $(ROOT)/scripts/agent_extract_runtime.py
	@mkdir -p $(BUILD_DIR)/user
	python3 $(ROOT)/scripts/agent_extract_runtime.py "$(APPIMAGE)" "$@"
endif

$(BUILD_DIR)/user/hostlibs.S $(BUILD_DIR)/user/hostlibs.gen.c: $(ROOT)/scripts/gen_hostlibs.py
	@mkdir -p $(BUILD_DIR)/user
	python3 $(ROOT)/scripts/gen_hostlibs.py $(BUILD_DIR)/user/hostlibs.S $(BUILD_DIR)/user/hostlibs.gen.c

$(HOSTLIBS_OBJ): $(BUILD_DIR)/user/hostlibs.gen.c $(ROOT)/kernel/include/linux/user_files.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

$(USER_BLOB): $(ROOT)/kernel/linux/user_blob.S $(BUILD_DIR)/user/hello.elf \
		$(BUILD_DIR)/user/dynhello $(BUILD_DIR)/user/ldlinux.so \
		$(BUILD_DIR)/user/libc.so $(BUILD_DIR)/user/libdl.so \
		$(BUILD_DIR)/user/libpthread.so $(BUILD_DIR)/user/libz.so \
		$(BUILD_DIR)/user/libm.so $(BUILD_DIR)/user/libgcc_s.so \
		$(BUILD_DIR)/user/libstdcxx.so $(BUILD_DIR)/user/librt.so \
		$(BUILD_DIR)/user/hostlibs.S \
		$(APP_RUNTIME) $(ROOT)/Makefile
	@mkdir -p $(dir $@)
	$(NASM) $(ASFLAGS) -I$(ROOT)/ -DMETTA_APPRUNTIME='"$(APP_RUNTIME)"' -o $@ $<

ifeq ($(BUILD_MDE),0)
LINK_OBJS := $(KERNEL_LINK_OBJS)
else
LINK_OBJS := $(KERNEL_LINK_OBJS) $(MDE_OBJS)
endif

QEMU ?= qemu-system-x86_64
# 6144 MiB; ISO híbrido como disco (El Torito CD se queda colgado en SeaBIOS/WSL).
QEMU_MEM ?= 6144
QEMU_ISO_RUNTIME := /tmp/mettaos-run.iso
DATA_DISK ?= $(ROOT)/data/mettaos.img
QEMU_FLAGS := -m $(QEMU_MEM) -serial stdio -nic user,model=rtl8139 -no-reboot \
	-drive file=$(QEMU_ISO_RUNTIME),format=raw,if=ide,index=0 \
	-drive file="$(DATA_DISK)",format=raw,if=ide,index=1 \
	-boot order=c \
	-machine pc,vmport=on -rtc base=localtime \
	-accel kvm -accel tcg

GRUB_MKRESCUE ?= grub-mkrescue

.DEFAULT_GOAL := all

.PHONY: all kernel-only iso run clean print-objs

all: $(BIN)

kernel-only:
	@$(MAKE) BUILD_MDE=0 all

iso: $(ISO)

$(BIN): $(LINK_OBJS) $(ROOT)/linker.ld
	@mkdir -p $(dir $@)
	@echo "[LD]  $@ (BUILD_MDE=$(BUILD_MDE))"
	$(LD) $(LDFLAGS) -o $@ $(LINK_OBJS)
	@echo "[OK]  kernel: $@"

$(ISO): $(BIN) $(ROOT)/boot/grub/grub.cfg | $(ISO_DIR)/boot/grub
	@cp $(BIN) $(ISO_DIR)/boot/mettaos.bin
	@cp $(ROOT)/boot/grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	@echo "[ISO] $@"
	$(GRUB_MKRESCUE) -o $@ $(ISO_DIR)

$(ISO_DIR)/boot/grub:
	@mkdir -p $@

$(OBJ_DIR)/boot/%.o: $(ROOT)/boot/%.S
	@mkdir -p $(dir $@)
	@echo "[ASM] $<"
	$(NASM) $(ASFLAGS) -o $@ $<

$(OBJ_DIR)/kernel/%.o: $(ROOT)/kernel/%.c
	@mkdir -p $(dir $@)
	@echo "[CC]  $<"
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

$(OBJ_DIR)/mde/%.o: $(ROOT)/mde/%.c
	@mkdir -p $(dir $@)
	@echo "[CC]  $<"
	$(CC) $(MDE_CFLAGS) -MMD -MP -c -o $@ $<

-include $(shell find $(OBJ_DIR) -name '*.d' 2>/dev/null)

$(DATA_DISK):
	python3 scripts/create-data-disk.py "$@"

run: $(ISO) $(DATA_DISK)
	python3 scripts/run-mde.py --qemu "$(QEMU)" --memory "$(QEMU_MEM)" --iso "$(ISO)" --disk "$(DATA_DISK)" $(if $(LINUX_ISO),--linux-iso "$(LINUX_ISO)") $(if $(RUN_APPIMAGE),--appimage "$(RUN_APPIMAGE)")

clean:
	@echo "[CLEAN]"
	rm -rf $(BUILD_DIR)

print-objs:
	@echo "BUILD_MDE:   $(BUILD_MDE)"
	@echo "BOOT_SRCS:   $(BOOT_SRCS)"
	@echo "KERNEL_SRCS: $(KERNEL_SRCS)"
	@echo "MDE_SRCS:    $(MDE_SRCS)"
	@echo "LINK_OBJS:   $(LINK_OBJS)"
