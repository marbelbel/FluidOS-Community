ZIG ?= zig
QEMU ?= qemu-system-x86_64
LIMINE_DIR ?= limine-binary
LIMINE = $(LIMINE_DIR)/limine

# Keep every subsystem buildable from the same freestanding Zig toolchain used
# by build.ps1. GNU make is useful on Unix-like hosts; Windows uses build.ps1.
SOURCES = $(shell find src -type f -name '*.c' -print)
ZIG_SOURCE = src/security/package_crypto.zig
HEADERS = $(shell find src -type f \( -name '*.h' -o -name '*.zig' \) -print)
INCLUDE_DIRS = $(shell find src -type d -print)
INCLUDE_FLAGS = $(addprefix -I,$(INCLUDE_DIRS))
ISO_ROOT = build/iso_root
ISO = build/VOS.iso

CFLAGS = -std=gnu11 -O2 -Wall -Wextra -ffreestanding -fno-builtin \
	-fno-stack-protector -fno-pic -fno-pie -m64 -mno-red-zone \
	-mgeneral-regs-only -mno-sse -mno-sse2 -mno-mmx -mcmodel=kernel \
	$(INCLUDE_FLAGS)

.PHONY: all kernel iso run check-build-tools check-iso-tools check-run-tools clean

all: iso

kernel: build/kernel.elf

iso: $(ISO)

run: $(ISO) | check-run-tools
	$(QEMU) -machine pc -m 512M -cdrom $(ISO) -boot order=d \
		-nic none -snapshot -no-reboot -no-shutdown

check-build-tools:
	@command -v $(ZIG) >/dev/null 2>&1 || { echo "Falta Zig ($(ZIG))."; exit 1; }

check-iso-tools: check-build-tools
	@command -v xorriso >/dev/null 2>&1 || { echo "Falta xorriso para crear la ISO."; exit 1; }
	@test -x "$(LIMINE)" || { echo "Falta $(LIMINE); extrae el paquete binario oficial de Limine en $(LIMINE_DIR)."; exit 1; }
	@test -f "$(LIMINE_DIR)/limine-bios.sys" || { echo "Falta $(LIMINE_DIR)/limine-bios.sys."; exit 1; }
	@test -f "$(LIMINE_DIR)/limine-bios-cd.bin" || { echo "Falta $(LIMINE_DIR)/limine-bios-cd.bin."; exit 1; }
	@test -f "$(LIMINE_DIR)/limine-uefi-cd.bin" || { echo "Falta $(LIMINE_DIR)/limine-uefi-cd.bin."; exit 1; }
	@test -f "$(LIMINE_DIR)/BOOTX64.EFI" || { echo "Falta $(LIMINE_DIR)/BOOTX64.EFI."; exit 1; }

check-run-tools:
	@command -v $(QEMU) >/dev/null 2>&1 || { echo "Falta $(QEMU) para arrancar la ISO en una maquina virtual."; exit 1; }

build:
	mkdir -p build

build/kernel.elf: $(SOURCES) $(ZIG_SOURCE) $(HEADERS) linker.ld | build check-build-tools
	$(ZIG) build-exe -target x86_64-freestanding-none -O ReleaseFast \
		-fentry=kernel_main -T linker.ld -femit-bin=$@ \
		-cflags $(CFLAGS) -- $(ZIG_SOURCE) $(SOURCES)

$(ISO): build/kernel.elf boot/limine.conf | check-iso-tools
	mkdir -p $(ISO_ROOT)/boot $(ISO_ROOT)/EFI/BOOT
	cp build/kernel.elf $(ISO_ROOT)/boot/kernel.elf
	cp boot/limine.conf $(ISO_ROOT)/limine.conf
	cp $(LIMINE_DIR)/limine-bios.sys $(ISO_ROOT)/
	cp $(LIMINE_DIR)/limine-bios-cd.bin $(ISO_ROOT)/boot/
	cp $(LIMINE_DIR)/limine-uefi-cd.bin $(ISO_ROOT)/boot/
	cp $(LIMINE_DIR)/BOOTX64.EFI $(ISO_ROOT)/EFI/BOOT/
	xorriso -as mkisofs -R -r -J -b boot/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot boot/limine-uefi-cd.bin -efi-boot-part --efi-boot-image \
		--protective-msdos-label $(ISO_ROOT) -o $(ISO)
	$(LIMINE) bios-install $(ISO)

clean:
	rm -rf build
