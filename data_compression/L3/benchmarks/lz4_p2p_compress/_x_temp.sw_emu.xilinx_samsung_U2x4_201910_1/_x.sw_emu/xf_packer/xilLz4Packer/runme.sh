#!/bin/sh

# 
# xocc(TM)
# runme.sh: a xocc-generated Runs Script for UNIX
# Copyright 1986-2019 Xilinx, Inc. All Rights Reserved.
# 

if [ -z "$PATH" ]; then
  PATH=/proj/xbuilds/2019.1_0524_1430/installs/lin64/Vivado/2019.1/bin:/proj/xbuilds/2019.1_0524_1430/installs/lin64/SDK/2019.1/bin:/proj/xbuilds/2019.1_0524_1430/installs/lin64/SDx/2019.1/bin
else
  PATH=/proj/xbuilds/2019.1_0524_1430/installs/lin64/Vivado/2019.1/bin:/proj/xbuilds/2019.1_0524_1430/installs/lin64/SDK/2019.1/bin:/proj/xbuilds/2019.1_0524_1430/installs/lin64/SDx/2019.1/bin:$PATH
fi
export PATH

if [ -z "$LD_LIBRARY_PATH" ]; then
  LD_LIBRARY_PATH=
else
  LD_LIBRARY_PATH=:$LD_LIBRARY_PATH
fi
export LD_LIBRARY_PATH

HD_PWD='/wrk/xhdhdnobkup1/hatchuta/git_repo/compression/samsung_p2p/compression_p2p/public_test/Vitis_Libraries/data_compression/L3/benchmarks/lz4_p2p_compress/_x_temp.sw_emu.xilinx_samsung_U2x4_201910_1/_x.sw_emu/xf_packer/xilLz4Packer'
cd "$HD_PWD"

HD_LOG=runme.log
/bin/touch $HD_LOG

ISEStep="./ISEWrap.sh"
EAStep()
{
     $ISEStep $HD_LOG "$@" >> $HD_LOG 2>&1
     if [ $? -ne 0 ]
     then
         exit
     fi
}

EAStep vivado_hls -f xilLz4Packer.tcl -messageDb vivado_hls.pb
