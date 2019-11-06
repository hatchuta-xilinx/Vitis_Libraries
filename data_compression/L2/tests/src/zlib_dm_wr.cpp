/*
 * (c) Copyright 2019 Xilinx, Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
/**
 * @file zlib_dm_wr.cpp
 * @brief Source for data writer kernel for streaming data to zlib decompression streaming kernel.
 *
 * This file is part of Vitis Data Compression Library.
 */

#include "zlib_dm_wr.hpp"

const int kGMemBurstSize = 16;

template <uint16_t STREAMDWIDTH>
void streamDataDm2kSync(hls::stream<ap_uint<STREAMDWIDTH> >& in,
                        hls::stream<ap_axiu<STREAMDWIDTH, 0, 0, 0> >& inStream_dm,
                        uint32_t inputSize) {
    // read data from input hls to input stream for decompression kernel
    uint32_t itrLim = 1 + (inputSize - 1) / (STREAMDWIDTH / 8);
streamDataDm2kSync:
    for (uint32_t i = 0; i < itrLim; i++) {
#pragma HLS PIPELINE II = 1
        ap_uint<STREAMDWIDTH> temp = in.read();
        ap_axiu<STREAMDWIDTH, 0, 0, 0> dataIn;
        dataIn.data = temp; // kernel to kernel data transfer
        inStream_dm.write(dataIn);
    }
    // printf("DM2K Done\n");
}

void zlib_dm_wr(uintMemWidth_t* in, uint32_t input_size, hls::stream<ap_axiu<kGMemDWidth, 0, 0, 0> >& instream) {
    hls::stream<uintMemWidth_t> instream512("inputStream");
#pragma HLS STREAM variable = instream512 depth = 32

#pragma HLS dataflow
    xf::compression::mm2sSimple<kGMemDWidth, 16>(in, instream512, input_size);

    streamDataDm2kSync<kGMemDWidth>(instream512, instream, input_size);
}

extern "C" {
void xilZlibDmWriter(uintMemWidth_t* in, uint32_t input_size, hls::stream<ap_axiu<kGMemDWidth, 0, 0, 0> >& instreamk) {
#pragma HLS INTERFACE m_axi port = in offset = slave bundle = gmem
#pragma HLS interface axis port = instreamk
#pragma HLS INTERFACE s_axilite port = in bundle = control
#pragma HLS INTERFACE s_axilite port = input_size bundle = control
#pragma HLS INTERFACE s_axilite port = return bundle = control

    ////printme("In datamover kernel \n");
    // Call for compression
    zlib_dm_wr(in, input_size, instreamk);
}
}
