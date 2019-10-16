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
#include <fstream>
#include <vector>
#include "cmdlineparser.h"

void xil_compress_top(std::string & compress_mod, uint32_t block_size, int enable_p2p, uint8_t device_id) {
   
    // Xilinx LZ4 object 
    xil_lz4 xlz;

    // LZ4 Compression Binary Name
    std::string binaryFileName = "xil_lz4_compress_" + std::to_string(PARALLEL_BLOCK) + "b";
    xlz.m_bin_flow = 1;

    // Create xil_lz4 object
    xlz.init(binaryFileName, device_id);
        
    std::ifstream inFile(compress_mod.c_str(), std::ifstream::binary);
    if(!inFile) {
        std::cout << "Unable to open file";
        exit(1);
    }
    uint32_t input_size = get_file_size(inFile);
    inFile.close();
#ifdef VERBOSE
    std::cout << "input_size = " << input_size << " bytes." << std::endl;
#endif
    
    std::string lz_compress_in  = compress_mod;
    std::string lz_compress_out = compress_mod;
    lz_compress_out =  lz_compress_out + ".lz4";

    // Update class membery with block_size    
    xlz.m_block_size_in_kb = block_size;    
    
    // 0 means Xilinx flow
    xlz.m_switch_flow = 0;

    // Call LZ4 compression
    auto total_start = std::chrono::high_resolution_clock::now();
    uint32_t enbytes = xlz.compress_file(lz_compress_in, lz_compress_out);
    auto total_end = std::chrono::high_resolution_clock::now();
    auto total_time_ns = std::chrono::duration<double, std::nano>(total_end - total_start);

#ifdef VERBOSE
    std::cout.precision(3);
    std::cout   << "\t\t" << (float) input_size/enbytes 
                << "\t\t" << (float) input_size/1000000 
                << "\t\t\t" << lz_compress_in << std::endl;
    std::cout << "\n"; 
    std::cout << "Output Location: " << lz_compress_out.c_str() << std::endl;
    /* total time taken for read from disk + compress & packer kernels + write to disk operations */
    std::cout << "Total Time (milli sec): " << total_time_ns.count() / 1000000 << std::endl;
#endif
    
    xlz.release();

}

void xil_validate(std::string & file_list, std::string & ext){
        
        std::cout<<"\n";
        std::cout<<"Status\t\tFile Name"<<std::endl;
        std::cout<<"\n";
        
        std::ifstream infilelist_val(file_list.c_str());
        std::string line_val;
        
        while(std::getline(infilelist_val, line_val)) {
            
            std::string line_in  = line_val;
            std::string line_out = line_in + ext;

            int ret = 0;
            // Validate input and output files 
            ret = validate(line_in, line_out);
            if(ret == 0) {
                std::cout << (ret ? "FAILED\t": "PASSED\t") << "\t"<<line_in << std::endl; 
            }
            else {
                std::cout << "Validation Failed" << line_out.c_str() << std::endl;
        //        exit(1);
            }
        }
}

void xil_compress_decompress_list(std::string & file_list, std::string & ext1,
        std::string & ext2, uint32_t block_size, std::string& compress_bin) {
    
        // Compression 
        // LZ4 Compression Binary Name
        std::string binaryFileName = compress_bin;
        
        // Create xil_lz4 object
        xil_lz4 xlz;
               
        std::cout<<"\n";
        xlz.m_bin_flow = 1;
        xlz.init(binaryFileName, 0);
        std::cout<<"\n";
        
        std::cout << "--------------------------------------------------------------" << std::endl;
        std::cout << "                     Xilinx Compress                          " << std::endl;
        std::cout << "--------------------------------------------------------------" << std::endl;
   
        std::cout<<"\n";
        std::cout<<"COMP(MBps)\tPACK(MBps)\tLZ4_CR\t\tFile Size(MB)\t\tFile Name"<<std::endl;
        std::cout<<"\n";
        
        std::ifstream infilelist(file_list.c_str());
        std::string line;

        // Compress list of files 
        // This loop does LZ4 compression on list
        // of files.
        while(std::getline(infilelist, line)) {
            
            std::ifstream inFile(line.c_str(), std::ifstream::binary);
            if(!inFile) {
                std::cout << "Enc Unable to open file";
                exit(1);
            }
            
            uint32_t input_size = get_file_size(inFile);

            std::string lz_compress_in  = line;
            std::string lz_compress_out = line;
            lz_compress_out =  lz_compress_out + ext1;

            std::string change_perm = "chmod 755 " + lz_compress_out;
            system(change_perm.c_str());
         
            xlz.m_block_size_in_kb = block_size;
            xlz.m_switch_flow = 0;            
 
            // Call LZ4 compression
            uint32_t enbytes = xlz.compress_file(lz_compress_in, lz_compress_out);
                std::cout << "\t\t" << (float) input_size  / enbytes << "\t\t" 
                          << (float) input_size/1000000 << "\t\t\t" 
                          << lz_compress_in << std::endl;
        }

    // Xilinx LZ4 object 
    std::ifstream infilelist_dec(file_list.c_str());
    std::string line_dec;
    
    std::cout<<"\n";
    std::cout << "--------------------------------------------------------------" << std::endl;
    std::cout << "                     Standard De-Compress                     " << std::endl;
    std::cout << "--------------------------------------------------------------" << std::endl;
    std::cout<<"\n";
    std::cout<<"File Size(MB)\tFile Name"<<std::endl;
    std::cout<<"\n";
    
    // Decompress list of files 
    // This loop does LZ4 decompress on list
    // of files.
    while(std::getline(infilelist_dec, line_dec)) {
        
        std::string file_line = line_dec;
        file_line = file_line + ext2;

        std::ifstream inFile_dec(file_line.c_str(), std::ifstream::binary);
        if(!inFile_dec) {
            std::cout << "Dec Unable to open file";
            exit(1);
        }
        
        uint32_t input_size = get_file_size(inFile_dec);

        std::string lz_decompress_in  = file_line;
        std::string lz_decompress_out = file_line;
        lz_decompress_out =  lz_decompress_out + ".orig";
       
        // Call LZ4 decompression
        xlz.m_switch_flow = 1;
        xlz.decompress_file(lz_decompress_in, lz_decompress_out);
        
        std::cout << std::fixed << std::setprecision(3);
        std::cout   << (float) input_size/1000000 
                    << "\t\t" << lz_decompress_in << std::endl;
    } // While loop ends
}
void xil_batch_verify(std::string & file_list, uint32_t block_size, std::string& compress_bin) {

        // Flow : Xilinx LZ4 Compress vs Standard LZ4 Decompress
        // Xilinx LZ4 compression    
        std::string ext1 = ".xe2sd.lz4";    
        std::string ext2 = ".xe2sd.lz4";    
        xil_compress_decompress_list(file_list, ext1, ext2, block_size,compress_bin);        
       
        // Validate 
        std::cout<<"\n";
        std::cout << "---------------------------------------------------------------------------------------" << std::endl;
        std::cout << "                       Validate: Xilinx LZ4 Compress vs Standard LZ4 Decompress        " << std::endl;
        std::cout << "---------------------------------------------------------------------------------------" << std::endl;
        std::string ext3 = ".xe2sd";
        xil_validate(file_list, ext3);
}

int main(int argc, char *argv[])
{
    sda::utils::CmdLineParser parser;
    parser.addSwitch("--compress_xclbin",    "-cx",      "Compress XCLBIN",        "compress");
    parser.addSwitch("--compress",    "-c",      "Compress",        "");
    parser.addSwitch("--file_list",   "-l",      "List of Input Files",    "");
    parser.addSwitch("--block_size",  "-B",      "Compress Block Size [0-64: 1-256: 2-1024: 3-4096]",    "0");
    parser.addSwitch("--id",  "-id",      "Device ID",    "0");
    parser.parse(argc, argv);
    
    std::string compress_bin      = parser.value("compress_xclbin");   
    std::string compress_mod      = parser.value("compress");   
    std::string filelist          = parser.value("file_list");   
    std::string block_size        = parser.value("block_size");    
    std::string device_ids        = parser.value("id");
    uint8_t device_id = 0;

    if (!(device_ids.empty())) {
	device_id = atoi(device_ids.c_str());
    }
    uint32_t bSize = 0;
    // Block Size
    if (!(block_size.empty())) { 
        bSize = atoi(block_size.c_str());
        
        switch(bSize) {
            case 0: bSize = 64; break;
            case 1: bSize = 256; break;
            case 2: bSize = 1024; break;
            case 3: bSize = 4096; break;
            default:
                    std::cout << "Invalid Block Size provided" << std::endl;
                    parser.printHelp();
                    exit(1);
        } 
    }
    else {
        // Default Block Size - 64KB
        bSize = BLOCK_SIZE_IN_KB;
    }

    // "-c" - Compress Mode
    if (!compress_mod.empty())
    	xil_compress_top(compress_mod, bSize, 1, device_id);

    // "-l" List of Files 
    if (!filelist.empty()) {
        std::cout << "\n" << std::endl;
        std::cout << "Validation flows with Standard LZ4 ";
        std::cout << "requires executable" << std::endl;
        std::cout << "Please build LZ4 executable ";
        std::cout << "from following source ";
        std::cout << "https://github.com/lz4/lz4.git" << std::endl;
        xil_batch_verify(filelist, bSize,compress_bin);
    }
}
