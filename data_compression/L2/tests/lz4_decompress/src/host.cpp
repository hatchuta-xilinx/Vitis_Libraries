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
#include "lz4.hpp"
#include <fstream>
#include <vector>
#include "cmdlineparser.h"

void xilDecompressTop(std::string& decompress_mod, std::string& decompress_bin) {
    // Create xfLz4 object
    xfLz4 xlz(decompress_bin, 0);

    xlz.m_bin_flow = 0;

#ifdef VERBOSE
    std::cout << "\n";
    std::cout << "KT(MBps)\tFile Size(MB)\t\tFile Name" << std::endl;
    std::cout << "\n";
#endif

    std::ifstream inFile(decompress_mod.c_str(), std::ifstream::binary);
    if (!inFile) {
        std::cout << "Unable to open file";
        exit(1);
    }

    uint64_t input_size = getFileSize(inFile);
    inFile.close();

    string lz_decompress_in = decompress_mod;
    string lz_decompress_out = decompress_mod;
    lz_decompress_out = lz_decompress_out + ".orig";

    xlz.m_switch_flow = 0;

    // Call LZ4 decompression
    xlz.decompressFile(lz_decompress_in, lz_decompress_out, input_size);
#ifdef VERBOSE
    std::cout << "\t\t" << (double)input_size / 1000000 << "\t\t\t" << lz_decompress_in << std::endl;
    std::cout << "\n";
    std::cout << "Output Location: " << lz_decompress_out.c_str() << std::endl;
#endif
}

int main(int argc, char* argv[]) {
    sda::utils::CmdLineParser parser;
    parser.addSwitch("--xclbin", "-dx", "XCLBIN", "decompress");
    parser.addSwitch("--decompress", "-d", "Decompress", "");
    parser.addSwitch("--block_size", "-B", "Compress Block Size [0-64: 1-256: 2-1024: 3-4096]", "0");
    parser.parse(argc, argv);

    std::string decompress_bin = parser.value("xclbin");
    std::string decompress_mod = parser.value("decompress");
    std::string block_size = parser.value("block_size");

    uint32_t bSize = 0;
    // Block Size
    if (!(block_size.empty())) {
        bSize = atoi(block_size.c_str());

        switch (bSize) {
            case 0:
                bSize = 64;
                break;
            case 1:
                bSize = 256;
                break;
            case 2:
                bSize = 1024;
                break;
            case 3:
                bSize = 4096;
                break;
            default:
                std::cout << "Invalid Block Size provided" << std::endl;
                parser.printHelp();
                exit(1);
        }
    } else {
        // Default Block Size - 64KB
        bSize = BLOCK_SIZE_IN_KB;
    }

    // "-d" Decompress Mode
    if (!decompress_mod.empty()) xilDecompressTop(decompress_mod, decompress_bin);
}
