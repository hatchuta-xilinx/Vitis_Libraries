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
#include "xil_lz4.h"
#include "xxhash.h"
#define BLOCK_SIZE 64
#define KB 1024
#define MAGIC_HEADER_SIZE 4
#define MAGIC_BYTE_1 4
#define MAGIC_BYTE_2 34
#define MAGIC_BYTE_3 77
#define MAGIC_BYTE_4 24
#define FLG_BYTE 104

#define RESIDUE_4K 4096

/* File descriptors to open the input and output files with O_DIRECT option
 * These descriptors used in P2P case only
 */

uint64_t xil_lz4::get_event_duration_ns(const cl::Event &event){

    uint64_t start_time=0, end_time=0;

    event.getProfilingInfo<uint64_t>(CL_PROFILING_COMMAND_START, &start_time);
    event.getProfilingInfo<uint64_t>(CL_PROFILING_COMMAND_END, &end_time);
    return (end_time - start_time);
}

// Constructor
xil_lz4::xil_lz4(const std::string& binaryFileName, uint8_t device_id){
            // Index calculation
#if 0
            h_blksize.resize(MAX_NUMBER_BLOCKS);
            h_lz4OutSize[0].resize(RESIDUE_4K);
            h_lz4OutSize[1].resize(RESIDUE_4K);
            h_header.resize(RESIDUE_4K);
#endif
#if 1
             // The get_xil_devices will return vector of Xilinx Devices 
    std::vector<cl::Device> devices = xcl::get_xil_devices();

	/* Multi board support: selecting the right device based on the device_id,
	 * provided through command line args (-id <device_id>).
	 */
	if (devices.size() < device_id) {
		std::cout << "Identfied devices = " << devices.size() << ", given device id = " << unsigned(device_id) << std::endl;
		std::cout << "Error: Device ID should be within the range of number of Devices identified" << std::endl;
		std::cout << "Program exited.." << std::endl;
		exit(1);
	}
   	devices.at(0) = devices.at(device_id);

    cl::Device device = devices.at(0);

    //Creating Context and Command Queue for selected Device 
    m_context = new cl::Context(device);
    m_q = new cl::CommandQueue(*m_context, device, 
            CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_PROFILING_ENABLE);
    std::string device_name = device.getInfo<CL_DEVICE_NAME>(); 
    std::cout << "Found Device=" << device_name.c_str() << ", device id = " << unsigned(device_id) << std::endl;

    // import_binary() command will find the OpenCL binary file created using the 
    // xocc compiler load into OpenCL Binary and return as Binaries
    // OpenCL and it can contain many functions which can be executed on the
    // device.
    
    auto fileBuf = xcl::read_binary_file(binaryFileName.c_str());
    cl::Program::Binaries bins{{fileBuf.data(), fileBuf.size()}};
    devices.resize(1);

    m_program = new cl::Program(*m_context, devices, bins);
    
   
    // Create Compress kernels
    compress_kernel_lz4 = new cl::Kernel(*m_program, compress_kernel_names[0].c_str());
    packer_kernel_lz4 = new cl::Kernel(*m_program, packer_kernel_names[0].c_str());
#endif
}   

// Destructor
xil_lz4::~xil_lz4(){
    delete(compress_kernel_lz4);
    delete(m_program);
    delete(m_q);
    delete(m_context);
}

// This version of compression does overlapped execution between
// Kernel and Host. I/O operations between Host and Device are
// overlapped with Kernel execution between multiple compute units
void xil_lz4::compress_in_line_multiple_files(std::vector<char *> &inVec,
                           const std::vector<std::string> &outFileVec,
                           std::vector<uint32_t>   &inSizeVec,
                           bool enable_p2p
                         ) {
    std::vector<cl::Buffer*> bufInputVec;
    std::vector<cl::Buffer*> bufOutputVec;
    std::vector<cl::Buffer*> buflz4OutVec;
    std::vector<cl::Buffer*> buflz4OutSizeVec;
    std::vector<cl::Buffer*> bufblockSizeVec;
    std::vector<cl::Buffer*> bufCompSizeVec;
    std::vector<cl::Buffer*> bufheadVec;
    std::vector<uint8_t*> bufp2pOutVec;

    std::vector<uint8_t*> h_headerVec;
    std::vector<uint32_t*> h_blkSizeVec;
    std::vector<uint32_t*> h_lz4OutSizeVec;

    int ret = 0;

    uint64_t total_kernel_time = 0;
    uint64_t total_packer_kernel_time = 0;

    auto total_start = std::chrono::high_resolution_clock::now();
    for (uint32_t i = 0; i < inVec.size(); i++) {
        // Default value set to 64K
        uint8_t block_size_header = 0;
        switch(m_block_size_in_kb) {

            case 64:
                    block_size_header = BSIZE_STD_64KB;
                    break;
            case 256:
                     block_size_header = BSIZE_STD_256KB;
                     break;
            case 1024:
                      block_size_header = BSIZE_STD_1024KB;
                      break;
            case 4096:
                      block_size_header = BSIZE_STD_4096KB;
                      break;
            default:
                    block_size_header = BSIZE_STD_64KB;
                    std::cout << "Valid block size not given, setting to 64K"<<std::endl;
                    break;
        }

        uint8_t temp_buff[10] = {FLG_BYTE,
                                 block_size_header,
                                 inSizeVec[i],
                                 inSizeVec[i] >> 8,
                                 inSizeVec[i] >> 16,
                                 inSizeVec[i] >> 24,
                                 0,0,0,0
                                };

        // xxhash is used to calculate hash value
        uint32_t xxh = XXH32(temp_buff, 10, 0);
        // This value is sent to Kernel 2
        uint32_t xxhash_val = (xxh>>8);


        uint32_t block_size_in_bytes = m_block_size_in_kb * 1024;

        // Header information
        uint32_t head_size = 0;
        uint8_t* h_header = (uint8_t*) aligned_alloc(4096,200);
        uint32_t* h_blksize = (uint32_t*) aligned_alloc(4096,200);
        uint32_t* h_lz4outSize = (uint32_t*) aligned_alloc(4096,200);

        h_header[head_size++] = MAGIC_BYTE_1;
        h_header[head_size++] = MAGIC_BYTE_2;
        h_header[head_size++] = MAGIC_BYTE_3;
        h_header[head_size++] = MAGIC_BYTE_4;

        h_header[head_size++] = FLG_BYTE;

        // Value
        switch(m_block_size_in_kb) {
            case 64:h_header[head_size++]   = BSIZE_STD_64KB;  break;
            case 256:h_header[head_size++]  = BSIZE_STD_256KB; break;
            case 1024:h_header[head_size++] = BSIZE_STD_1024KB;break;
            case 4096:h_header[head_size++] = BSIZE_STD_4096KB;break;
        }

        // Input size
        h_header[head_size++] = inSizeVec[i];
        h_header[head_size++] = inSizeVec[i] >> 8;
        h_header[head_size++] = inSizeVec[i] >> 16;
        h_header[head_size++] = inSizeVec[i] >> 24;
        h_header[head_size++] = 0;
        h_header[head_size++] = 0;
        h_header[head_size++] = 0;
        h_header[head_size++] = 0;

        // XXHASH value 
        h_header[head_size++] = xxhash_val;
        h_headerVec.push_back(h_header);
        h_blkSizeVec.push_back(h_blksize);
        h_lz4OutSizeVec.push_back(h_lz4outSize);

        // Total chunks in input file
        // For example: Input file size is 12MB and Host buffer size is 2MB
        // Then we have 12/2 = 6 chunks exists 
        // Calculate the count of total chunks based on input size
        // This count is used to overlap the execution between chunks and file
        // operations
        
        uint32_t num_blocks = (inSizeVec[i] - 1)/block_size_in_bytes + 1;

        // DDR buffer extensions
        cl_mem_ext_ptr_t lz4Ext;
        lz4Ext.flags = XCL_MEM_DDR_BANK0 | XCL_MEM_EXT_P2P_BUFFER;
        lz4Ext.param   = NULL;
        lz4Ext.obj   = nullptr;

        // Device buffer allocation
        // K1 Input:- This buffer contains input chunk data
        cl::Buffer* buffer_input = new cl::Buffer(*m_context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,
                                                inSizeVec[i],
                                                inVec[i]);
        bufInputVec.push_back(buffer_input);
        // K2 Output:- This buffer contains compressed data written by device
        cl::Buffer* buffer_lz4out = new cl::Buffer(*m_context, 
                                            CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                                                inSizeVec[i],
                                            &(lz4Ext));
        buflz4OutVec.push_back(buffer_lz4out);
        uint8_t* h_buf_out_p2p = (uint8_t*)m_q->enqueueMapBuffer(*(buffer_lz4out),
                    CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, 0, inSizeVec[i]);
        bufp2pOutVec.push_back(h_buf_out_p2p);
        // K1 Output:- This buffer contains compressed data written by device
        // K2 Input:- This is a input to data packer kernel
        cl::Buffer* buffer_output = new cl::Buffer(*m_context, 
                                            CL_MEM_READ_WRITE,
                                            inSizeVec[i]
                                            );
        bufOutputVec.push_back(buffer_output);
       
        // K2 input:- This buffer contains compressed data written by device
        cl::Buffer* buffer_lz4OutSize = new cl::Buffer(*m_context, 
                                            CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE,
                                            10 * sizeof(uint32_t),
                                            h_lz4OutSizeVec[i]);
        buflz4OutSizeVec.push_back(buffer_lz4OutSize);

        // K1 Ouput:- This buffer contains compressed block sizes
        // K2 Input:- This buffer is used in data packer kernel
        cl::Buffer* buffer_compressed_size = new cl::Buffer(*m_context, 
                                                    CL_MEM_READ_WRITE,
                                                    num_blocks * sizeof(uint32_t));
        bufCompSizeVec.push_back(buffer_compressed_size);

        // Input:- This buffer contains original input block sizes
        cl::Buffer* buffer_block_size = new cl::Buffer(*m_context, 
                                                 CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE,
                                                 num_blocks * sizeof(uint32_t),
                                                 h_blkSizeVec[i]);
        bufblockSizeVec.push_back(buffer_block_size);

        // Input:- Header buffer only used once
        cl::Buffer* buffer_header = new cl::Buffer(*m_context, 
                                                 CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE,
                                                         head_size * sizeof(uint8_t),
                                                         h_headerVec[i]);
        bufheadVec.push_back(buffer_header);

        std::chrono::duration<double, std::nano> packer_kernel_time_ns_1(0);
        std::chrono::duration<double, std::nano> comp_kernel_time_ns_1(0);
        uint64_t total_p2p_read_time = 0, total_p2p_write_time = 0;
        
        uint32_t offset = 0;
        uint32_t tail_bytes = 0;
        // Main loop of overlap execution
        // Loop below runs over total bricks i.e., host buffer size chunks
        // Figure out block sizes per brick
        uint32_t bIdx = 0; 
        for (uint32_t j = 0; j < inSizeVec[i]; j+=block_size_in_bytes) {
            uint32_t block_size = block_size_in_bytes;
            if (j+block_size > inSizeVec[i]){
                block_size = inSizeVec[i] - j;
            }
            h_blksize[bIdx++] = block_size;
        } 
     
        /* Transfer data from host to device
        * In p2p case, no need to transfer buffer input to device from host.
        */
        std::vector<cl::Memory> inBufVec;

        inBufVec.push_back(*(bufInputVec[i]));
        inBufVec.push_back(*(bufblockSizeVec[i]));

        // Migrate memory - Map host to device buffers
        m_q->enqueueMigrateMemObjects(inBufVec, 0 /* 0 means from host*/);
        m_q->finish();

        // Set kernel arguments
        int narg = 0;
        compress_kernel_lz4->setArg(narg++, *(bufInputVec[i]));
        compress_kernel_lz4->setArg(narg++, *(bufOutputVec[i]));
        compress_kernel_lz4->setArg(narg++, *(bufCompSizeVec[i]));
        compress_kernel_lz4->setArg(narg++, *(bufblockSizeVec[i]));
        compress_kernel_lz4->setArg(narg++, m_block_size_in_kb);
        compress_kernel_lz4->setArg(narg++, inSizeVec[i]);
        
        tail_bytes = 1;        
        uint32_t no_blocks_calc = (inSizeVec[i] - 1) / (m_block_size_in_kb * 1024) + 1;

        // K2 Set Kernel arguments
        narg = 0;
        packer_kernel_lz4->setArg(narg++, *(bufOutputVec[i]));
        packer_kernel_lz4->setArg(narg++, *(buflz4OutVec[i]));
        packer_kernel_lz4->setArg(narg++, *(bufheadVec[i]));
        packer_kernel_lz4->setArg(narg++, *(bufCompSizeVec[i]));
        packer_kernel_lz4->setArg(narg++, *(bufblockSizeVec[i]));
        packer_kernel_lz4->setArg(narg++, *(buflz4OutSizeVec[i]));
        packer_kernel_lz4->setArg(narg++, *(bufInputVec[i]));
        packer_kernel_lz4->setArg(narg++, head_size);
        packer_kernel_lz4->setArg(narg++, offset);
        packer_kernel_lz4->setArg(narg++, m_block_size_in_kb);
        packer_kernel_lz4->setArg(narg++, no_blocks_calc);
        packer_kernel_lz4->setArg(narg++, tail_bytes);
    }

    std::vector<cl::Event> compWait[inVec.size()];
    std::vector<cl::Event> packWait[inVec.size()];

    auto kernel_start = std::chrono::high_resolution_clock::now();   
    for (uint32_t i = 0; i < inVec.size(); i++) {
        cl::Event comp_event, pack_event;

        // Fire compress kernel
        m_q->enqueueTask(*compress_kernel_lz4, NULL, &comp_event);
        compWait[i].push_back(comp_event);

        // Fire packer kernel
        m_q->enqueueTask(*packer_kernel_lz4, &compWait[i], &pack_event);
        packWait[i].push_back(pack_event);

        // Read back data
        m_q->enqueueMigrateMemObjects({*(buflz4OutSizeVec[i])}, CL_MIGRATE_MEM_OBJECT_HOST, &packWait[i], NULL);
    }
    m_q->finish();
    auto kernel_end = std::chrono::high_resolution_clock::now();   

    for (uint32_t i = 0; i < inVec.size(); i++) {
        uint32_t compressed_size = *(h_lz4OutSizeVec[i]);
        uint32_t align_4k = compressed_size / RESIDUE_4K;
        uint32_t outIdx_align = RESIDUE_4K * align_4k;
        uint32_t residue_size = compressed_size - outIdx_align;
        uint8_t empty_buffer[4096] = {0};
        // Counter which helps in tracking
        // Output buffer index    
        uint32_t outIdx = 0;
        uint8_t *temp;

        temp = (uint8_t*)bufp2pOutVec[i];

        /* Make last packer output block divisible by 4K by appending 0's */
        temp = temp + compressed_size;
        memcpy(temp, empty_buffer, RESIDUE_4K-residue_size);
        compressed_size = outIdx_align + RESIDUE_4K;

        if (enable_p2p) {
            //uncomment after enabling for p2p
            //ret = write(fd_p2p_vec[i],  bufp2pOutVec[i], compressed_size);
            if (ret == -1)
                std::cout<<"P2P: write() failed with error: "<<ret<<", line: "<<__LINE__<<std::endl;
            //close(fd_p2p_vec[i]);
        }else {
            std::ofstream outFile(outFileVec[i].c_str(), std::ofstream::binary);
            // Testing purpose on non-p2p platform
            char* compressData = new char [inSizeVec[i]];
            m_q->enqueueReadBuffer(*(buflz4OutVec[i]), 0, 0, compressed_size, compressData);        
            m_q->finish();
            outFile.write((char*)compressData, compressed_size);            
            delete(compressData);
            outFile.close();
        }

        outIdx += compressed_size;
        auto kernel_time_ns = std::chrono::duration<double, std::nano>(kernel_end - kernel_start);
        float kernel_throughput_in_mbps_1 = (float)inSizeVec[i] * 1000 / kernel_time_ns.count();

        std::cout << "\nKernel Throughput (MBps):" << std::fixed << std::setprecision(2) << kernel_throughput_in_mbps_1;
    }
   
    for (uint32_t i = 0; i < inVec.size(); i++) { 
        delete (bufInputVec[i]);
        delete (bufOutputVec[i]);
        delete (buflz4OutVec[i]);
        delete (bufCompSizeVec[i]);
        delete (bufblockSizeVec[i]);
        delete (buflz4OutSizeVec[i]);
    }
}
