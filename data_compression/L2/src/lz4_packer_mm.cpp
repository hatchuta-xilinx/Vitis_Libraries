/**********
 * Copyright (c) 2017, Xilinx, Inc.
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification,
 * are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * **********/
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "hls_stream.h"
#include <ap_int.h>

#ifndef PARALLEL_BLOCK
#define PARALLEL_BLOCK 8
#endif

#define GMEM_DWIDTH 512
#define GMEM_BURST_SIZE 16
#define BLOCK_PARITION 1024
#define MARKER 255
#define MAX_LIT_COUNT 4096 
const int c_gmem_burst_size = (2*GMEM_BURST_SIZE);
const int c_size_stream_depth = 8;
const int max_literal_count = MAX_LIT_COUNT;


// Pack width matching 
#define PACK_WIDTH 64

// Byte length
#define PARLLEL_BYTE 8

typedef ap_uint<GMEM_DWIDTH> uint512_t;
typedef ap_uint<PACK_WIDTH>  uintV_t; // 64bit input stream
#define MAGIC_BYTE_1 4
#define MAGIC_BYTE_2 34
#define MAGIC_BYTE_3 77
#define MAGIC_BYTE_4 24
#define FLG_BYTE 104

// Below are the codes as per LZ4 standard for 
// various maximum block sizes supported.
#define BSIZE_STD_64KB 64
#define BSIZE_STD_256KB 80
#define BSIZE_STD_1024KB 96
#define BSIZE_STD_4096KB 112


template <int DATAWIDTH, int BURST_SIZE>
void mm2s(const uint512_t *in,
          uint512_t *head_prev_blk,
          uint512_t *orig_input_data,
          hls::stream<ap_uint<DATAWIDTH> > &outStream,
          hls::stream<uint32_t> &outStreamSize,
          uint32_t *compressd_size,
          uint32_t *in_block_size,
          uint32_t no_blocks,
          uint32_t block_size_in_kb,
          uint32_t head_res_size,
          uint32_t offset  
         )
{
    const int c_byte_size = 8;
    const int c_word_size = DATAWIDTH / c_byte_size;
    ap_uint<DATAWIDTH> buffer[BURST_SIZE];
    #pragma HLS RESOURCE variable=buffer core=RAM_2P_LUTRAM

    uint32_t offset_gmem = offset?offset/64:0;

    // Handle header or residue here
    uint32_t block_stride = block_size_in_kb * 1024 / 64; 

    uint32_t blkCompSize = 0;
    uint32_t origSize = 0;
    uint32_t sizeInWord = 0;
    uint32_t byteSize = 0; 
    // Run over number of blocks
    for (int bIdx = 0; bIdx < no_blocks + 1; bIdx++) {
     
        if (bIdx == 0) {
            sizeInWord = head_res_size?((head_res_size - 1) / c_word_size + 1):0;
            byteSize = head_res_size;
        } else {  
            blkCompSize = compressd_size[bIdx - 1];
            origSize    = in_block_size[bIdx - 1];
            // Put compress block & input block
            // into streams for next block
            sizeInWord = (blkCompSize - 1) / c_word_size + 1;
            byteSize = blkCompSize;
        }

        //Send size in bytes
        outStreamSize << byteSize;
        
        //printf("[ %s ]blkCompSize %d origSize %d sizeInWord_512 %d offset %d head_res_size %d\n", __FUNCTION__, blkCompSize, origSize, sizeInWord, offset, head_res_size);

        // Copy data from global memory to local
        // Put it into stream
        for (uint32_t i = 0; i < sizeInWord; i+=BURST_SIZE) {
            uint32_t chunk_size = BURST_SIZE;
        
            if (i + BURST_SIZE > sizeInWord)
                chunk_size = sizeInWord - i;

            memrd1: for (uint32_t j = 0; j < chunk_size; j++) {
            #pragma HLS PIPELINE II=1
                    if (bIdx == 0) 
                        buffer[j] = head_prev_blk[(offset_gmem + i) + j];
                    else if (blkCompSize == origSize)
                        buffer[j] = orig_input_data[(block_stride * (bIdx - 1) + i) + j];
                    else 
                        buffer[j] = in[(block_stride * (bIdx - 1) + i) + j];
            }
   
            memrd2: for (uint32_t j = 0; j < chunk_size; j++) {
            #pragma HLS PIPELINE II=1
                outStream << buffer[j];
            }    
        }
    }
    //printf("%s Done \n", __FUNCTION__); 
    outStreamSize << 0;
}

template <int IN_WIDTH, int OUT_WIDTH>
void streamDownSizer(hls::stream<ap_uint<IN_WIDTH> > &inStream,
                     hls::stream<ap_uint<OUT_WIDTH> > &outStream,
                     hls::stream<uint32_t> &inStreamSize,
                     hls::stream<uint32_t> &outStreamSize,
                     uint32_t no_blocks
                    )
{
    const int c_byte_width = 8;
    const int c_input_word = IN_WIDTH / c_byte_width;
    const int c_out_word = OUT_WIDTH / c_byte_width;

    int factor = c_input_word / c_out_word;
    ap_uint<IN_WIDTH> inBuffer = 0;

    for (int size = inStreamSize.read(); size != 0; size = inStreamSize.read()) {
        
        // input size interms of 512width * 64 bytes after downsizing
        uint32_t sizeOutputV = (size - 1) / c_out_word + 1;
        
        // Send ouputSize of the module
        outStreamSize << size; 

 //       printf("[ %s ] sizeOutputV %d input_size %d size_4m_mm2s %d \n", __FUNCTION__, sizeOutputV, input_size, size);

        conv512toV: for (int i = 0; i < sizeOutputV; i++) {
        #pragma HLS PIPELINE II=1
            int idx = i % factor;
            if (idx == 0) inBuffer = inStream.read();
            ap_uint<OUT_WIDTH> tmpValue = inBuffer.range((idx+1)*PACK_WIDTH - 1, idx * PACK_WIDTH);
            outStream << tmpValue;
        }
    }
    //printf("%s Done \n", __FUNCTION__); 
    //outStreamSize << 0;
}

uint32_t  packer(hls::stream<ap_uint<PACK_WIDTH> > &inStream,
            hls::stream<ap_uint<PACK_WIDTH> > &outStream,
            hls::stream<uint32_t> &inStreamSize,
            hls::stream<uint32_t> &outStreamSize,
            uint32_t block_size_in_kb,
            uint32_t no_blocks,
            uint32_t head_res_size,
            uint32_t tail_bytes
           )
{
    // 16 bytes can be held in on shot
    ap_uint<2 * PACK_WIDTH> lcl_buffer;
    uint32_t lbuf_idx = 0;
    
    uint32_t cBlen = 4;

    uint32_t endSizeCnt = 0;
    uint32_t sizeOutput = 0;
    uint32_t word_head = 0;
    uint32_t over_size = 0;
    uint32_t flag  = 0;    
    uint32_t size_cntr = 0;
    // Core packer logic
    packer:for (int blkIdx = 0; blkIdx < no_blocks + 1; blkIdx++) {
       
        uint32_t size = inStreamSize.read(); 
        //printf("lbuf %d \n", lbuf_idx); 
        // Find out the compressed size header
        // This value is sent by mm2s module
        // by using compressed_size[] buffer picked from 
        // LZ4 compression kernel
        if (blkIdx == 0) {
            uint32_t word_head;
            if(head_res_size < 8)
               word_head = 1;
            else
               word_head = head_res_size / 8;
            sizeOutput = word_head;
            endSizeCnt += head_res_size;
        }
        else {
            // Size is nothing but 8bytes * 8 gives original input
            over_size = lbuf_idx + cBlen + size; 
            endSizeCnt += cBlen + size;
            // 64bit size value including headers etc
            sizeOutput = over_size / 8; 
        }
        //printf("%s - trailSize %d size %d lbuf %d word_64size %d over_size_pblick %d \n", __FUNCTION__, trailSize, size, lbuf_idx, sizeOutput, over_size);
        // Send the size of output to next block
        outStreamSize << sizeOutput;

        if (blkIdx != 0) {    
            // Update local buffer with compress size of current block - 4Bytes 
            lcl_buffer.range((lbuf_idx * 8) + 8 -1, lbuf_idx*8) = size;lbuf_idx++;
            lcl_buffer.range((lbuf_idx * 8) + 8 -1, lbuf_idx*8) = size >> 8;lbuf_idx++;
            lcl_buffer.range((lbuf_idx * 8) + 8 -1, lbuf_idx*8) = size >> 16;lbuf_idx++;
            lcl_buffer.range((lbuf_idx * 8) + 8 -1, lbuf_idx*8) = size >> 24;lbuf_idx++;
        }
        
        if (lbuf_idx >= 8) {
            outStream << lcl_buffer.range(63,0);
            lcl_buffer >>= PACK_WIDTH;
            lbuf_idx -= 8;
        }
        //printf("%s size %d sizeOutput %d \n", __FUNCTION__, size, sizeOutput);

        uint32_t chunk_size = 0;
        // Packer logic
        // Read compressed data of a block
        // Stream it to upsizer 
        pack_post: for (int i = 0; i < size; i += PARLLEL_BYTE) {
        #pragma HLS PIPELINE II=1
            
            if (i + chunk_size > size)                
                chunk_size = size - i;
            else 
                chunk_size = PARLLEL_BYTE;

            // Update local buffer with new set of data
            // Update local buffer index
            lcl_buffer.range((lbuf_idx * 8) + PACK_WIDTH - 1, lbuf_idx * 8) = inStream.read(); 
            lbuf_idx+=chunk_size;

            if (lbuf_idx >= 8) {
                outStream << lcl_buffer.range(63,0);
                lcl_buffer >>= PACK_WIDTH;
                lbuf_idx -= 8;
            }
        } // End of main packer loop
        
    } 
  

    //printf("End of packer \n");    
    if (tail_bytes) {
        // Trailing bytes based on LZ4 standard
        lcl_buffer.range((lbuf_idx * 8) + 8 -1, lbuf_idx*8) = 0;lbuf_idx++;
        lcl_buffer.range((lbuf_idx * 8) + 8 -1, lbuf_idx*8) = 0;lbuf_idx++;
        lcl_buffer.range((lbuf_idx * 8) + 8 -1, lbuf_idx*8) = 0;lbuf_idx++;
        lcl_buffer.range((lbuf_idx * 8) + 8 -1, lbuf_idx*8) = 0;lbuf_idx++;
    }
    //printf("flag %d lbuf_idx %d\n", flag, lbuf_idx); 


    uint32_t extra_size = (lbuf_idx - 1) / 8 + 1;
    if (lbuf_idx) outStreamSize << extra_size;
    
    while(lbuf_idx) {
        
        //printf("Ssent the data \n"); 
        outStream << lcl_buffer.range(63,0);
        
        if (lbuf_idx >= 8) {
            lcl_buffer >>= PACK_WIDTH;
            lbuf_idx -= 8;
        } else {
            lbuf_idx = 0;    
        }
    }
    
    //printf("%s Done \n", __FUNCTION__); 
    // Termination condition of 
    // next block
    outStreamSize << 0;
    //printf("endsizeCnt %d \n", endSizeCnt);
    return endSizeCnt;
}

void streamUpSizer(hls::stream<uintV_t> &inStream,
                   hls::stream<ap_uint<512> > &outStream,
                   hls::stream<uint32_t> &inStreamSize,
                   hls::stream<uint32_t> &outStreamSize
                  ) 
{
    const int c_byte_width = 8;
    const int c_upsize_factor = GMEM_DWIDTH / c_byte_width;
    const int c_in_size = PACK_WIDTH / c_byte_width;
    
    // Declaring double buffers
    ap_uint<2 * GMEM_DWIDTH> outBuffer = 0; 
    uint32_t byteIdx = 0;
    
    for (int size = inStreamSize.read(); size != 0; size = inStreamSize.read()) {
        //printf("Size %d \n", size);
        // Rounding off the output size
        uint32_t outSize = (size * c_byte_width + byteIdx) / PACK_WIDTH;
        
        if (outSize) 
            outStreamSize << outSize;
        streamUpsizer:for (int i = 0; i < size; i++) {
        #pragma HLS PIPELINE II=1
            //printf("val/size %d/%d \n", i, size);
            ap_uint<PACK_WIDTH> tmpValue = inStream.read();
            outBuffer.range((byteIdx + c_in_size) * c_byte_width - 1, byteIdx * c_byte_width) = tmpValue;
            byteIdx += c_byte_width;
            
            if (byteIdx >= c_upsize_factor) {
                outStream << outBuffer.range(GMEM_DWIDTH - 1, 0);
                outBuffer >>= GMEM_DWIDTH;
                byteIdx -= c_upsize_factor;
            }

        }
    }
   
    if (byteIdx) {
        outStreamSize << 1;
        outStream << outBuffer.range(GMEM_DWIDTH - 1, 0);
    }
    //printf("%s Done \n", __FUNCTION__); 
    // end of block
    outStreamSize << 0;
}

void s2mm(hls::stream<ap_uint<512> > &inStream,
              uint512_t *out,
              hls::stream<uint32_t> &inStreamSize
             )
{
    const int c_byte_size = 8;
    const int c_factor = GMEM_DWIDTH / c_byte_size;
    
    uint32_t outIdx = 0;
    uint32_t size = 1;
    uint32_t sizeIdx = 0;

    for (int size = inStreamSize.read(); size != 0;size = inStreamSize.read() ) {
        
        mwr:for (int i = 0; i < size; i++) {
        #pragma HLS PIPELINE II=1
            out[outIdx + i] = inStream.read();
        }
        outIdx += size;
    }
}

void lz4comp(const uint512_t *in,
             uint512_t *out,
             uint512_t *head_prev_blk,
             uint32_t *compressd_size, 
             uint32_t *in_block_size,
             uint32_t *encoded_size,
             uint512_t *orig_input_data,
             uint32_t no_blocks,
             uint32_t head_res_size,
             uint32_t offset,
             uint32_t block_size_in_kb,
             uint32_t tail_bytes
            )
{
    hls::stream<uint512_t> inStream512("inStream512_mm2s");
    hls::stream<uintV_t> inStreamV("inStreamV_dsizer");
    hls::stream<uintV_t> packStreamV("packerStreamOut");
    hls::stream<uint512_t> outStream512("UpsizeStreamOut");
    #pragma HLS STREAM variable=inStream512 depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStreamV depth=c_gmem_burst_size
    #pragma HLS STREAM variable=packStreamV depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStream512 depth=c_gmem_burst_size

    #pragma HLS RESOURCE variable=inStream512 core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStreamV core=FIFO_SRL
    #pragma HLS RESOURCE variable=packStreamV core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStream512 core=FIFO_SRL
    
    hls::stream<uint32_t> mm2sStreamSize("mm2sOutSize");
    hls::stream<uint32_t> downStreamSize("dstreamOutSize");
    hls::stream<uint32_t> packStreamSize("packOutSize");
    hls::stream<uint32_t> upStreamSize("upStreamSize");
    #pragma HLS STREAM variable=mm2sStreamSize depth=c_gmem_burst_size
    #pragma HLS STREAM variable=downStreamSize depth=c_gmem_burst_size
    #pragma HLS STREAM variable=packStreamSize depth=c_gmem_burst_size
   
    #pragma HLS RESOURCE variable=mm2sStreamSize core=FIFO_SRL
    #pragma HLS RESOURCE variable=downStreamSize core=FIFO_SRL
    #pragma HLS RESOURCE variable=packStreamSize core=FIFO_SRL
    #pragma HLS RESOURCE variable=upStreamSize   core=FIFO_SRL
 
    #pragma HLS dataflow
    mm2s<GMEM_DWIDTH,GMEM_BURST_SIZE>(in, head_prev_blk, orig_input_data, inStream512, mm2sStreamSize, compressd_size, in_block_size, no_blocks, block_size_in_kb, head_res_size, offset);
    streamDownSizer<GMEM_DWIDTH,PACK_WIDTH>(inStream512, inStreamV, mm2sStreamSize, downStreamSize, no_blocks);
    encoded_size[0] = packer(inStreamV, packStreamV, downStreamSize, packStreamSize, block_size_in_kb, no_blocks, head_res_size, tail_bytes);
    streamUpSizer(packStreamV, outStream512, packStreamSize, upStreamSize);
    s2mm(outStream512, out, upStreamSize);
}

extern "C" {
void xil_lz4_packer_cu1
(
                const uint512_t *in,      
                uint512_t       *out,          
                uint512_t       *head_prev_blk,          
                uint32_t        *compressd_size,
                uint32_t        *in_block_size,
                uint32_t        *encoded_size,
                uint512_t        *orig_input_data,
                uint32_t        head_res_size,
                uint32_t        offset,
                uint32_t block_size_in_kb,
                uint32_t no_blocks,
                uint32_t tail_bytes                      
                )
{
    #pragma HLS INTERFACE m_axi port=in offset=slave bundle=gmem0
    #pragma HLS INTERFACE m_axi port=out offset=slave bundle=gmem0
    #pragma HLS INTERFACE m_axi port=head_prev_blk offset=slave bundle=gmem0
    #pragma HLS INTERFACE m_axi port=compressd_size offset=slave bundle=gmem1
    #pragma HLS INTERFACE m_axi port=in_block_size offset=slave bundle=gmem1
    #pragma HLS INTERFACE m_axi port=encoded_size offset=slave bundle=gmem1
    #pragma HLS INTERFACE m_axi port=orig_input_data offset=slave bundle=gmem0
    #pragma HLS INTERFACE s_axilite port=in bundle=control
    #pragma HLS INTERFACE s_axilite port=out bundle=control
    #pragma HLS INTERFACE s_axilite port=head_prev_blk bundle=control
    #pragma HLS INTERFACE s_axilite port=compressd_size bundle=control
    #pragma HLS INTERFACE s_axilite port=in_block_size bundle=control
    #pragma HLS INTERFACE s_axilite port=encoded_size bundle=control
    #pragma HLS INTERFACE s_axilite port=orig_input_data bundle=control
    #pragma HLS INTERFACE s_axilite port=head_res_size bundle=control
    #pragma HLS INTERFACE s_axilite port=offset bundle=control
    #pragma HLS INTERFACE s_axilite port=block_size_in_kb bundle=control
    #pragma HLS INTERFACE s_axilite port=no_blocks bundle=control
    #pragma HLS INTERFACE s_axilite port=tail_bytes bundle=control
    #pragma HLS INTERFACE s_axilite port=return bundle=control

    lz4comp(in, out, head_prev_blk, compressd_size, in_block_size, encoded_size, orig_input_data, no_blocks, head_res_size, offset, block_size_in_kb, tail_bytes);
    
    return;
}
}
