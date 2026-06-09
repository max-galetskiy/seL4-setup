# Acknowledgements
This was originally part of a course project for the course ["Embedded Systems"](https://onlineservices.polimi.it/manifesti/manifesti/controller/ManifestoPublic.do?EVN_DETTAGLIO_RIGA_MANIFESTO=evento&aa=2025&k_cf=225&k_corso_la=542&k_indir=T2A&codDescr=056899&lang=EN&semestre=1&idGruppo=5415&idRiga=332890) of [Prof. William Fornaciari](https://www.deib.polimi.it/eng/people/details/196670) during my exchange at the [Politecnico Di Milano](https://www.polimi.it/). It was supervised by [Andrea Galimberti](https://onlineservices.polimi.it/manifesti/manifesti/controller/ricerche/RicercaPerDocentiPublic.do?evn_DIDATTICA=evento&lang=IT&k_doc=455015).

# Table of Contents
* Designing & Simulating an SeL4 Microkit application
    * 0: Prerequisites
    * 1: Setting up
    * 2: Building and Executing
* Appendix A: Installing Prerequisites (not related to SeL4)
    * 1: RISC-V GNU Toolchain
    * 2: Spike
    * 3: CVA6/Ariane
* Appendix B: Build & Simulate an SeL4 Application without the Microkit
    * 0: Requirements
    * 1: Set up Kernel + Dependencies
    * 2: Set up Custom Application + Makefile
    * 3: Folder Structure
    * 4: Build and Simulate
* Appendix C: Simulate the SeL4 Microkit with Ariane/CVA6
    * 1: Initial Setup Attempt
    * 2: Troubleshooting

# Designing & Simulating an SeL4 Microkit application

## 0. Prerequisites
For an SeL4 Microkit Application on a simulated RISC-V target, you will need:
* make (`[sudo] apt install make`)
* RISC-V Toolchain (see Appendix A)
* QEMU for RISC-V (`[sudo] apt install qemu-system-riscv`)

## 1. Setting up
First, create a directory and then download the Microkit SDK:
```
mkdir microkit_application && cd microkit_application
curl -L https://github.com/seL4/microkit/releases/download/2.2.0/microkit-sdk-2.2.0-linux-x86-64.tar.gz -o sdk.tar.gz
tar xf sdk.tar.gz
```

Note: This assumes Linux and an Intel CPU, for an AArch64, macOS, or Nix see [the official tutorial](https://docs.sel4.systems/projects/microkit/tutorial/part0.html).

Having installed the Microkit itself, create a new directory for your application code
```
mkdir application && cd application
```

For an application on the SeL4 Microkit, you will need the following:
* a `*.system` file describing the architecture of your system in XML
* at least one `.c` file for each process (called "protection domain" in SeL4) with it's code. Every file has to define the `init()` and `notified(...)` functions
* a `Makefile` to build

For concrete examples of the first two see **Appendix D**.

The following is a sample Makefile for two protection domains (`server.c` and `client.c`) and a system file called `application.system` all intended to run on QEMU:

```
# If you would like to choose a different path to the SDK, you can pass it as an
# argument.
ifndef MICROKIT_SDK
	MICROKIT_SDK := ../microkit-sdk-2.2.0
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

BOARD := qemu_virt_riscv64
BUILD_DIR := build-qemu

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
	@echo "#!/bin/bash" > $(SIMULATE_SCRIPT)
	@echo "qemu-system-riscv64 -machine virt \\" >> $(SIMULATE_SCRIPT)
	@echo "-nographic \\" >> $(SIMULATE_SCRIPT)
	@echo "-serial mon:stdio \\" >> $(SIMULATE_SCRIPT)
	@echo "-kernel $(IMAGE_FILE) \\" >> $(SIMULATE_SCRIPT)
	@echo "-m size=2G \\" >> $(SIMULATE_SCRIPT)
	@echo "-netdev user,id=mynet0 \\" >> $(SIMULATE_SCRIPT)
	@echo "-device virtio-net-device,netdev=mynet0,mac=52:55:00:d1:55:01\\" >> $(SIMULATE_SCRIPT)

$(BUILD_DIR)/%.o: %.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/server.elf: $(addprefix $(BUILD_DIR)/, $(SERVER_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/client.elf: $(addprefix $(BUILD_DIR)/, $(CLIENT_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(IMAGE_FILE): $(addprefix $(BUILD_DIR)/, $(IMAGES)) $(SYSTEM_FILE)
$(MICROKIT_TOOL) $(SYSTEM_FILE) --search-path $(BUILD_DIR) --board $(BOARD) --config $(MICROKIT_CONFIG) -o $(IMAGE_FILE) -r $(REPORT_FILE)
```

Note: The `Makefile` in our code and **Appendix D** is a bit more complicated than this one. It expects you to call it via:
```
make PLATFORM=qemu-riscv
```


## 2. Building and Executing
Assuming you have your application code and a suitable Makefile (like the sample one), you then simply build the code via `make`.

With the `Makefile` from before, the script to run the simulation can be run via:
```
bash build-qemu/simulate.sh
```

Otherwise, you can also run the command yourself:
```
qemu-system-riscv64 -machine virt -nographic -serial mom:stdio -kernel build-qemu/loader.img -m size=2G -netdev user,id=mynet0 -device virtio-net-device,netdev=mynet0,mac=2:55:00:d1:55:01
```

The simulation may be stopped via `CTRL+a` and then `x`.

Currently, simulation only works with QEMU. For information on building and simulating with CVA6/Ariane, see **Appendix C**

# Appendix A: Installing Prerequisites (not related to SeL4)

## 1. RISC-V GNU Toolchain
Clone the repository (Note: The repo is around 6.65GB in size)
```
git clone https://github.com/riscv/riscv-gnu-toolchain
```

Then install the prerequisites (here Ubuntu is assumed, refer to the Github repo for other Linux distros)
```
[sudo] apt-get install autoconf automake autotools-dev curl python3 python3-pip python3-tomli libmpc-dev libmpfr-dev libgmp-dev gawk build-essential bison flex texinfo gperf libtool patchutils bc zlib1g-dev libexpat-dev ninja-build git cmake libglib2.0-dev libslirp-dev libncurses-dev
```
Then install the toolchain into the path specified by `$RISCV`
```
cd riscv-gnu-toolchain
./configure --prefix=$RISCV
make
```

## 2. Spike
A note on Spike: Currently, the Microkit does not support Spike. However, it is possible to simulate the SeL4 Kernel itself via Spike (see **Appendix B**).

Clone the repository:
```
git clone https://github.com/riscv-software-src/riscv-isa-sim
```
Install prerequisites:
```
[sudo] apt-get install device-tree-compiler libboost-regex-dev libboost-system-dev
```

Then build:
```
cd riscv-isa-sim
mkdir build
cd build
../configure --prefix=$RISCV
make
[sudo] make install
```

## 3. CVA6/Ariane
A note on CVA6: Currently, our Microkit setup does not work with CVA6. However, it is possible to simulate the SeL4 Kernel itself via CVA6 (see **Appendix B**).


First, clone the repo and initialize all sub-repos:
```
git clone https://github.com/openhwgroup/cva6.git
cd cva6
git submodule update --init --recursive
```

For us, the most reliable way to build the Verilate model of CVA6 was to reuse the one created by the smoke-tests. The repo itself recommends using either the `verif/sim/cva6.py` script or `make verilate`, but neither option worked for us personally.

To use the model generated by the smoke-tests, you will need to commend out the following lines (56-57) in `verif/regress/smoke-tests-cv64a6_imafdc_sv39.sh`:

```
python3 cva6.py --c_tests ../tests/custom/hello_world/hello_world.c --iss_yaml cva6.yaml --target cv64a6_imafdc_sv39 --iss=$DV_SIMULATORS --gcc_opts="$CC_OPTS -nostdlib -lgcc" $DV_OPTS --linker=../../config/gen_from_riscv_config/linker/link.ld
# make -C ../.. clean
# make clean_all
```

This is to make sure the Verilate model is not removed after running the smoke-tests. After modifying the file, simply run:
```
bash verif/regress/smoke-tests-cv64a6_imafdc_sv39.sh
```

Afterwards, the model should lie at `work-ver/Variane_testharness`

**Troubleshooting**: If something fails, maybe the reason is that your RISCV-Toolchain does not have the correct fork of the `fesvr` library. If this is the case, you can simply copy and patch the `/include/` and `libfesvr.a` files from spike which will be installed anyways by the bash script (find the files at `/tools/spike`). Other problems can come from issues with your $PATH or $RISCV environmental variables.

# Appendix B: Build & Simulate an SeL4 Application without the Microkit
Note: The project structure of SeL4 is very particular, so follow these steps carefully (**Section 3** also gives an overview of the folder structure in case you're lost).

## 0. Requirements
The complex build system of SeL4 requires a number of utilities:
```
[sudo] apt-get install build-essential cmake ccache ninja-build cmake-curses-gui libxml2-utils ncurses-dev curl git doxygen device-tree-compiler xxd u-boot-tools python3-dev python3-pip python-is-python3 protobuf-compiler python3-protobuf
```

Then, you also need a number of python packages. Note that (at least for the testing suite SeL4Test) an older version of setuptools is required. It is recommended to use a vortual environment like this:
```
python3 -m venv seL4-venv  
./seL4-venv/bin/pip install sel4-deps
./seL4-venv/bin/pip install "setuptools<70.0.0"
source ./seL4-venv/bin/activate
```

It's important that the command `python3` actually calls this venv version of python (can be managed by changing your `$PATH` variable temporarily)

## 1. Set up Kernel + Dependencies
First, clone the Kernel itself into a directory called "kernel"
```
git clone https://github.com/seL4/seL4.git kernel
```

Then create a "tools" folder whcih will contain the SeL4 building tools and OpenSBI:

```
mkdir tools   
cd tools
git clone https://github.com/seL4/seL4\_tools.git ./seL4
git clone --branch v0.9 https://github.com/riscv-software-src/opensbi opensbi
cd ../
```

--------------
**Note**: We use v0.9 for OpenSBI since this is also used by the testing suite. However, this version has a problem with the newest C standard (C23). 

To fix, you need to find the file `tools/opensbi/include/sbi/sbi_types.h` and change this line `typedef int bool;` to:
```
#if __STDC_VERSION__ < 202311L
typedef int bool;
#endif
```
You can also put this into a sed command inside of your Makefile for convenience (but make sure to not overwrite the file repeatedly):
```
ed -i -E 's/typedef[[:space:]]+int[[:space:]]+bool;/\n#if __STDC_VERSION__ < 202311L\ntypedef int bool;\n#endif\n/' $(OPENSBI_DIR)/include/sbi/sbi_types.h
```
--------------

After cloning OpenSBI, initialize the "projects" folder. This will contain your custom application, but also the following libraries:
* SeL4 Runtime (runtime required by a C process)
* SeL4 Libraries (libraries to work with SeL4)
* SeL4 Utility Libraries (OS-independent utilities)
* Musl LibC (SeL4's implementation of the C stdlib)

```
mkdir projects
cd projects
git clone https://github.com/seL4/sel4runtime.git sel4runtime
git clone https://github.com/seL4/sel4_libs.git seL4_libs
git clone https://github.com/seL4/util_libs util_libs
git clone https://github.com/seL4/musllibc.git musllibc
```

## 2. Set up Custom Application + Makefile
First, set up a folder where your custom application will live (still inside the `projects` folder!). Here, we just assume a simple hello world program with a `hello.c` file:

```
mkdir <custom_project> && cd <custom_project>
mkdir apps && cd apps
mkdir <your_application> && cd <your_application>
mkdir src 
```
Add your code into the `src` folder, then add the following `CMakeLists.txt` into the `<your_application>` folder. It's basically a skimmed down version of the one used by Sel4Test:

```
#
# Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.16.0)

project(hello C)

set(configure_string "")

find_package(musllibc REQUIRED)
find_package(util_libs REQUIRED)
find_package(seL4_libs REQUIRED)

set(UserLinkerGCSections OFF CACHE BOOL "" FORCE)
# This sets up environment build flags and imports musllibc and runtime libraries.
musllibc_setup_build_environment_with_sel4runtime()
sel4_import_libsel4()
util_libs_import_libraries()
sel4_libs_import_libraries()

file(
    GLOB
        static
        src/*.c
)

add_executable(hello src/hello.c)

# target_include_directories(hello PRIVATE "include")
target_link_libraries(
    hello
    PUBLIC
        sel4_autoconf
        muslc
        sel4
        sel4runtime
        sel4allocman
        sel4vka
        sel4utils
        sel4platsupport
        sel4muslcsys
)
target_compile_options(hello PRIVATE -Werror -g)

# Set this image as the rootserver
include(rootserver)
DeclareRootserver(hello)
```

Then go back to your `<custom_project>` folder and add the following three files:

1. CMakeLists.txt:
```
#
# Copyright 2019, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.16.0)

include(settings.cmake)
project(custom C CXX ASM)

find_package(seL4 REQUIRED)
find_package(elfloader-tool REQUIRED)

set(KernelRootCNodeSizeBits 13 CACHE INTERNAL "")

sel4_import_kernel()

if((NOT Sel4testAllowSettingsOverride) AND (KernelArchARM OR KernelArchRiscV))
    # Elfloader settings that correspond to how Data61 sets its boards up.
    ApplyData61ElfLoaderSettings(${KernelPlatform} ${KernelSel4Arch})
endif()
elfloader_import_project()

add_subdirectory(apps/<your_application>)
if(SIMULATION)
    include(simulation)
    if(KernelSel4ArchX86\_64)
        SetSimulationScriptProperty(MEM\_SIZE "3G")
    endif()
    if(KernelPlatformQEMUArmVirt)
        SetSimulationScriptProperty(MEM\_SIZE "2G")
    endif()
    GenerateSimulateScript()
endif()
```

2. easy-settings.cmake:

```
#
# Copyright 2019, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Define our top level settings.  Whilst they have doc strings for readability
# here, they are hidden in the cmake-gui as they cannot be reliably changed
# after the initial configuration.  Enterprising users can still change them if
# they know what they are doing through advanced mode.
#
# Users should initialize a build directory by doing something like:
#
# mkdir build\_sabre
# cd build\_sabre
#
# Then
#
# ../griddle --PLATFORM=sabre --SIMULATION
# ninja
#
set(SIMULATION OFF CACHE BOOL "Include only simulation compatible tests")
set(RELEASE OFF CACHE BOOL "Performance optimized build")
set(VERIFICATION OFF CACHE BOOL "Only verification friendly kernel features")
set(BAMBOO OFF CACHE BOOL "Enable machine parseable output")
set(DOMAINS OFF CACHE BOOL "Test multiple domains")
set(SMP OFF CACHE BOOL "(if supported) Test SMP kernel")
set(NUM_NODES "" CACHE STRING "(if SMP) the number of nodes (default 4)")
set(PLATFORM "spike" CACHE STRING "Platform to test")
set(ARM_HYP OFF CACHE BOOL "Hyp mode for ARM platforms")
set(MCS OFF CACHE BOOL "MCS kernel")
set(KernelSel4Arch "" CACHE STRING "aarch32, aarch64, arm_hyp, ia32, x86_64, riscv32, riscv64")
set(LibSel4TestPrinterRegex ".*" CACHE STRING "A POSIX regex pattern used to filter tests")
set(LibSel4TestPrinterHaltOnTestFailure OFF CACHE BOOL "Halt on the first test failure")
mark_as_advanced(CLEAR LibSel4TestPrinterRegex LibSel4TestPrinterHaltOnTestFailure)
```

3. settings.cmake:

```
#
# Copyright 2019, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.16.0)

set(project_dir "${CMAKE_CURRENT_LIST_DIR}/../..")
file(GLOB project_modules ${project_dir}/projects/*)
list(
    APPEND
        CMAKE_MODULE_PATH
        ${project_dir}/kernel
        ${project_dir}/tools/seL4/cmake-tool/helpers/
        ${project_dir}/tools/seL4/elfloader-tool/
        ${project_modules}
)

set(NANOPB_SRC_ROOT_FOLDER "${project_dir}/tools/nanopb" CACHE STRING "NanoPB Folder location")
set(OPENSBI_PATH "${project_dir}/tools/opensbi" CACHE STRING "OpenSBI Folder location")

set(SEL4_CONFIG_DEFAULT_ADVANCED ON)

include(application_settings)
include(${CMAKE_CURRENT_LIST_DIR}/easy-settings.cmake)

correct_platform_strings()

find_package(seL4 REQUIRED)
sel4_configure_platform_settings()

set(valid_platforms ${KernelPlatform_all_strings} ${correct_platform_strings_platform_aliases})
set_property(CACHE PLATFORM PROPERTY STRINGS ${valid_platforms})
if(NOT "${PLATFORM}" IN_LIST valid_platforms)
    message(FATAL_ERROR "Invalid PLATFORM selected: \"${PLATFORM}\"
Valid platforms are: \"${valid_platforms}\"")
endif()

# Declare a cache variable that enables/disablings the forcing of cache variables to
# the specific test values. By default it is disabled
set(Sel4testAllowSettingsOverride OFF CACHE BOOL "Allow user to override configuration settings")
if(NOT Sel4testAllowSettingsOverride)
    # We use 'FORCE' when settings these values instead of 'INTERNAL' so that they still appear
    # in the cmake-gui to prevent excessively confusing users
    if(ARM_HYP)
        set(KernelArmHypervisorSupport ON CACHE BOOL "" FORCE)
    endif()

    if(KernelSel4ArchAarch32)
        set(KernelArmTLSReg tpidruro CACHE STRING "" FORCE)
    endif()
    if(KernelSel4ArchAarch64)
        set(KernelArmTLSReg tpidru CACHE STRING "" FORCE)
    endif()

    if(KernelPlatformQEMUArmVirt OR KernelPlatformQEMURiscVVirt OR KernelPlatformSpike)
        set(SIMULATION ON CACHE BOOL "" FORCE)
    endif()

    if(SIMULATION)
        ApplyCommonSimulationSettings(${KernelSel4Arch})
    else()
        if(KernelArchX86)
            set(KernelIOMMU ON CACHE BOOL "" FORCE)
        endif()
    endif()


    # Check the hardware debug API non simulated (except for ia32, which can be simulated),
    # or platforms that don't support it.
    if(((NOT SIMULATION) OR KernelSel4ArchIA32) AND NOT KernelHardwareDebugAPIUnsupported)
        set(HardwareDebugAPI ON CACHE BOOL "" FORCE)
    else()
        set(HardwareDebugAPI OFF CACHE BOOL "" FORCE)
    endif()

    ApplyCommonReleaseVerificationSettings(${RELEASE} ${VERIFICATION})

    if(BAMBOO)
        set(LibSel4TestPrintXML ON CACHE BOOL "" FORCE)
    else()
        set(LibSel4TestPrintXML OFF CACHE BOOL "" FORCE)
    endif()

    if(DOMAINS)
        set(KernelNumDomains 4 CACHE STRING "" FORCE)
    else()
        set(KernelNumDomains 1 CACHE STRING "" FORCE)
    endif()

    if(SMP)
        if(NUM_NODES MATCHES "^\[0-9]+$")
            set(KernelMaxNumNodes ${NUM_NODES} CACHE STRING "" FORCE)
        else()
            set(KernelMaxNumNodes 4 CACHE STRING "" FORCE)
        endif()
    else()
        set(KernelMaxNumNodes 1 CACHE STRING "" FORCE)
    endif()

    if(MCS)
        set(KernelIsMCS ON CACHE BOOL "" FORCE)
    else()
        set(KernelIsMCS OFF CACHE BOOL "" FORCE)
    endif()

endif()
```

## 3. Folder Structure
If at any point, you are unsure regarding the (somewhat tedious) structure of SeL4, thisi s what your folders should look like before building (only important files shown):

```
./
├── easy-settings.cmake -> projects/<custom_project>/easy-settings.cmake
├── init-build.sh -> tools/seL4/cmake-tool/init-build.sh
├── kernel
│   └── \[...]
├── projects
│   ├── <custom_project>
|   |	├── apps
|   |	│   └── <your_application>
|   |	│       ├── CMakeLists.txt
|   |	│       └── src
|   |	│           └── hello.c
|   |	├── CMakeLists.txt
|   |	├── easy-settings.cmake
|   |	└── settings.cmake
│   ├── musllibc
│   │   └── \[...]
│   ├── seL4\_libs
│   │   └── \[...]
│   ├── sel4runtime
│   │   └── \[...]
│   └── util\_libs
│       └── \[...]
└── tools
    ├── opensbi
    │   └── \[...]
    └── seL4
        └── \[...]
```


## 4. Build and Simulate
Return all the way into the root folder (the one containing `kernel`, `projects` and `tools`). Then create the following Symlinks:

```
ln -s projects/custom_project/easy-settings.cmake easy-settings.cmake
ln -s tools/seL4/cmake-tool/init-build.sh init-build.sh
```
Afterwards you can easily build for whatever platform you want.

Building for Spike:
```
mkdir build
cd build
../init-build.sh -DPLATFORM=spike -DSIMULATION=True -DRISCV64=1
ninja
```

Running on Spike:
```
cd images
spike -m4096 hello-image-riscv-spike
```

Building for QEMU:
```
mkdir build
cd build
../init-build.sh -DPLATFORM=qemu-riscv-virt -DRISCV64=1 -DSIMULATION=True
ninja
```
Running on QEMU:
```
./simulate
```

Building for CVA6/Ariane:
```
mkdir build
cd build
../init-build.sh -DPLATFORM=ariane -DRISCV64=1 -DSIMULATION=True
ninja
```

Running on CVA6/Ariane: (assuming that the cva6 folder is in the same folder as your SeL4 project):
```
../../../cva6/work-ver/Variane_testharness hello-image-riscv-ariane +time_out=1000000000000000000000
```
The time_out should be set rather high since running an OS takes quite some time on the simulator.


# Appendix C: Simulate the SeL4 Microkit with Ariane/CVA6

## 1. Initial Setup Attempt
Theoretically, it should be possible to also simulate the SeL4 Microkit on CVA6/Ariane ([see the documentation](https://docs.sel4.systems/projects/microkit/manual/latest/#ariane)). However, we were not able to correctly boot. The following section describes our attempt and where it goes wrong.

In general, the section **1. Setting up** can be adapted to CVA6 almost verbatim.

The major change is that you now also need to clone and build OpenSBI (or any other compatible SBI of choice). For our setup, we clone OpenSBI at the same hierarchy where our `application` and microkit folders lie. We specifically choose version 1.1. because newer versions use kconfig which is more difficult to deal with:

```
git clone --branch v1.1 --depth 1 https://github.com/riscv-software-src/opensbi
```

Furthermore, to facilitate developing on different platforms, we replace the `application.system` with an `application.system.template` file where we can put in different input flags. This file features C preprocessor directives which our `Makefile` will later process.

We then use this (more sophisticated) Makefile which (among other slight changes) does the following when choosing `PLATFORM=CVA6`:
1. Hotfix the "typedef int bool" line from OpenSBI
2. Compile the application as an ELF using the microkit tool (results in `loader.elf`)
3. Turn this ELF into a stripped binary using `objdump`
4. Then compile OpenSBI with the binary as a payload (results in `payload.elf`)
5. We put the Microkit at `0x80200000` where it expects itself to be and OpenSBI at `0x80000000` where CVA6 expects it to be

In the following you find the full Makefile in question. It assumes that a `$CVA6` variable has been defined which points to the CVA6 repository.
And as said before, two programs (`server.c` and `client.c`) are assumed. 

You can compile to QEMU with:
`make PLATFORM=qemu-riscv`
Or to CVA6 with:
`make PLATFORM=cva6`
And you can clean all build directories with:
`make clean`.

Makefile
```
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
	
	
	# Copy the final payload to the build directory
	cp $(BUILD_DIR)/opensbi_out/platform/fpga/ariane/firmware/fw_payload.elf $(BUILD_DIR)/payload.elf
	
	# Remove the Hotpatch (for robustness reasons)
	@sed -i -E 's/\n#if __STDC_VERSION__ < 202311L\ntypedef int bool;\n#endif\n/typedef int bool;/' $(OPENSBI_DIR)/include/sbi/sbi_types.h
	
endif
```

Execution (on CVA6) is then done via:

```
$CVA6/work-ver/Variane_testharness build-cva6/payload.elf +time_out=10000000000000000 +tohost_addr=0x80001000
```

## 2. Troubleshooting
Unfortunately, the CVA6 setup as of yet does not work correctly. After starting the simulation, the OpenSBI Boot Screen appears and goes through, but after a few (~2) minutes (when SeL4 is trying to boot), the Testharness repeatedly throws this error:

```
%Warning: ariane_testharness.sv:676: Assertion failed in TOP.ariane_testharness.p_assert: B Response Errored
```

This corresponds to a protected area of memory being written to/read from, but why is unclear as of yet.

A look into the tracefile (`trace_hart_0.dasm`) reveals that OpenSBI does reach the Microkit (at 0x802...) and then it jumps to 0x900.. which is where the SeL4 Kernel itself will be placed by the Microkit. But then after the first few instructions of the Kernel itself, the simulation seems to fail and loop endlessly through the OpenSBI trap handler.

```
[...]
1561995 0x80200a28 S (0x00008082) DASM(00008082)
1561997 0x802000ba S (0x0000a011) DASM(0000a011)
1562006 0x802000be S (0x00006082) DASM(00006082)
1562006 0x802000c0 S (0x00000121) DASM(00000121)
1562007 0x802000c2 S (0x00008082) DASM(00008082)
1562013 0x802000dc S (0x00004585) DASM(00004585)
1562018 0x802000de S (0x00b50663) DASM(00b50663)
1562019 0x802000e2 S (0x000089aa) DASM(000089aa)
1562020 0x802000e4 S (0x00008522) DASM(00008522)
1562021 0x802000e6 S (0x000085ca) DASM(000085ca)
1562022 0x802000e8 S (0x00008982) DASM(00008982)
1562033 0x90000000 S (0x00002197) DASM(00002197) <--- Here it switches from the Microkit to the SeL4 Kernel
1562035 0x90000004 S (0xbd018193) DASM(bd018193)
1562036 0x90000008 S (0x0000842a) DASM(0000842a)
1562037 0x9000000a S (0x0000892e) DASM(0000892e)
1562038 0x9000000c S (0x00003117) DASM(00003117)
1562042 0x90000010 S (0x00410113) DASM(00410113)
1562044 0x90000014 S (0x000048c1) DASM(000048c1)
1562045 0x90000016 S (0x0000480d) DASM(0000480d)
1562046 0x90000018 S (0x00004541) DASM(00004541)
1562054 0x800004e8 M (0x34021273) DASM(34021273) <-- And then it seems to crash and jump back to the OpenSBI trap in M-mode
1562058 0x800004ec M (0x04523423) DASM(04523423) <-- (After that it never goes back to S-Mode)
1562059 0x800004f0 M (0x300022f3) DASM(300022f3)
[...]
```

Something seems to be going wrong when initializing SeL4. One possible explanation might be our Verilator setup (which seemingly "only" has 1GB RAM assigned to it) or there might be some overlap with the OpenSBI, CVA6 and SeL4 Kernel addresses.

We have tried the following (to no avail):
* Giving OpenSBI the ELF directly instead of the `.bin` (which seems like the recommended way, but here it doesnt even enter S-Mode)
* Giving OpenSBI a device tree of Ariane (which we compiled using the `.dts` file) which seemed to make no difference
* Other offsets than 0x200000 (0x400000) which also changed little
* Another approach where instead of combining everything into one ELF, we use OpenSBI in FW_JUMP mode and supply two ELFs to Variane_Testharness.

Maybe something useful can come about when investigating how SeL4Test builds for the CVA6 target.

