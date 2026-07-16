#include "sub_0022_tensors.h"

const TensorInfo sub_0022_tensors[] = {
  { "_split_1_command_stream", 0, 884, "COMMAND_STREAM", 0xffffffff },
  { "_split_1_flash", 1, 24864, "MODEL", 0xffffffff },
  { "_split_1_scratch", 2, 40960, "ARENA", 0x0 },
  { "_split_1_scratch_fast", 3, 40960, "FAST_SCRATCH", 0x0 },
  { "functional_1_pan3_concat_1_concat_70303", 4, 24576, "INPUT_TENSOR", 0x4000 },
  { "functional_1_pan3_csp_bn_1_split_functional_1_pan3_csp_bn_1_split1_70307", 7, 8192, "OUTPUT_TENSOR", 0x4000 },
  { "functional_1_pan3_csp_bn_1_split_functional_1_pan3_csp_bn_1_split11_70310", 6, 8192, "OUTPUT_TENSOR", 0x6000 },
  { "functional_1_pan3_csp_bn_1_pan3_csp_bn_bottleneck_0_1_add_70313", 5, 8192, "OUTPUT_TENSOR", 0x0 },
};

const size_t sub_0022_tensors_count = sizeof(sub_0022_tensors) / sizeof(sub_0022_tensors[0]);

// Addresses for each input and output buffer inside of the arena
const uint32_t sub_0022_address_functional_1_pan3_concat_1_concat_70303 = 0x4000;
const uint32_t sub_0022_address_functional_1_pan3_csp_bn_1_split_functional_1_pan3_csp_bn_1_split1_70307 = 0x4000;
const uint32_t sub_0022_address_functional_1_pan3_csp_bn_1_split_functional_1_pan3_csp_bn_1_split11_70310 = 0x6000;
const uint32_t sub_0022_address_functional_1_pan3_csp_bn_1_pan3_csp_bn_bottleneck_0_1_add_70313 = 0x0;

