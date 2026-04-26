# Detect OS and set LLVM paths accordingly
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # macOS with Homebrew LLVM 18
    LLVM_PREFIX := /opt/homebrew/opt/llvm@18/bin
else
    # Linux with LLVM 18 from apt repository
    LLVM_PREFIX := /usr/lib/llvm-18/bin
endif

SDK     ?= /opt/ps5-kld-sdk
SDK_FREEBSD_HEADERS := $(if $(wildcard $(SDK)/include/freebsd-headers),$(SDK)/include/freebsd-headers,$(SDK)/freebsd-headers)
SDK_LINKER_SCRIPT   := $(if $(wildcard $(SDK)/ld/linker.x),$(SDK)/ld/linker.x,$(SDK)/linker.x)
SDK_LIBS            := $(if $(wildcard $(SDK)/lib/*.o),$(SDK)/lib/*.o,$(SDK)/build/*.o)
CC      := $(LLVM_PREFIX)/clang
LD      := $(LLVM_PREFIX)/ld.lld
OBJCOPY := $(LLVM_PREFIX)/llvm-objcopy
ODIR    := build
SDIR    := src

MAPFILE := $(shell basename "$(CURDIR)").map
CFLAGS  := --target=x86_64-unknown-none-elf \
           -I$(SDK)/include/ \
           -I$(SDK_FREEBSD_HEADERS)/ \
           -I./include/ \
           $(IDIRS) \
           -Os -std=c11 \
           -ffunction-sections -fdata-sections \
           -fno-builtin -nostdlib -ffreestanding \
           -Wall -Wextra \
           -march=btver2 -mtune=btver2 \
           -m64 -mcmodel=small \
           -mno-red-zone \
           -mno-mmx -mno-sse -mno-sse2 -mno-avx \
           -mno-80387 -msoft-float \
           -fPIC \
           -fno-vectorize -fno-slp-vectorize

LFLAGS  := -T $(SDK_LINKER_SCRIPT) \
           -Map=$(MAPFILE) \
           --gc-sections \
           --build-id=none

CFILES  := $(wildcard $(SDIR)/*.c)
SFILES  := $(wildcard $(SDIR)/*.s)
OBJS    := $(patsubst $(SDIR)/%.c, $(ODIR)/%.o, $(CFILES)) $(patsubst $(SDIR)/%.s, $(ODIR)/%.o, $(SFILES))
LIBS    := $(SDK_LIBS)
TARGET  := $(shell basename "$(CURDIR)").bin
ELF     := $(shell basename "$(CURDIR)").elf

$(TARGET): $(ODIR) $(OBJS)
	$(LD) $(ODIR)/*.o $(LIBS) -o $(ELF) $(LFLAGS)
	$(OBJCOPY) -S -O binary $(ELF) "$(TARGET)"
	mv $(ELF) build/

$(ODIR)/%.o: $(SDIR)/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(ODIR)/%.o: $(SDIR)/%.s
	$(CC) -c -o $@ $< $(CFLAGS)

$(ODIR):
	@mkdir -p build/

.PHONY: clean

clean:
	rm -rf "$(TARGET)" "$(MAPFILE)" $(ODIR)
