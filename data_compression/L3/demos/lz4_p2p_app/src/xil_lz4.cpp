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
																int enable_p2p
                               ) 
{
    if (m_switch_flow == 0) { // Xilinx FPGA compression flow
        std::ifstream inFile;
        std::ofstream outFile;
        uint32_t input_size;
        uint32_t out_size;

	if (enable_p2p) {
		fd_p2p_c_in = open(inFile_name.c_str(), O_RDONLY | O_DIRECT);
		if(fd_p2p_c_in <= 0) {
			std::cout << "P2P: Unable to open input file, fd: " << fd_p2p_c_in << std::endl;
			exit(1);
		}
		input_size = lseek(fd_p2p_c_in, 0, SEEK_END);
		lseek(fd_p2p_c_in, 0, SEEK_SET);

		fd_p2p_c_out = open(outFile_name.c_str(),  O_CREAT | O_WRONLY | O_TRUNC | O_APPEND | O_DIRECT, S_IRWXG | S_IRWXU);
		if(fd_p2p_c_out <= 0) {
			std::cout << "P2P: Unable to open output file, exited!, ret: "<< fd_p2p_c_out << std::endl;
			close(fd_p2p_c_in);
			exit(1);
		}
	}
	else {
        	inFile.open(inFile_name.c_str(), std::ifstream::binary);
        	if(!inFile) {
            		std::cout << "Non P2P: Unable to open input file\n";
            		exit(1);
        	}
        	input_size = get_file_size(inFile);

        	outFile.open(outFile_name.c_str(), std::ofstream::binary);
        	if(!outFile) {
            		std::cout << "Non P2P: Unable to open output file\n";
			inFile.close();
            		exit(1);
        	}
	}

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

        uint32_t host_buffer_size = HOST_BUFFER_SIZE;
        uint32_t acc_buff_size = m_block_size_in_kb * 1024 * PARALLEL_BLOCK;
        if (acc_buff_size > host_buffer_size){
            host_buffer_size = acc_buff_size;
        }
        if (host_buffer_size > input_size){
            host_buffer_size = input_size;
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

        uint32_t enbytes;
        // LZ4 overlap & multiple compute unit compress 
	if (enable_p2p) {
		/* Pass NULL values for input and output to the compress() function,
		 * since these buffers are not needed in P2P.
		 */
        	enbytes = compress(NULL, NULL, input_size, host_buffer_size, enable_p2p);
        	close(fd_p2p_c_in);
        	close(fd_p2p_c_out);
	}
	else {
            if(input_size < 4096)
               out_size = 4096;
            else
               out_size = input_size; 
        	std::vector<uint8_t,aligned_allocator<uint8_t>> in (input_size);
        	inFile.read((char *)in.data(),input_size);
        	std::vector<uint8_t,aligned_allocator<uint8_t>> out(out_size);
        	enbytes = compress(in.data(), out.data(), input_size, host_buffer_size, enable_p2p);
        	outFile.write((char *)out.data(), enbytes);
        	inFile.close();
        	outFile.close();
	}
        return enbytes;
    } else { // Standard LZ4 flow
        std::string command = "../../../common/thirdParty/std_lz4/lz4 --content-size -f -q " + inFile_name;
        system(command.c_str());
        std::string output = inFile_name + ".lz4";
        std::string rout = inFile_name + ".std.lz4";
        std::string rename = "mv " + output + " " + rout;
        system(rename.c_str());
        return 0;
    }
}

int validate(std::string & inFile_name, std::string & outFile_name) {

    std::string command = "cmp " + inFile_name + " " + outFile_name;
    int ret = system(command.c_str());
    return ret;
}

void xil_lz4::buffer_extension_assignments(bool flow){
    for (int i = 0; i < MAX_COMPUTE_UNITS; i++) {
        for (int j = 0; j < OVERLAP_BUF_COUNT; j++){
            if(flow){
                inExt[i][j].flags = comp_ddr_nums[i];
                inExt[i][j].obj   = h_buf_in[i][j].data();
                inExt[i][j].param   = NULL;
                
                outExt[i][j].flags = comp_ddr_nums[i];
                outExt[i][j].obj   = h_buf_out[i][j].data();
                outExt[i][j].param   = NULL;
             
   
                csExt[i][j].flags = comp_ddr_nums[i];
                csExt[i][j].obj   = h_compressSize[i][j].data();
                csExt[i][j].param   = NULL;
                
                bsExt[i][j].flags = comp_ddr_nums[i];
                bsExt[i][j].obj   = h_blksize[i][j].data();
                bsExt[i][j].param   = NULL;

                lz4SizeExt[i][j].flags = comp_ddr_nums[0];
                lz4SizeExt[i][j].obj = h_lz4OutSize[i][j].data();
                lz4SizeExt[i][j].param   = NULL;

                lz4Ext[i][j].flags = comp_ddr_nums[0];
                lz4Ext[i][j].obj   = h_enc_out[i][j].data();
                lz4Ext[i][j].param   = NULL;
            
            }
        }
    }

	if (flow) {
		headExt.flags = comp_ddr_nums[0];
		headExt.obj   = h_header.data();
		headExt.param   = NULL;
	}
}

// Constructor
xil_lz4::xil_lz4(){

    for (int i = 0; i < MAX_COMPUTE_UNITS; i++) {
        for (int j = 0; j < OVERLAP_BUF_COUNT; j++){
            // Index calculation
            h_buf_in[i][j].resize(HOST_BUFFER_SIZE);
            h_buf_out[i][j].resize(HOST_BUFFER_SIZE);

            h_blksize[i][j].resize(MAX_NUMBER_BLOCKS);
            h_compressSize[i][j].resize(MAX_NUMBER_BLOCKS);    
 
            m_compressSize[i][j].reserve(MAX_NUMBER_BLOCKS);
            m_blkSize[i][j].reserve(MAX_NUMBER_BLOCKS);
            h_enc_out[i][j].resize(HOST_BUFFER_SIZE);           
            h_lz4OutSize[i][j].resize(RESIDUE_4K);
        }
    }
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
    for (int i = 0; i < C_COMPUTE_UNIT; i++){ 
        compress_kernel_lz4[i] = new cl::Kernel(*m_program, compress_kernel_names[i].c_str());
        packer_kernel_lz4[i] = new cl::Kernel(*m_program, packer_kernel_names[i].c_str());
    }
   
    return 0;
}

int xil_lz4::release()
{
   
    if (m_bin_flow) {
        for(int i = 0; i < C_COMPUTE_UNIT; i++)
            delete(compress_kernel_lz4[i]);
    }
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
                           uint32_t input_size,
                           uint32_t host_buffer_size,
                           int enable_p2p
                         ) {

    uint32_t block_size_in_bytes = m_block_size_in_kb * 1024;
    uint32_t overlap_buf_count = OVERLAP_BUF_COUNT;
    uint64_t total_kernel_time = 0;
    uint64_t total_packer_kernel_time = 0;
    //uint64_t total_write_time = 0;
    //uint64_t total_read_time = 0;

    /* Input buffer pointer used in p2p case */
    uint8_t *h_buf_in_p2p[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];
    /* Packer output buffer pointer used in p2p case */
    uint8_t *h_buf_out_p2p[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];
    int ret = 0;

    // Read, Write and Kernel events
    cl::Event kernel_events[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];
    cl::Event read_events[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];
    cl::Event write_events[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];
    cl::Event packer_kernel_events[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];

    cl::Event cs_kernel_events;
    cl::Event pk_kernel_events;

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
    
    uint32_t total_chunks = (input_size - 1) / host_buffer_size + 1;

    if(total_chunks < 2) overlap_buf_count = 1;

    // Find out the size of each chunk spanning entire file
    // For eaxmple: As mentioned in previous example there are 6 chunks
    // Code below finds out the size of chunk, in general all the chunks holds
    // HOST_BUFFER_SIZE except for the last chunk
    uint32_t sizeOfChunk[total_chunks];
    uint32_t idx = 0;
    for (uint32_t i = 0; i < input_size; i += host_buffer_size, idx++) {
        uint32_t chunk_size = host_buffer_size;
        if (chunk_size + i > input_size) {
            chunk_size = input_size - i;
        }
        // Update size of each chunk buffer
        sizeOfChunk[idx] = chunk_size;
    }

    uint32_t temp_nblocks = (host_buffer_size - 1)/block_size_in_bytes + 1;
    host_buffer_size = ((host_buffer_size-1)/64 + 1) * 64;

    // Device buffer allocation
    for (int cu = 0; cu < C_COMPUTE_UNIT;cu++) {
        for (uint32_t flag = 0; flag < overlap_buf_count; flag++) {
	  if (enable_p2p) {
	    /* set cl_mem_ext_ptr flag to XCL_MEM_EXT_P2P_BUFFER for p2p to work */
	    inExt[cu][flag].flags |= XCL_MEM_EXT_P2P_BUFFER;
	    /* obj set to nullptr as we call enqueueMapBuffer explicitly for p2p to work */
	    inExt[cu][flag].obj   = nullptr;

	    lz4Ext[cu][flag].flags |= XCL_MEM_EXT_P2P_BUFFER;
	    lz4Ext[cu][flag].obj   = nullptr;

	    // K1 Input:- This buffer contains input chunk data
	    buffer_input[cu][flag] = new cl::Buffer(*m_context, 
                                                CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX,
                                                host_buffer_size,
                                                &(inExt[cu][flag]));
	    h_buf_in_p2p[cu][flag] = (uint8_t*)m_q->enqueueMapBuffer(*(buffer_input[cu][flag]),
	    				CL_TRUE, CL_MAP_WRITE, 0, host_buffer_size);

	    // K2 Output:- This buffer contains compressed data written by device
	    buffer_lz4out[cu][flag] = new cl::Buffer(*m_context, 
                                            CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                                            host_buffer_size,
					    &(lz4Ext[cu][flag]));
	    h_buf_out_p2p[cu][flag] = (uint8_t*)m_q->enqueueMapBuffer(*(buffer_lz4out[cu][flag]),
	    				CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, 0, host_buffer_size);
	 } else {
            // K1 Input:- This buffer contains input chunk data
            buffer_input[cu][flag] = new cl::Buffer(*m_context, 
                                                CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                                                host_buffer_size,
                                                &(inExt[cu][flag]));
            // K2 Output:- This buffer contains compressed data written by device
            buffer_lz4out[cu][flag] = new cl::Buffer(*m_context, 
                                            CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                                            host_buffer_size,
					    &(lz4Ext[cu][flag]));
         }					    
            // K1 Output:- This buffer contains compressed data written by device
            // K2 Input:- This is a input to data packer kernel
            buffer_output[cu][flag] = new cl::Buffer(*m_context, 
                                                CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                                                host_buffer_size,
                                                &(outExt[cu][flag]));
           
            // K2 input:- This buffer contains compressed data written by device
            buffer_lz4OutSize[cu][flag] = new cl::Buffer(*m_context, 
                                                CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                                                10 * sizeof(uint32_t),
                                                &lz4SizeExt[cu][flag]);

            // K1 Ouput:- This buffer contains compressed block sizes
            // K2 Input:- This buffer is used in data packer kernel
            buffer_compressed_size[cu][flag] = new cl::Buffer(*m_context, 
                                                        CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                                                        temp_nblocks * sizeof(uint32_t),
                                                        &(csExt[cu][flag]));

            // Input:- This buffer contains original input block sizes
            buffer_block_size[cu][flag] = new cl::Buffer(*m_context, 
                                                     CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                                                     temp_nblocks * sizeof(uint32_t),
                                                     &(bsExt[cu][flag]));
            
            // Input:- Header buffer only used once
            buffer_header[cu][flag] = new cl::Buffer(*m_context, 
                                                     CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                                                     head_size * sizeof(uint8_t),
                                                     &headExt);
        }
    }

    // Counter which helps in tracking
    // Output buffer index    
    uint32_t outIdx = 0;

    std::chrono::duration<double, std::nano> packer_kernel_time_ns_1(0);
    std::chrono::duration<double, std::nano> comp_kernel_time_ns_1(0);
    uint64_t total_p2p_read_time = 0, total_p2p_write_time = 0;
    
    int flag = 0; 
    uint32_t offset = 0;
    uint32_t tail_bytes = 0;
    // Kernel wait events for writing & compute
    std::vector<cl::Event> kernelWriteWait;
    std::vector<cl::Event> kernelComputeWait;
    std::vector<cl::Event> kernelReadWait;
    int cu = 0;
    // Main loop of overlap execution
    // Loop below runs over total bricks i.e., host buffer size chunks
    auto total_start = std::chrono::high_resolution_clock::now();
    for (uint32_t brick = 0, itr = 0; brick < total_chunks; brick+=C_COMPUTE_UNIT, itr++, flag = !flag) {
            // Figure out block sizes per brick
            uint32_t bIdx = 0; 
            for (uint32_t i = 0; i < sizeOfChunk[brick + cu]; i+=block_size_in_bytes) {
                uint32_t block_size = block_size_in_bytes;
                if (i+block_size > sizeOfChunk[brick + cu]){
                    block_size = sizeOfChunk[brick + cu] - i;
                }
                (h_blksize[cu][flag]).data()[bIdx++] = block_size;
            } 
            
            // Copy data from input buffer to host
	    if (enable_p2p) {
		auto start = std::chrono::high_resolution_clock::now();
	    	/* Third arg in read() should be divisible by 4K */
            	ret = read(fd_p2p_c_in, h_buf_in_p2p[cu][flag], HOST_BUFFER_SIZE);
		if (ret == -1)
			std::cout<<"P2P: read() failed with error: "<<ret<<", line: "<<__LINE__<<std::endl;
		auto end = std::chrono::high_resolution_clock::now();
		auto p2p_time = std::chrono::duration<double, std::nano>(end - start);
		total_p2p_read_time += p2p_time.count();
	    } else {
            	std::memcpy(h_buf_in[cu][flag].data(), &in[(brick + cu) * host_buffer_size], sizeOfChunk[brick + cu]);
	    }

            // Set kernel arguments
            int narg = 0;
            compress_kernel_lz4[cu]->setArg(narg++, *(buffer_input[cu][flag]));
            compress_kernel_lz4[cu]->setArg(narg++, *(buffer_output[cu][flag]));
            compress_kernel_lz4[cu]->setArg(narg++, *(buffer_compressed_size[cu][flag]));
            compress_kernel_lz4[cu]->setArg(narg++, *(buffer_block_size[cu][flag]));
            compress_kernel_lz4[cu]->setArg(narg++, m_block_size_in_kb);
            compress_kernel_lz4[cu]->setArg(narg++, sizeOfChunk[brick + cu]);
            
            /* Transfer data from host to device
	     * In p2p case, no need to transfer buffer input to device from host.
	     */
	    if (enable_p2p)
            	m_q->enqueueMigrateMemObjects({*(buffer_block_size[cu][flag])}, 0, NULL, &(write_events[cu][flag]));
	    else
           	m_q->enqueueMigrateMemObjects({*(buffer_input[cu][flag]), *(buffer_block_size[cu][flag])}, 0, NULL, &(write_events[cu][flag]));

            // Kernel Write events update
            kernelWriteWait.push_back(write_events[cu][flag]);
            
            // Fire the kernel
            m_q->enqueueTask(*compress_kernel_lz4[cu], &kernelWriteWait, &(kernel_events[cu][flag]));
            
            // Update kernel events flag on computation
            kernelComputeWait.push_back(kernel_events[cu][flag]);
            
            tail_bytes = (brick == total_chunks - 1) ? 1 : 0;        

            // K2 Set Kernel arguments
            narg = 0;
            packer_kernel_lz4[cu]->setArg(narg++, *(buffer_output[cu][flag]));
            packer_kernel_lz4[cu]->setArg(narg++, *(buffer_lz4out[cu][flag]));
            if (brick == 0) {
                packer_kernel_lz4[cu]->setArg(narg++, *(buffer_header[cu][flag]));
            }else {
                // Wait on previous packer operation to finish
                read_events[cu][!flag].wait();
            	total_kernel_time += get_event_duration_ns(kernel_events[cu][!flag]);
            	total_packer_kernel_time += get_event_duration_ns(packer_kernel_events[cu][!flag]);
#ifdef EVENT_PROFILE
		if (!enable_p2p) {
            		total_read_time += get_event_duration_ns(read_events[cu][!flag]);
            		total_write_time += get_event_duration_ns(write_events[cu][!flag]);
		}
#endif

		// Copy output from kernel/packer and dump as .lz4 stream
            	uint32_t compressed_size = h_lz4OutSize[cu][!flag].data()[0];
            	uint32_t packed_buffer = compressed_size / RESIDUE_4K;
            	uint32_t outIdx_align = RESIDUE_4K * packed_buffer;
            	head_size = compressed_size - outIdx_align;
            	offset = outIdx_align;
   
	    	if (enable_p2p) {
			auto start = std::chrono::high_resolution_clock::now();
			ret = write(fd_p2p_c_out, h_buf_out_p2p[cu][!flag], outIdx_align);
			if (ret == -1)
				std::cout<<"P2P: write() failed with error: "<<ret<<", line: "<<__LINE__<<std::endl;
			auto end = std::chrono::high_resolution_clock::now();
			auto p2p_time = std::chrono::duration<double, std::nano>(end - start);
			total_p2p_write_time += p2p_time.count();
	   	} else {
                	std::memcpy(&out[outIdx],  &h_enc_out[cu][!flag].data()[0], outIdx_align);
	    	}	
                outIdx += outIdx_align;
		packer_kernel_lz4[cu]->setArg(narg++, *(buffer_lz4out[cu][!flag]));
            }
            uint32_t no_blocks_calc = (sizeOfChunk[brick+cu] - 1) / (m_block_size_in_kb * 1024) + 1;
            packer_kernel_lz4[cu]->setArg(narg++, *(buffer_compressed_size[cu][flag]));
            packer_kernel_lz4[cu]->setArg(narg++, *(buffer_block_size[cu][flag]));
            packer_kernel_lz4[cu]->setArg(narg++, *(buffer_lz4OutSize[cu][flag]));
            packer_kernel_lz4[cu]->setArg(narg++, *(buffer_input[cu][flag]));
            packer_kernel_lz4[cu]->setArg(narg++, head_size);
            packer_kernel_lz4[cu]->setArg(narg++, offset);
            packer_kernel_lz4[cu]->setArg(narg++, m_block_size_in_kb);
            packer_kernel_lz4[cu]->setArg(narg++, no_blocks_calc);
            packer_kernel_lz4[cu]->setArg(narg++, tail_bytes);

            // Fire the kernel
            m_q->enqueueTask(*packer_kernel_lz4[cu], &kernelComputeWait, &(packer_kernel_events[cu][flag]));
            // Update kernel events flag on computation
            kernelComputeWait.push_back(packer_kernel_events[cu][flag]);

            /* Transfer data from device to host.
	     * In p2p case, no need to transfer packer output data from device to host 
	     */
	    if (enable_p2p)
            	m_q->enqueueMigrateMemObjects({*(buffer_lz4OutSize[cu][flag])}, CL_MIGRATE_MEM_OBJECT_HOST,&kernelComputeWait, &(read_events[cu][flag]));
	    else
            	m_q->enqueueMigrateMemObjects({*(buffer_lz4out[cu][flag]), *(buffer_lz4OutSize[cu][flag])}, CL_MIGRATE_MEM_OBJECT_HOST, &kernelComputeWait, &(read_events[cu][flag]));
    } // Main loop ends here

    read_events[cu][!flag].wait();
    total_kernel_time += get_event_duration_ns(kernel_events[cu][!flag]);
    total_packer_kernel_time += get_event_duration_ns(packer_kernel_events[cu][!flag]);
#ifdef EVENT_PROFILE
    if (!enable_p2p) {
    	total_read_time += get_event_duration_ns(read_events[cu][!flag]);
    	total_write_time += get_event_duration_ns(write_events[cu][!flag]);
    }
#endif
    m_q->finish();
    m_q->flush();
    
    uint32_t compressed_size = h_lz4OutSize[0][!flag].data()[0];
    uint32_t align_4k = compressed_size / RESIDUE_4K;
    uint32_t outIdx_align = RESIDUE_4K * align_4k;
    uint32_t residue_size = compressed_size - outIdx_align;
    uint8_t empty_buffer[4096] = {0};
    uint8_t *temp;

    if (enable_p2p)
    	temp = (uint8_t*)h_buf_out_p2p[0][!flag];
    else		
    	temp = (uint8_t*)h_enc_out[0][!flag].data();

    /* Make last packer output block divisible by 4K by appending 0's */
    temp = temp + compressed_size;
    memcpy(temp, empty_buffer, RESIDUE_4K-residue_size);
    compressed_size = outIdx_align + RESIDUE_4K;

    if (enable_p2p) {
    	ret = write(fd_p2p_c_out,  h_buf_out_p2p[0][!flag], compressed_size);
    	if (ret == -1)
		std::cout<<"P2P: write() failed with error: "<<ret<<", line: "<<__LINE__<<std::endl;
    } else {
    	std::memcpy(&out[outIdx],  &h_enc_out[0][!flag].data()[0], compressed_size);
    }
    outIdx += compressed_size;
    auto total_end = std::chrono::high_resolution_clock::now();   
    auto total_time_ns = std::chrono::duration<double, std::nano>(total_end - total_start);
    float throughput_in_mbps_1 = (float)input_size * 1000 / total_time_ns.count();
    float kernel_throughput_in_mbps_1 = (float)input_size * 1000 / (total_kernel_time + total_packer_kernel_time);
    //float compress_kernel_mbps = (float)input_size * 1000 / total_kernel_time;
    //float packer_kernel_mbps = (float)outIdx * 1000 / total_packer_kernel_time;

#ifdef EVENT_PROFILE
    std::cout << "Total compress Kernel Time (milli sec) = " << total_kernel_time / 1000000 << std::endl;
    std::cout << "Total Packer Kernel Time (milli sec) = " << total_packer_kernel_time / 1000000 << std::endl;
    if (enable_p2p) {
    	std::cout << "Total Write Time (milli sec) = " << total_p2p_write_time / 1000000 << std::endl;
    	std::cout << "Total Read Time (milli sec) = " << total_p2p_read_time / 1000000 << std::endl;
    } else {
    	std::cout << "Total Write Time (milli sec) = " << total_write_time / 1000000 << std::endl;
    	std::cout << "Total Read Time (milli sec) = " << total_read_time / 1000000 << std::endl;
    }
    std::cout << "Compression kernel throughput (MBps) = " << compress_kernel_mbps << std::endl;
    std::cout << "Packer kernel throughput (MBps) = " << packer_kernel_mbps  << std::endl;
#endif    
#ifdef VERBOSE
    //std::cout << "Compression is done, compressed_size = " << outIdx << std::endl;
    //std::cout << "\nE2E(MBps)\tKT(MBps)\tLZ4_CR\t\tFile Size(MB)\t\tFile Name" << std::endl;
    std::cout << std::fixed << std::setprecision(2) << throughput_in_mbps_1 << "\t\t";
    std::cout << std::fixed << std::setprecision(2) << kernel_throughput_in_mbps_1;
#endif    
            
    for (int cu = 0; cu < C_COMPUTE_UNIT;cu++) {
        for (uint32_t flag = 0; flag < overlap_buf_count;flag++) {
            delete (buffer_input[cu][flag]);
            delete (buffer_output[cu][flag]);
            delete (buffer_lz4out[cu][flag]);
            delete (buffer_compressed_size[cu][flag]);
            delete (buffer_block_size[cu][flag]);
            delete (buffer_lz4OutSize[cu][flag]);
        }
    }
    return outIdx; 
} // Overlap end
