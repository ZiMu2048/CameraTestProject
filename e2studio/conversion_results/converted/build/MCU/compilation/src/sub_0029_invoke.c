#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "common_data.h"

#include "sub_0029_tensors.h"
#include "sub_0029_command_stream.h"
#include "sub_0029_model_data.h"

#include "sub_0029_invoke.h"

// Include Ethos-U driver headers (Assumed to be available)
#include "ethosu_driver.h"

// Define arenas with allocation and 16-byte alignment
__attribute__((aligned(16))) uint8_t sub_0029_arena[278784];
// Fast scratch arena not used for Ethos-U55
//  We will not create it for now and reuse the address of the other arena
// __attribute__((aligned(16))) static uint8_t sub_0029_fast_scratch[278784];
uint8_t* sub_0029_fast_scratch = sub_0029_arena;

int sub_0029_invoke(bool clean_outputs) {
  // Initialize base addresses and sizes
  uint64_t base_addrs[6] = {0};
  size_t base_addrs_size[6] = {0};
  int num_base_addrs = 6;

  // Variables for command stream
  uint8_t* cms_data = NULL;
  int cms_size = 0;

  // Prepare base_addrs and base_addrs_size arrays
  // Buffer sub_0029_model with size 61760 and address: 4294967295
  base_addrs[0] = (uint64_t)(uintptr_t)sub_0029_model_data;
  base_addrs_size[0] = sub_0029_model_data_size;
  // Buffer sub_0029_arena with size 278784 and address: 0
  base_addrs[1] = (uint64_t)(uintptr_t) (sub_0029_arena+0);
  base_addrs_size[1] = 278784;

  // Buffer sub_0029_fast_scratch with size 278784 and address: 0
  base_addrs[2] = (uint64_t)(uintptr_t) (sub_0029_arena+0);
  base_addrs_size[2] = 278784;

  // Buffer input_tensor_0 with size 92928 and address: 185856
  base_addrs[3] = (uint64_t)(uintptr_t) (sub_0029_arena+185856);
  base_addrs_size[3] = 92928;

  // Buffer input_tensor_1 with size 46464 and address: 0
  base_addrs[4] = (uint64_t)(uintptr_t) (sub_0029_arena+0);
  base_addrs_size[4] = 46464;

  // Buffer output_tensor_0 with size 13068 and address: 13072
  if (clean_outputs) {
    memset(sub_0029_arena + 13072, 0, 13068);
  }
  base_addrs[5] = (uint64_t)(uintptr_t) (sub_0029_arena+13072);
  base_addrs_size[5] = 13068;

  // Command stream data
  cms_data = (uint8_t*)sub_0029_command_stream;
  cms_size = (int) sub_0029_command_stream_size;

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
