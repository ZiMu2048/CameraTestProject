#include "sub_0030_tensors.h"

const TensorInfo sub_0030_tensors[] = {
  { "_split_1_command_stream", 0, 448, "COMMAND_STREAM", 0xffffffff },
  { "_split_1_flash", 1, 5376, "MODEL", 0xffffffff },
  { "_split_1_scratch", 2, 10752, "ARENA", 0x0 },
  { "_split_1_scratch_fast", 3, 10752, "FAST_SCRATCH", 0x0 },
  { "functional_1_box_decoding_1_Sum_70354", 4, 5376, "INPUT_TENSOR", 0x0 },
  { "functional_1_box_decoding_1_sub_70362", 6, 2688, "OUTPUT_TENSOR", 0x1500 },
  { "functional_1_box_decoding_1_add_70361", 5, 2688, "OUTPUT_TENSOR", 0x1f80 },
};

const size_t sub_0030_tensors_count = sizeof(sub_0030_tensors) / sizeof(sub_0030_tensors[0]);

// Addresses for each input and output buffer inside of the arena
const uint32_t sub_0030_address_functional_1_box_decoding_1_Sum_70354 = 0x0;
const uint32_t sub_0030_address_functional_1_box_decoding_1_sub_70362 = 0x1500;
const uint32_t sub_0030_address_functional_1_box_decoding_1_add_70361 = 0x1f80;

