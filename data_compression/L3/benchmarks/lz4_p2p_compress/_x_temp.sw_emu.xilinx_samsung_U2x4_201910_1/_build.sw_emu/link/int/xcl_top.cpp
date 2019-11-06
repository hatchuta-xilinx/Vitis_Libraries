#include "libspir_types.h"
#include "hls_stream.h"
#include "xcl_top_defines.h"
#include "ap_axi_sdata.h"
#define EXPORT_PIPE_SYMBOLS 1
#include "cpu_pipes.h"
#undef EXPORT_PIPE_SYMBOLS
#include "xcl_half.h"
#include <cstddef>
#include <vector>
#include <complex>
#include <pthread.h>
using namespace std;

extern "C" {

void xilLz4Packer(size_t in_r, size_t out_r, size_t head_prev_blk, size_t compressd_size, size_t in_block_size, size_t encoded_size, size_t orig_input_data, unsigned int head_res_size, unsigned int offset, unsigned int block_size_in_kb, unsigned int no_blocks, unsigned int tail_bytes);

static pthread_mutex_t __xlnx_cl_xilLz4Packer_mutex = PTHREAD_MUTEX_INITIALIZER;
void __stub____xlnx_cl_xilLz4Packer(char **argv) {
  void **args = (void **)argv;
  size_t in_r = *((size_t*)args[0+1]);
  size_t out_r = *((size_t*)args[1+1]);
  size_t head_prev_blk = *((size_t*)args[2+1]);
  size_t compressd_size = *((size_t*)args[3+1]);
  size_t in_block_size = *((size_t*)args[4+1]);
  size_t encoded_size = *((size_t*)args[5+1]);
  size_t orig_input_data = *((size_t*)args[6+1]);
  unsigned int head_res_size = *((unsigned int*)args[7+1]);
  unsigned int offset = *((unsigned int*)args[8+1]);
  unsigned int block_size_in_kb = *((unsigned int*)args[9+1]);
  unsigned int no_blocks = *((unsigned int*)args[10+1]);
  unsigned int tail_bytes = *((unsigned int*)args[11+1]);
 pthread_mutex_lock(&__xlnx_cl_xilLz4Packer_mutex);
  xilLz4Packer(in_r, out_r, head_prev_blk, compressd_size, in_block_size, encoded_size, orig_input_data, head_res_size, offset, block_size_in_kb, no_blocks, tail_bytes);
  pthread_mutex_unlock(&__xlnx_cl_xilLz4Packer_mutex);
}
void xilLz4Compress(size_t in_r, size_t out_r, size_t compressd_size, size_t in_block_size, unsigned int block_size_in_kb, unsigned int input_size);

static pthread_mutex_t __xlnx_cl_xilLz4Compress_mutex = PTHREAD_MUTEX_INITIALIZER;
void __stub____xlnx_cl_xilLz4Compress(char **argv) {
  void **args = (void **)argv;
  size_t in_r = *((size_t*)args[0+1]);
  size_t out_r = *((size_t*)args[1+1]);
  size_t compressd_size = *((size_t*)args[2+1]);
  size_t in_block_size = *((size_t*)args[3+1]);
  unsigned int block_size_in_kb = *((unsigned int*)args[4+1]);
  unsigned int input_size = *((unsigned int*)args[5+1]);
 pthread_mutex_lock(&__xlnx_cl_xilLz4Compress_mutex);
  xilLz4Compress(in_r, out_r, compressd_size, in_block_size, block_size_in_kb, input_size);
  pthread_mutex_unlock(&__xlnx_cl_xilLz4Compress_mutex);
}
}
