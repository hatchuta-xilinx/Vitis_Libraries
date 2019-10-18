This LZ4 P2P Compress application runs with Xilinx compression and standard decompression flow.

* Source codes (data_compression): In this folder all the source files are available.
```
   Host Sources : ./data_compression/L3/demos/lz4_p2p_app/src/
   Kernel Code  : ./data_compression/L2/src/
   HLS Modules  : ./data_compression/L1/include/
```

* Running Emulation: Steps to build the design and run for sw_emu
```
    $ Setup Xilinx SDx 2019.1 along with XRT 
    $ cd ./data_compression/L3/demos/lz4_p2p_app/
    $ make run TARGET=sw_emu DEVICE=<path to u.2 xpfm file>
```

* Building Design (xclbin): Steps to build the design and run for hw
```
    $ Setup Xilinx SDx 2019.1 along with XRT 
    $ cd ./data_compression/L3/demos/lz4_p2p_app/
    $ make all TARGET=hw DEVICE=<path to u.2 xpfm file> 
```

* Input Test Data
  - The input files are placed in data/ folder under L3/demos/lz4_p2p_app/ which are used for design.

* Following are the step by step instructions to run the design on board.
  - Source the XRT 
```
        $ source /opt/xilinx/xrt/setup.sh
```
  - To Mount the data from SSD
```
        $ mkfs.xfs -f /dev/nvme0n1
        $ mount /dev/nvme0n1 /mnt
        $ cp -rf <./data/> /mnt/ (copy the input files to /mnt path)
```
  - Run the design
```
        $ ./build/xil_lz4_8b -cx ./build/compress.xclbin -l <./data/test.list>
```

