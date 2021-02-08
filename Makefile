#
# Copyright (C) 2021 Gaël PORTAY
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

QEMU ?= qemu-system-x86_64
QEMU += -enable-kvm -m 4G -machine q35 -smp 4 -cpu host
ifndef NO_SPICE
QEMU += -vga virtio -display egl-headless,gl=on
QEMU += -spice port=5924,disable-ticketing -device virtio-serial-pci -device virtserialport,chardev=spicechannel0,name=com.redhat.spice.0 -chardev spicevmc,id=spicechannel0,name=vdagent
endif
QEMU += -serial mon:stdio
ifndef NO_OVMF
QEMU += -drive format=raw,file=/usr/share/ovmf/x64/OVMF_CODE.fd,readonly,if=pflash -drive format=raw,file=OVMF_VARS.fd,if=pflash
endif

ARCH = $(shell uname -m)
ifneq ($(ARCH),x86_64)
$(error $(ARCH) not supported)
endif

# https://wiki.osdev.org/GNU-EFI
# https://wiki.osdev.org/UEFI#Developing_with_GNU-EFI
CFLAGS += -Wall -Werror -Wextra
CFLAGS += -I/usr/include/efi -I/usr/include/efi/$(ARCH) -fpic
CFLAGS += -ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar
CFLAGS += -mno-red-zone -maccumulate-outgoing-args

ELFFLAGS += -nostdlib -znocombreloc
ELFFLAGS += -T /usr/lib/elf_$(ARCH)_efi.lds -shared -Bsymbolic
ELFFLAGS += -L/usr/lib

LOADLIBES += /usr/lib/crt0-efi-$(ARCH).o
LDLIBS += -lgnuefi -lefi

.PHONY: all
all: defi.efi

.PHONY: run-qemu
run-qemu: ESP/EFI/BOOT/BOOTX64.EFI | OVMF_VARS.fd
	$(QEMU) $(QEMUFLAGS) -drive file=fat:rw:ESP

.PHONY: run-remote-viewer
run-remote-viewer:
	remote-viewer spice://localhost:5924

.PRECIOUS: OVMF_VARS.fd
OVMF_VARS.fd: /usr/share/ovmf/x64/OVMF_VARS.fd
	cp $< $@

LINK.elf = $(LD) $(ELFFLAGS) $(TARGET_ARCH)
%.elf: %.o
	$(LINK.elf) $^ $(LOADLIBES) $(LDLIBS) -o $@

OBJCOPY = objcopy
OBJCOPYFLAGS += -j .text -j .sdata -j .data -j .dynamic -j .dynsym  -j .rel -j .rela -j .reloc
%.efi: %.elf
	$(OBJCOPY) $(OBJCOPYFLAGS) --target=efi-app-x86_64 $< $@

ESP/EFI/BOOT/BOOTX64.EFI ESP/EFI/D/D.EFI: defi.efi
	install -Dm0755 $< $@

.PHONY: clean
clean:
	rm -f defi.elf defi.efi

.PHONY: mrproper
mrproper: clean
	rm -Rf ESP/
