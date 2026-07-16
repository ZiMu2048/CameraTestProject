#include "sub_0032_tensors.h"

const TensorInfo sub_0032_tensors[] = {
  { "_split_1_command_stream", 0, 356, "COMMAND_STREAM", 0xffffffff },
  { "_split_1_flash", 1, 1344, "MODEL", 0xffffffff },
  { "_split_1_scratch", 2, 5376, "ARENA", 0x0 },
  { "_split_1_scratch_fast", 3, 5376, "FAST_SCRATCH", 0x0 },
  { "functional_1_box_decoding_1_concat_70363", 4, 5376, "INPUT_TENSOR", 0x0 },
  { "functional_1_box_decoding_1_mul_1_70364", 5, 5376, "OUTPUT_TENSOR", 0x0 },
};

const size_t sub_0032_tensors_count = sizeof(sub_0032_tensors) / sizeof(sub_0032_tensors[0]);

// Addresses for each input and output buffer inside of the arena
const uint32_t sub_0032_address_functional_1_box_decoding_1_concat_70363 = 0x0;
const uint32_t sub_0032_address_functional_1_box_decoding_1_mul_1_70364 = 0x0;

