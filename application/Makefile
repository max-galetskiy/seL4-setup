# If you would like to choose a different path to the SDK, you can pass it as an
# argument.
ifndef MICROKIT_SDK
	MICROKIT_SDK := ../microkit-sdk-2.2.0
endif

# Platform Specification
ifneq ($(MAKECMDGOALS), clean)
	ifndef PLATFORM
        $(error Please provide a PLATFORM argument [qemu-riscv, cva6])
	endif
endif

# In case the default compiler triple doesn't work for you or your package manager,
# you can specifiy the toolchain.
ifndef TOOLCHAIN
	# Get whether the common toolchain triples exist
	TOOLCHAIN_RISCV64_UNKNOWN_ELF := $(shell command -v riscv64-unknown-elf-gcc 2> /dev/null)
	TOOLCHAIN_RISCV64_LINUX_GNU := $(shell command -v riscv64-linux-gnu-gcc 2> /dev/null)
	# Then check if they are defined and select the appropriate one
	ifdef TOOLCHAIN_RISCV64_UNKNOWN_ELF
		TOOLCHAIN := riscv64-unknown-elf
	else ifdef TOOLCHAIN_RISCV64_LINUX_GNU
		TOOLCHAIN := riscv64-linux-gnu
	else
		$(error "Could not find a RISC-V 64-bit cross-compiler")
	endif
endif

ifneq ($(MAKECMDGOALS), clean)
  ifeq ($(PLATFORM), qemu-riscv)
      BOARD := qemu_virt_riscv64
      BUILD_DIR := build-qemu
  else ifeq ($(PLATFORM), cva6)
      BOARD := ariane
      BUILD_DIR := build-cva6
      OPENSBI_DIR := ../opensbi
      
      # Find the CVA6 Variable
      ifdef CVA6
      	VARIANE_TESTHARNESS := $(CVA6)/work-ver/Variane_testharness
      else
      	$(info ---------------------------------------------------)
      	$(info CVA6 Environment Variable not set. Assuming default)
      	$(info ---------------------------------------------------)
      	CVA6 := ../../../cva6
	VARIANE_TESTHARNESS := $(CVA6)/work-ver/Variane_testharness
      endif	
  else
      $(error Invalid platform: '$(PLATFORM)'. Choose a valid platform [qemu-riscv, cva6])
  endif
endif

MICROKIT_CONFIG := debug

CC := $(TOOLCHAIN)-gcc
LD := $(TOOLCHAIN)-ld
AS := $(TOOLCHAIN)-as
MICROKIT_TOOL ?= $(MICROKIT_SDK)/bin/microkit

SERVER_OBJS := server.o
CLIENT_OBJS := client.o

BOARD_DIR := $(MICROKIT_SDK)/board/$(BOARD)/$(MICROKIT_CONFIG)

IMAGES := server.elf client.elf

CFLAGS := -march=rv64gc -mabi=lp64d -mstrict-align -nostdlib -ffreestanding -g -Wall -Wno-array-bounds -Wno-unused-variable -Wno-unused-function -Werror -I$(BOARD_DIR)/include -Ivmm/src/util -Iinclude -DBOARD_$(BOARD) -DPLATFORM=$(PLATFORM)
LDFLAGS := -L$(BOARD_DIR)/lib
LIBS := -lmicrokit -Tmicrokit.ld

SYSTEM_FILE = $(BUILD_DIR)/application.system

ELF_FILE = $(BUILD_DIR)/loader.elf
IMAGE_FILE = $(BUILD_DIR)/loader.img
REPORT_FILE = $(BUILD_DIR)/report.txt

SIMULATE_SCRIPT = $(BUILD_DIR)/simulate.sh

all: directories $(IMAGE_FILE)

directories:
	@mkdir -p $(BUILD_DIR)
ifeq ($(PLATFORM), qemu-riscv)
	@echo "#!/bin/bash" > $(SIMULATE_SCRIPT)
	@echo "qemu-system-riscv64 -machine virt \\" >> $(SIMULATE_SCRIPT)
	@echo "-nographic \\" >> $(SIMULATE_SCRIPT)
	@echo "-serial mon:stdio \\" >> $(SIMULATE_SCRIPT)
	@echo "-kernel $(IMAGE_FILE) \\" >> $(SIMULATE_SCRIPT)
	@echo "-m size=2G \\" >> $(SIMULATE_SCRIPT)
	@echo "-netdev user,id=mynet0 \\" >> $(SIMULATE_SCRIPT)
	@echo "-device virtio-net-device,netdev=mynet0,mac=52:55:00:d1:55:01\\" >> $(SIMULATE_SCRIPT)
else ifeq ($(PLATFORM), cva6)
	@echo "#!/bin/bash" > $(SIMULATE_SCRIPT)
	@echo "$(VARIANE_TESTHARNESS) payload.elf +time_out=1000000000000" >> $(SIMULATE_SCRIPT)	
endif	

clean:
	@echo "Cleaning build directories..."
	rm -rf build
	rm -rf build-qemu
	rm -rf build-cva6


$(SYSTEM_FILE): application.system.template Makefile
	$(CC) -E -P -xc -DPLATFORM_$(subst -,_,$(PLATFORM)) $< -o $@

$(BUILD_DIR)/%.o: %.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/server.elf: $(addprefix $(BUILD_DIR)/, $(SERVER_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/client.elf: $(addprefix $(BUILD_DIR)/, $(CLIENT_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(IMAGE_FILE): $(addprefix $(BUILD_DIR)/, $(IMAGES)) $(SYSTEM_FILE)
ifeq ($(PLATFORM), qemu-riscv)
	$(MICROKIT_TOOL) $(SYSTEM_FILE) --search-path $(BUILD_DIR) --board $(BOARD) --config $(MICROKIT_CONFIG) -o $(IMAGE_FILE) -r $(REPORT_FILE)
else ifeq ($(PLATFORM), cva6)
	
	# Microkit ELF
	$(MICROKIT_TOOL) $(SYSTEM_FILE) --search-path $(BUILD_DIR) --board $(BOARD) --config $(MICROKIT_CONFIG) -o $(ELF_FILE)
	$(TOOLCHAIN)-objcopy -O binary $(ELF_FILE) $(BUILD_DIR)/loader.bin
	
	# Hotpatch the "typedef int bool;" line of OpenSBI v1.1
	@grep -q "__bool_true_false_are_defined" $(OPENSBI_DIR)/include/sbi/sbi_types.h || \
	sed -i -E 's/typedef[[:space:]]+int[[:space:]]+bool;/\n#if __STDC_VERSION__ < 202311L\ntypedef int bool;\n#endif\n/' $(OPENSBI_DIR)/include/sbi/sbi_types.h
	
	# Compile OpenSBI with the Microkit ELF as payload
	@mkdir -p $(BUILD_DIR)/opensbi_out
	$(MAKE) -C $(OPENSBI_DIR) \
		CROSS_COMPILE=$(TOOLCHAIN)- \
		PLATFORM=fpga/ariane \
		FW_TEXT_START=0x80000000 \
		FW_PAYLOAD_PATH=$(CURDIR)/$(BUILD_DIR)/loader.bin \
		FW_PAYLOAD_OFFSET=0x200000 \
		O=$(CURDIR)/$(BUILD_DIR)/opensbi_out
	
	# Placeholders
	# 		FW_PAYLOAD_OFFSET=0x200000 \
	#	FW_FDT_PATH=../../cva6/corev_apu/bootrom/ariane.dtb \
	
	# Copy the final payload to the build directory
	cp $(BUILD_DIR)/opensbi_out/platform/fpga/ariane/firmware/fw_payload.elf $(BUILD_DIR)/payload.elf
	
	# Remove the Hotpatch (for robustness reasons)
	@sed -i -E 's/\n#if __STDC_VERSION__ < 202311L\ntypedef int bool;\n#endif\n/typedef int bool;/' $(OPENSBI_DIR)/include/sbi/sbi_types.h
	
endif
