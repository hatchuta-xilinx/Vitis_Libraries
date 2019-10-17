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
int fd_p2p_c_out = 0;
int fd_p2p_c_in = 0;

// XXHASH for K2
uint32_t xxhash_val;

uint64_t xil_lz4::get_event_duration_ns(const cl::Event &event){

    uint64_t start_time=0, end_time=0;

    event.getProfilingInfo<uint64_t>(CL_PROFILING_COMMAND_START, &start_time);
    event.getProfilingInfo<uint64_t>(CL_PROFILING_COMMAND_END, &end_time);
    return (end_time - start_time);
}

uint32_t xil_lz4::compress_file(std::string & inFile_name, 
                                std::string & outFile_name,
                                uint32_t input_size
                               ) 
{
        std::ofstream outFile;
        uint32_t out_size;

        std::ifstream inFile(inFile_name.c_str(), std::ifstream::binary);
         if (!inFile) {
            std::cout << "Unable to open file";
            exit(1);
        }
        std::vector<uint8_t, aligned_allocator<uint8_t> > in(input_size);
        inFile.read((char*)in.data(), input_size);

		//fd_p2p_c_out = open(outFile_name.c_str(),  O_CREAT | O_WRONLY | O_TRUNC | O_APPEND | O_DIRECT, S_IRWXG | S_IRWXU);
		fd_p2p_c_out = open(outFile_name.c_str(),  O_CREAT | O_WRONLY | O_DIRECT, 0777);
		if(fd_p2p_c_out <= 0) {
			std::cout << "P2P: Unable to open output file, exited!, ret: "<< fd_p2p_c_out << std::endl;
			close(fd_p2p_c_in);
			exit(1);
		}
        
        uint32_t enbytes;
		/* Pass NULL value for output to the compress() function,
		 * since this buffer not needed in P2P.
		 */
        enbytes = compress(in.data(), NULL, input_size);
        inFile.close();
        close(fd_p2p_c_out);
        return enbytes;
}

int validate(std::string & inFile_name, std::string & outFile_name) {

    std::string command = "cmp " + inFile_name + " " + outFile_name;
    int ret = system(command.c_str());
    return ret;
}

void xil_lz4::buffer_extension_assignments(bool flow){
            if(flow){
                inExt.flags = comp_ddr_nums[0];
                inExt.obj   = h_buf_in.data();
                inExt.param   = NULL;
                
                outExt.flags = comp_ddr_nums[0];
                outExt.obj   = h_buf_out.data();
                outExt.param   = NULL;
             
   
                csExt.flags = comp_ddr_nums[0];
                csExt.obj   = h_compressSize.data();
                csExt.param   = NULL;
                
                bsExt.flags = comp_ddr_nums[0];
                bsExt.obj   = h_blksize.data();
                bsExt.param   = NULL;

                lz4SizeExt.flags = comp_ddr_nums[0];
                lz4SizeExt.obj = h_lz4OutSize.data();
                lz4SizeExt.param   = NULL;

                lz4Ext.flags = comp_ddr_nums[0];
                lz4Ext.obj   = h_enc_out.data();
                lz4Ext.param   = NULL;
            
            }
	if (flow) {
		headExt.flags = comp_ddr_nums[0];
		headExt.obj   = h_header.data();
		headExt.param   = NULL;
	}
}

// Constructor
xil_lz4::xil_lz4(){
            // Index calculation
            h_buf_in.resize(HOST_BUFFER_SIZE);
            h_buf_out.resize(HOST_BUFFER_SIZE);

            h_blksize.resize(MAX_NUMBER_BLOCKS);
            h_compressSize.resize(MAX_NUMBER_BLOCKS);    
 
            m_compressSize.reserve(MAX_NUMBER_BLOCKS);
            m_blkSize.reserve(MAX_NUMBER_BLOCKS);
            h_enc_out.resize(HOST_BUFFER_SIZE);           
            h_lz4OutSize.resize(RESIDUE_4K);
            h_header.resize(RESIDUE_4K);
}   

// Destructor
xil_lz4::~xil_lz4(){
}

int xil_lz4::init(const std::string& binaryFileName, uint8_t device_id)
{
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
   
    return 0;
}

int xil_lz4::release()
{
   
    if (m_bin_flow) 
        delete(compress_kernel_lz4);
    delete(m_program);
    delete(m_q);
    delete(m_context);

    return 0;
}

uint32_t xil_lz4::decompress_file(std::string & inFile_name, 
                                  std::string & outFile_name
                                 ) {
        std::string command = "../../../common/thirdParty/std_lz4/lz4 --content-size -f -q -d " + inFile_name;
        system(command.c_str());
        return 0;
}

// This version of compression does overlapped execution between
// Kernel and Host. I/O operations between Host and Device are
// overlapped with Kernel execution between multiple compute units
uint32_t xil_lz4::compress(uint8_t *in,
                           uint8_t *out,
                           uint32_t input_size
                         ) {

    /* Packer output buffer pointer used in p2p case */
    uint8_t *h_buf_out_p2p;
    int ret = 0;

    uint64_t total_kernel_time = 0;
    uint64_t total_packer_kernel_time = 0;

    auto total_start = std::chrono::high_resolution_clock::now();
    for (uint32_t i = 0; i < 1; i++) {
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
                    std::cout << "Invalid Block Size given, so setting to 64K"<<std::endl;
                    break;
        }

        uint8_t temp_buff[10] = {FLG_BYTE,
                                 block_size_header,
                                 input_size,
                                 input_size >> 8,
                                 input_size >> 16,
                                 input_size >> 24,
                                 0,0,0,0
                                };

        // xxhash is used to calculate hash value
        uint32_t xxh = XXH32(temp_buff, 10, 0);
        // This value is sent to Kernel 2
        xxhash_val = (xxh>>8);


        uint32_t block_size_in_bytes = m_block_size_in_kb * 1024;

        // Header information
        uint32_t head_size = 0;
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
        h_header[head_size++] = input_size;
        h_header[head_size++] = input_size >> 8;
        h_header[head_size++] = input_size >> 16;
        h_header[head_size++] = input_size >> 24;
        h_header[head_size++] = 0;
        h_header[head_size++] = 0;
        h_header[head_size++] = 0;
        h_header[head_size++] = 0;

        // XXHASH value 
        h_header[head_size++] = xxhash_val;

        //Assignment to the buffer extensions
        buffer_extension_assignments(1);

        // Total chunks in input file
        // For example: Input file size is 12MB and Host buffer size is 2MB
        // Then we have 12/2 = 6 chunks exists 
        // Calculate the count of total chunks based on input size
        // This count is used to overlap the execution between chunks and file
        // operations
        
        uint32_t num_blocks = (input_size - 1)/block_size_in_bytes + 1;

        std::memcpy(h_buf_in.data(), &in[0], input_size);

        // Device buffer allocation
        lz4Ext.flags |= XCL_MEM_EXT_P2P_BUFFER;
        lz4Ext.obj   = nullptr;

        // K1 Input:- This buffer contains input chunk data
        buffer_input = new cl::Buffer(*m_context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                                                input_size,
                                                &(inExt));
        // K2 Output:- This buffer contains compressed data written by device
        buffer_lz4out = new cl::Buffer(*m_context, 
                                            CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                                                input_size,
                                            &(lz4Ext));
        h_buf_out_p2p = (uint8_t*)m_q->enqueueMapBuffer(*(buffer_lz4out),
                    CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, 0, input_size);
        // K1 Output:- This buffer contains compressed data written by device
        // K2 Input:- This is a input to data packer kernel
        buffer_output = new cl::Buffer(*m_context, 
                                            CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                                            input_size,
                                            &(outExt));
       
        // K2 input:- This buffer contains compressed data written by device
        buffer_lz4OutSize = new cl::Buffer(*m_context, 
                                            CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                                            10 * sizeof(uint32_t),
                                            &lz4SizeExt);

        // K1 Ouput:- This buffer contains compressed block sizes
        // K2 Input:- This buffer is used in data packer kernel
        buffer_compressed_size = new cl::Buffer(*m_context, 
                                                    CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                                                    num_blocks * sizeof(uint32_t),
                                                    &(csExt));

        // Input:- This buffer contains original input block sizes
        buffer_block_size = new cl::Buffer(*m_context, 
                                                 CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                                                 num_blocks * sizeof(uint32_t),
                                                 &(bsExt));
        
        // Input:- Header buffer only used once
        buffer_header = new cl::Buffer(*m_context, 
                                                 CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                                                         head_size * sizeof(uint8_t),
                                                         &headExt);


        std::chrono::duration<double, std::nano> packer_kernel_time_ns_1(0);
        std::chrono::duration<double, std::nano> comp_kernel_time_ns_1(0);
        uint64_t total_p2p_read_time = 0, total_p2p_write_time = 0;
        
        uint32_t offset = 0;
        uint32_t tail_bytes = 0;
        // Main loop of overlap execution
        // Loop below runs over total bricks i.e., host buffer size chunks
        // Figure out block sizes per brick
        uint32_t bIdx = 0; 
        for (uint32_t i = 0; i < input_size; i+=block_size_in_bytes) {
            uint32_t block_size = block_size_in_bytes;
            if (i+block_size > input_size){
                block_size = input_size - i;
            }
            h_blksize.data()[bIdx++] = block_size;
        } 
     
        /* Transfer data from host to device
        * In p2p case, no need to transfer buffer input to device from host.
        */
        std::vector<cl::Memory> inBufVec;

        inBufVec.push_back(*(buffer_input));
        inBufVec.push_back(*(buffer_block_size));

        // Migrate memory - Map host to device buffers
        m_q->enqueueMigrateMemObjects(inBufVec, 0 /* 0 means from host*/);
        m_q->finish();

        // Set kernel arguments
        int narg = 0;
        compress_kernel_lz4->setArg(narg++, *(buffer_input));
        compress_kernel_lz4->setArg(narg++, *(buffer_output));
        compress_kernel_lz4->setArg(narg++, *(buffer_compressed_size));
        compress_kernel_lz4->setArg(narg++, *(buffer_block_size));
        compress_kernel_lz4->setArg(narg++, m_block_size_in_kb);
        compress_kernel_lz4->setArg(narg++, input_size);
        
        tail_bytes = 1;        
        uint32_t no_blocks_calc = (input_size - 1) / (m_block_size_in_kb * 1024) + 1;

        // K2 Set Kernel arguments
        narg = 0;
        packer_kernel_lz4->setArg(narg++, *(buffer_output));
        packer_kernel_lz4->setArg(narg++, *(buffer_lz4out));
        packer_kernel_lz4->setArg(narg++, *(buffer_header));
        packer_kernel_lz4->setArg(narg++, *(buffer_compressed_size));
        packer_kernel_lz4->setArg(narg++, *(buffer_block_size));
        packer_kernel_lz4->setArg(narg++, *(buffer_lz4OutSize));
        packer_kernel_lz4->setArg(narg++, *(buffer_input));
        packer_kernel_lz4->setArg(narg++, head_size);
        packer_kernel_lz4->setArg(narg++, offset);
        packer_kernel_lz4->setArg(narg++, m_block_size_in_kb);
        packer_kernel_lz4->setArg(narg++, no_blocks_calc);
        packer_kernel_lz4->setArg(narg++, tail_bytes);
    }

    for (uint32_t i = 0; i < 1; i++) {
        cl::Event comp_event, pack_event;
        std::vector<cl::Event> compWait;
        std::vector<cl::Event> packWait;

        // Fire compress kernel
        m_q->enqueueTask(*compress_kernel_lz4, NULL, &comp_event);
        compWait.push_back(comp_event);

        // Fire packer kernel
        m_q->enqueueTask(*packer_kernel_lz4, &compWait, &pack_event);
        packWait.push_back(pack_event);

        // Read back data
        m_q->enqueueMigrateMemObjects({*(buffer_lz4OutSize)}, CL_MIGRATE_MEM_OBJECT_HOST, &packWait, NULL);
    }
    m_q->finish();

    for (uint32_t i = 0; i < 1; i++) {
        uint32_t compressed_size = h_lz4OutSize.data()[0];
        uint32_t align_4k = compressed_size / RESIDUE_4K;
        uint32_t outIdx_align = RESIDUE_4K * align_4k;
        uint32_t residue_size = compressed_size - outIdx_align;
        uint8_t empty_buffer[4096] = {0};
        // Counter which helps in tracking
        // Output buffer index    
        uint32_t outIdx = 0;
        uint8_t *temp;

        temp = (uint8_t*)h_buf_out_p2p;

        /* Make last packer output block divisible by 4K by appending 0's */
        temp = temp + compressed_size;
        memcpy(temp, empty_buffer, RESIDUE_4K-residue_size);
        compressed_size = outIdx_align + RESIDUE_4K;

        ret = write(fd_p2p_c_out,  h_buf_out_p2p, compressed_size);
        if (ret == -1)
        std::cout<<"P2P: write() failed with error: "<<ret<<", line: "<<__LINE__<<std::endl;
        outIdx += compressed_size;
        auto total_end = std::chrono::high_resolution_clock::now();   
        auto total_time_ns = std::chrono::duration<double, std::nano>(total_end - total_start);
        float throughput_in_mbps_1 = (float)input_size * 1000 / total_time_ns.count();
        float kernel_throughput_in_mbps_1 = (float)input_size * 1000 / (total_kernel_time + total_packer_kernel_time);

#ifdef VERBOSE
        std::cout << std::fixed << std::setprecision(2) << throughput_in_mbps_1 << "\t\t";
        std::cout << std::fixed << std::setprecision(2) << kernel_throughput_in_mbps_1;
#endif    
                
        delete (buffer_input);
        delete (buffer_output);
        delete (buffer_lz4out);
        delete (buffer_compressed_size);
        delete (buffer_block_size);
        delete (buffer_lz4OutSize);
        return outIdx; 
    }
}
