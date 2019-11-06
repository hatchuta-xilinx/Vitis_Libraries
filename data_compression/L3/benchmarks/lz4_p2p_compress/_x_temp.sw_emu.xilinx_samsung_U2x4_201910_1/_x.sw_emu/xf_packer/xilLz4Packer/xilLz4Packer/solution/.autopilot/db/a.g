#!/bin/sh
lli=${LLVMINTERP-lli}
exec $lli \
    /wrk/xhdhdnobkup1/hatchuta/git_repo/compression/samsung_p2p/compression_p2p/public_test/Vitis_Libraries/data_compression/L3/benchmarks/lz4_p2p_compress/_x_temp.sw_emu.xilinx_samsung_U2x4_201910_1/_x.sw_emu/xf_packer/xilLz4Packer/xilLz4Packer/solution/.autopilot/db/a.g.bc ${1+"$@"}
