This LZ4 P2P Compress application runs with Xilinx compression and standard decompression flow and currently supported with non P2P flow.

* Source codes (data_compression): In this folder all the source files are available.
```
   Host Sources : ./data_compression/L3/src/
   Host Includes : ./data_compression/L3/include/
   Kernel Code  : ./data_compression/L2/src/
   HLS Modules  : ./data_compression/L1/include/
```

* Running Emulation: Steps to build the design and run for sw_emu
```
    $ Setup Xilinx SDx 2019.1 along with XRT 
    $ cd ./data_compression/L3/benchmarks/lz4_p2p_comp/
    $ make run TARGET=sw_emu DEVICE=<path to u.2 xpfm file> -f makefile.sdx
```

* Building Design (xclbin): Steps to build the design and run for hw
```
    $ Setup Xilinx SDx 2019.1 along with XRT 
    $ cd ./data_compression/L3/benchmarks/lz4_p2p_comp/
    $ make all TARGET=hw DEVICE=<path to u.2 xpfm file> -f makefile.sdx
```

* Input Test Data
  - The input files are placed in data/ folder under L3/benchmarks/lz4_p2p_comp/ which are used for design.

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
        To enable P2P flow give 1 else 0
        $ ./build/xil_lz4_8b -cx ./build/compress.xclbin -p2p <0/1> -l <./test.list> 
```

## Results

### Resource Utilization <br />

Table below presents resource utilization of Xilinx LZ4 P2P compress
kernel with 8 engines for single compute unit. It is possible to extend number of engines to achieve higher throughput.


| Design | LUT | LUTMEM | REG | BRAM | URAM| DSP | Fmax (MHz) |
| --------------- | --- | ------ | --- | ---- | --- | -----| -----|
| Compression     | 51772 (13.77%) |14163 (9.47%) | 64209 (7.69%) | 58 (11.58%)| 48 (37.50%)| 1 (0.05%)|200|
| Packer          | 10922 (2.90%) | 1828 (1.22%)| 16708 (2.00%)| 16 (3.19%)| 0 | 2(0.10%)|200|


### Throughput & Compression Ratio

Table below presents the best end to end compress kernel execution with SSD write throughput achieved with two compute units during execution of this application.

| Topic| Results| 
|-------|--------|
|Best Compression Throughput|1.7 GB/s|

Note: Overall throughput can still be increased with multiple compute units.
