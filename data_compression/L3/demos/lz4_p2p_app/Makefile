#
# Copyright 2019 Xilinx, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# ------------------------------------------------------------
#						Help

help::
	$(ECHO) "Makefile Usage:"
	$(ECHO) "  make all TARGET=<sw_emu/hw_emu/hw> DEVICE=<FPGA platform>"
	$(ECHO) "      Command to generate the design for specified Target and Device."
	$(ECHO) ""
	$(ECHO) "  make run TARGET=<sw_emu/hw_emu/hw> DEVICE=<FPGA platform>"
	$(ECHO) "      Command to run application in emulation."
	$(ECHO) ""
	$(ECHO) "  make clean "
	$(ECHO) "      Command to remove the generated non-hardware files."
	$(ECHO) ""
	$(ECHO) "  make cleanall"
	$(ECHO) "      Command to remove all the generated files."
	$(ECHO) ""

# ------------------------------------------------------------
#						Build Environment Setup

include ./utils.mk
include ./config.mk

#check environment setup
ifndef XILINX_SDX
  $(error XILINX_SDX must be set by SDx\'s setting64.sh)
endif
ifndef XILINX_VIVADO
  $(error XILINX_VIVADO should have been set by SDx\'s setting64.sh)
endif
ifndef XILINX_XRT
  $(error XILINX_XRT must be set after 2018.3 release.)
endif
#ifndef PLATFORM_REPO_PATHS
#  $(error PLATFORM_REPO_PATHS must be defined.)
#endif

# sw_emu, hw_emu, hw
TARGET ?= sw_emu
ifeq ($(filter $(TARGET),sw_emu hw_emu hw),)
$(error TARGET is not sw_emu, hw_emu or hw)
endif

# Target device
DEVICE ?= xilinx_samsung_U2x4_201910_1

ifneq (,$(wildcard $(DEVICE)))
# Use DEVICE as a file path
XPLATFORM := $(DEVICE)
else
ifeq (,$(XPLATFORM))
define MSG_PLATFORM
No platform matched pattern '$(DEVICE)'.
endef
endif
endif

XDEVICE := $(basename $(notdir $(XPLATFORM)))

# Target platform
$(info XPLATFORM=$(XPLATFORM))

# ------------------------------------------------------------
#						Directory Setup

XF_PROJ_ROOT ?= $(CUR_DIR)/../../..
MK_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
CUR_DIR := $(patsubst %/,%,$(dir $(MK_PATH)))

BUILD_DIR := $(CUR_DIR)/build
TEMP_DIR := $(CUR_DIR)/_x_$(TARGET)_temp

XFLIB_DIR := $(shell readlink -f $(XF_PROJ_ROOT))


# ------------------------------------------------------------
#                      kernel setup

KSRC_DIR = $(XFLIB_DIR)/L2/src/

VPP = $(XILINX_SDX)/bin/xocc

#HLS src files
HLS_SRC_DIR = $(XFLIB_DIR)/L1/include/hw

# Compilation flags
VPP_FLAGS = -I$(HLS_SRC_DIR) \
			 -I$(KSRC_DIR) \
			 -I$(XFLIB_DIR)/L2/include/
VPP_FLAGS += -t $(TARGET) --platform $(XPLATFORM) --save-temps
VPP_FLAGS += --kernel_frequency=250 --xp param:compiler.enableAutoPipelining=false \
			  -DPARALLEL_BLOCK=$(PARALLEL_BLOCK)


VPP_DIRS = --temp_dir $(TEMP_DIR)/_x.$(TARGET) \
			--report_dir $(CUR_DIR)/reports/_x.$(TARGET)

# Linking flags
VPP_LINK_FLAGS = --optimize 2 --jobs 8 \
				  --xp vivado_param:project.writeIntermediateCheckpoints=1

VPP_LINK_DIRS = --temp_dir $(TEMP_DIR)/_build.$(TARGET)\
				 --report_dir $(CUR_DIR)/reports/_build.$(TARGET)/

XCLBIN_FILE_C = $(BUILD_DIR)/xclbin_$(XDEVICE)_$(TARGET)/compress.xclbin

XO_FILES = $(TEMP_DIR)/xf_compress.xo

PACKER_KERNEL_SRCS = $(KSRC_DIR)/lz4_packer_mm.cpp
COMPRESS_KERNEL_SRCS = $(KSRC_DIR)/lz4_compress_mm.cpp

PACKER_KERNEL_NAME = xil_lz4_packer_cu1
COMPRESS_KERNEL_NAME = xilLz4Compress

# ------------------------------------------------------------
#                      kernel rules

# Building kernel
$(TEMP_DIR)/xf_packer.xo: $(PACKER_KERNEL_SRCS) $(HLS_SRC_DIR)
	@echo -e "----\nCompiling packer kernel $*..."
	mkdir -p $(TEMP_DIR)
	$(VPP) $(VPP_FLAGS) $(VPP_DIRS) -c -k $(PACKER_KERNEL_NAME) -DC_COMPUTE_UNIT=1 -I'$(<D)' -o'$@' '$<'

$(TEMP_DIR)/xf_compress.xo: $(COMPRESS_KERNEL_SRCS) $(HLS_SRC_DIR)
	@echo -e "----\nCompiling compression kernel $*..."
	mkdir -p $(TEMP_DIR)
	$(VPP) $(VPP_FLAGS) $(VPP_DIRS) -c -k $(COMPRESS_KERNEL_NAME) -DC_COMPUTE_UNIT=1 -I'$(<D)' -o'$@' '$<'

# xclbin Binary creation
$(XCLBIN_FILE_C): $(TEMP_DIR)/xf_packer.xo $(TEMP_DIR)/xf_compress.xo
	mkdir -p $(BUILD_DIR)
	$(VPP) $(VPP_FLAGS) $(VPP_LINK_FLAGS) $(VPP_LINK_DIRS) -l \
	--profile_kernel data:all:all:all \
	--sp $(PACKER_KERNEL_NAME)_1.m_axi_gmem0:bank0 \
	--sp $(PACKER_KERNEL_NAME)_1.m_axi_gmem1:bank0 \
	--sp $(COMPRESS_KERNEL_NAME)_1.m_axi_gmem0:bank0 \
	--sp $(COMPRESS_KERNEL_NAME)_1.m_axi_gmem1:bank0 \
	--nk $(PACKER_KERNEL_NAME):1 --nk $(COMPRESS_KERNEL_NAME):1 \
	-o'$@' $(+)


# ------------------------------------------------------------
#                       host setup

CXX := $(XILINX_SDX)/bin/xcpp -g 
HOST_EXE := xil_lz4_8b
#EXE_EXT = exe

CXXFLAGS +=-I$(CUR_DIR)/src/
CXXFLAGS +=-I$(XFLIB_DIR)/L2/include/
CXXFLAGS +=-I$(XFLIB_DIR)/L3/demos/lz4_p2p_app/src
CXXFLAGS +=-I$(XILINX_XRT)/include/
CXXFLAGS +=-I$(XFLIB_DIR)/common/thirdParty/xxhash/
CXXFLAGS +=-I$(XFLIB_DIR)/common/libs/xcl2/
CXXFLAGS +=-I$(XFLIB_DIR)/common/libs/cmdparser/
CXXFLAGS +=-I$(XFLIB_DIR)/common/libs/logger/

#Host and Common sources
HOST_SRCS += $(CUR_DIR)/src/host.cpp
HOST_SRCS += $(CUR_DIR)/src/xil_lz4.cpp
HOST_SRCS += $(XFLIB_DIR)/common/thirdParty/xxhash/xxhash.c
HOST_SRCS += $(XFLIB_DIR)/common/libs/xcl2/xcl2.cpp
HOST_SRCS += $(XFLIB_DIR)/common/libs/cmdparser/cmdlineparser.cpp
HOST_SRCS += $(XFLIB_DIR)/common/libs/logger/logger.cpp

CXXFLAGS += -fmessage-length=0
		-DXDEVICE=$(XDEVICE) \
	    -Wall -Wno-unknown-pragmas -Wno-unused-label -pthread

LDFLAGS += -L$(XILINX_XRT)/lib/ -lOpenCL -pthread
LDFLAGS += -lrt -Wno-unused-label -Wno-narrowing -std=c++0x -DVERBOSE

EXE_FILE = $(BUILD_DIR)/$(HOST_EXE)


# ------------------------------------------------------------
#                       host rules

$(EXE_FILE): $(HOST_SRCS)
	@echo -e "----\nCompiling host $(notdir $@)..."
	mkdir -p $(BUILD_DIR)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS)


# ------------------------------------------------------------
#                      build rules

.PHONY: all help host xclbin cleanh cleank cleanall clean

all: host xclbin

host: $(EXE_FILE)

xclbin: $(XCLBIN_FILE_C)


cleank:
	rm -f $(TEMP_DIR)/*.xo
	rm -f $(BUILD_DIR)/*.xclbin
	rm -rf $(TEMP_DIR)/_x* $(TEMP_DIR)/.Xil $(TEMP_DIR)/sdaccel_profile_summary.*
	rm -rf $(TEMP_DIR)/dltmp* $(TEMP_DIR)/kernel_info.dat $(TEMP_DIR)/*.log reports/
	
cleanh:
	rm -rf $(EXE_FILE)
	rm -f $(EXE_FILE)
	rm -rf sdaccel_* TempConfig system_estimate.xtxt *.rpt .run/
	rm -rf src/*.ll _xocc_* .Xil dltmp* xmltmp* *.log *.jou *.wcfg *.wdb

cleanall: cleanh cleank
	rm -rf $(BUILD_DIR)
	rm -rf $(TEMP_DIR)
	rm -rf $(XFLIB_DIR)/L3/demos/lz4_p2p_app/data/*.xe2sd $(XFLIB_DIR)/L3/demos/lz4_p2p_app/data/*.lz4 

clean: cleanh


# ------------------------------------------------------------
#                      simulation run

$(BUILD_DIR)/emconfig.json :
		emconfigutil --platform $(XPLATFORM) --od $(BUILD_DIR)

HOST_ARGS = -cx $(XCLBIN_FILE_C) -l test.list
$(info HOST_ARGS=$(HOST_ARGS))

ifeq ($(TARGET),sw_emu)
  EMU_MODE = export XCL_EMULATION_MODE=sw_emu
  EMU_CONFIG = $(BUILD_DIR)/emconfig.json
else ifeq ($(TARGET),hw_emu)
  EMU_MODE = export XCL_EMULATION_MODE=hw_emu
  EMU_CONFIG = $(BUILD_DIR)/emconfig.json
else ifeq ($(TARGET),hw)
  EMU_MODE = echo "TARGET=hw"
  EMU_CONFIG =
endif


run: host xclbin $(EMU_CONFIG) $(DAT_STAMP)
	$(EMU_MODE); \
	$(EXE_FILE) $(HOST_ARGS)

