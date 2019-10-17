/**********
 * Copyright (c) 2018, Xilinx, Inc.
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
#pragma once
#include "defns.h"

// Maximum compute units supported
#define MAX_COMPUTE_UNITS 1

// Maximum host buffer used to operate
// per kernel invocation
#define HOST_BUFFER_SIZE (100*1024*1024)

// Default block size
#define BLOCK_SIZE_IN_KB 64 

// Value below is used to associate with
// Overlapped buffers, ideally overlapped 
// execution requires 2 resources per invocation
#define OVERLAP_BUF_COUNT 1 

// Maximum number of blocks based on host buffer size
#define MAX_NUMBER_BLOCKS (HOST_BUFFER_SIZE / (BLOCK_SIZE_IN_KB * 1024))

// Below are the codes as per LZ4 standard for 
// various maximum block sizes supported.
#define BSIZE_STD_64KB 64
#define BSIZE_STD_256KB 80
#define BSIZE_STD_1024KB 96
#define BSIZE_STD_4096KB 112

// Maximum block sizes supported by LZ4
#define MAX_BSIZE_64KB 65536
#define MAX_BSIZE_256KB 262144
#define MAX_BSIZE_1024KB 1048576
#define MAX_BSIZE_4096KB 4194304

// This value is used to set 
// uncompressed block size value
// 4th byte is always set to below 
// and placed as uncompressed byte
#define NO_COMPRESS_BIT 128

// In case of uncompressed block 
// Values below are used to set
// 3rd byte to following values
// w.r.t various maximum block sizes 
// supported by standard
#define BSIZE_NCOMP_64 1
#define BSIZE_NCOMP_256 4
#define BSIZE_NCOMP_1024 16
#define BSIZE_NCOMP_4096 64

int validate(std::string & inFile_name, std::string & outFile_name);

static uint32_t get_file_size(std::ifstream &file){
    file.seekg(0,file.end);
    uint32_t file_size = file.tellg();
    file.seekg(0,file.beg);
    return file_size;
}

class xil_lz4 {
    public:
        int init(const std::string& binaryFile, uint8_t);
        int release();
        uint32_t compress(uint8_t *in, uint8_t *out, uint32_t actual_size);
        uint32_t compress_file(std::string & inFile_name, std::string & outFile_name, uint32_t input_size); 
        uint32_t decompress_file(std::string & inFile_name, std::string & outFile_name);
        uint64_t get_event_duration_ns(const cl::Event &event);
        void buffer_extension_assignments(bool flow);
        // Binary flow compress/decompress        
        bool m_bin_flow;        
        
        // Block Size
        uint32_t m_block_size_in_kb;       

        // Switch between FPGA/Standard flows
        bool m_switch_flow;
 
        xil_lz4();
        ~xil_lz4();
    private:
        cl::Program *m_program;
        cl::Context *m_context;
        cl::CommandQueue *m_q;
        cl::Kernel* compress_kernel_lz4;
        cl::Kernel* packer_kernel_lz4;

        // Compression related
        std::vector<uint8_t, aligned_allocator<uint8_t>>  h_buf_in;
        std::vector<uint8_t, aligned_allocator<uint8_t>>  h_buf_out;
        // LZ4 stream output
        std::vector<uint8_t, aligned_allocator<uint8_t>>  h_enc_out;
        std::vector<uint32_t, aligned_allocator<uint8_t>> h_blksize;
        std::vector<uint32_t, aligned_allocator<uint8_t>> h_compressSize;
        // LZ4 compress stream size out
        std::vector<uint32_t, aligned_allocator<uint8_t>> h_lz4OutSize;

        // Header bufffer
        std::vector<uint8_t, aligned_allocator<uint8_t>>  h_header;

        
        // Device buffers
        cl::Buffer* buffer_input;
        cl::Buffer* buffer_output;
        cl::Buffer* buffer_lz4out;
        cl::Buffer* buffer_compressed_size;
        cl::Buffer* buffer_block_size;
        cl::Buffer* buffer_lz4OutSize;
        cl::Buffer* buffer_header;
        
        // Decompression related
        std::vector<uint32_t> m_blkSize;
        std::vector<uint32_t> m_compressSize;
        std::vector<bool>     m_is_compressed;        
        
        // DDR buffer extensions
        cl_mem_ext_ptr_t inExt;
        cl_mem_ext_ptr_t outExt;
        cl_mem_ext_ptr_t lz4Ext;
        cl_mem_ext_ptr_t csExt;
        cl_mem_ext_ptr_t bsExt;
        cl_mem_ext_ptr_t lz4SizeExt;
        cl_mem_ext_ptr_t headExt;
        
        // Kernel names 
        std::vector<std::string> compress_kernel_names = {"xilLz4Compress"
                                                         };

        std::vector<std::string> packer_kernel_names = {"xil_lz4_packer_cu1"
                                                       };

        // DDR numbers
        std::vector<uint32_t> comp_ddr_nums = {XCL_MEM_DDR_BANK0
                                        };
};
