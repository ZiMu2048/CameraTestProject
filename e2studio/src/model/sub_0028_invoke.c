#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "common_data.h"

#include "sub_0028_tensors.h"
#include "sub_0028_command_stream.h"
#include "sub_0028_model_data.h"

#include "sub_0028_invoke.h"

// Include Ethos-U driver headers (Assumed to be available)
#include "ethosu_driver.h"

// Define arenas with allocation and 16-byte alignment
__attribute__((aligned(32), section(".sdram_noinit_nocache"))) uint8_t sub_0028_arena[1723008];
// Fast scratch arena not used for Ethos-U55
//  We will not create it for now and reuse the address of the other arena
// __attribute__((aligned(16))) static uint8_t sub_0028_fast_scratch[1723008];
uint8_t* sub_0028_fast_scratch = sub_0028_arena;

int sub_0028_invoke(bool clean_outputs) {
  // Initialize base addresses and sizes
  uint64_t base_addrs[8] = {0};
  size_t base_addrs_size[8] = {0};
  int num_base_addrs = 8;

  // Variables for command stream
  uint8_t* cms_data = NULL;
  int cms_size = 0;

  // Prepare base_addrs and base_addrs_size arrays
  // Buffer sub_0028_model with size 146752 and address: 4294967295
  base_addrs[0] = (uint64_t)(uintptr_t)sub_0028_model_data;
  base_addrs_size[0] = sub_0028_model_data_size;
  // Buffer sub_0028_arena with size 1723008 and address: 0
  base_addrs[1] = (uint64_t)(uintptr_t) (sub_0028_arena+0);
  base_addrs_size[1] = 1723008;

  // Buffer sub_0028_fast_scratch with size 1723008 and address: 0
  base_addrs[2] = (uint64_t)(uintptr_t) (sub_0028_arena+0);
  base_addrs_size[2] = 1723008;

  // Buffer input_tensor_0 with size 32768 and address: 19072
  base_addrs[3] = (uint64_t)(uintptr_t) (sub_0028_arena+19072);
  base_addrs_size[3] = 32768;

  // Buffer input_tensor_1 with size 16384 and address: 2688
  base_addrs[4] = (uint64_t)(uintptr_t) (sub_0028_arena+2688);
  base_addrs_size[4] = 16384;

  // Buffer input_tensor_2 with size 12288 and address: 51840
  base_addrs[5] = (uint64_t)(uintptr_t) (sub_0028_arena+51840);
  base_addrs_size[5] = 12288;

  // Buffer output_tensor_0 with size 2688 and address: 0
  if (clean_outputs) {
    memset(sub_0028_arena + 0, 0, 2688);
  }
  base_addrs[6] = (uint64_t)(uintptr_t) (sub_0028_arena+0);
  base_addrs_size[6] = 2688;

  // Buffer output_tensor_1 with size 21504 and address: 88704
  if (clean_outputs) {
    memset(sub_0028_arena + 88704, 0, 21504);
  }
  base_addrs[7] = (uint64_t)(uintptr_t) (sub_0028_arena+88704);
  base_addrs_size[7] = 21504;

  // Command stream data
  cms_data = (uint8_t*)sub_0028_command_stream;
  cms_size = (int) sub_0028_command_stream_size;

  // Invoke the Ethos-U driver
  if (num_base_addrs > 8) {
    num_base_addrs = 8;
  }
  int result = ethosu_invoke_v3(&g_ethosu0, cms_data, cms_size, base_addrs, base_addrs_size, num_base_addrs, NULL);

  if (result == -1) {
    // Ethos-U invocation failed
    return -1;
  }

  return 0;
}
