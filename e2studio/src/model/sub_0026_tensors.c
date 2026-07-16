#include "sub_0026_tensors.h"

const TensorInfo sub_0026_tensors[] = {
  { "_split_1_command_stream", 0, 1204, "COMMAND_STREAM", 0xffffffff },
  { "_split_1_flash", 1, 56192, "MODEL", 0xffffffff },
  { "_split_1_scratch", 2, 24576, "ARENA", 0x0 },
  { "_split_1_scratch_fast", 3, 24576, "FAST_SCRATCH", 0x0 },
  { "functional_1_pan4_concat_1_concat_70330", 4, 12288, "INPUT_TENSOR", 0x3000 },
  { "functional_1_pan4_csp_cib_1_split_functional_1_pan4_csp_cib_1_split1_70334", 7, 4096, "OUTPUT_TENSOR", 0x0 },
  { "functional_1_pan4_csp_cib_1_split_functional_1_pan4_csp_cib_1_split11_70337", 6, 4096, "OUTPUT_TENSOR", 0x5000 },
  { "functional_1_pan4_csp_cib_1_add_70343", 5, 4096, "OUTPUT_TENSOR", 0x1000 },
};

const size_t sub_0026_tensors_count = sizeof(sub_0026_tensors) / sizeof(sub_0026_tensors[0]);

// Addresses for each input and output buffer inside of the arena
const uint32_t sub_0026_address_functional_1_pan4_concat_1_concat_70330 = 0x3000;
const uint32_t sub_0026_address_functional_1_pan4_csp_cib_1_split_functional_1_pan4_csp_cib_1_split1_70334 = 0x0;
const uint32_t sub_0026_address_functional_1_pan4_csp_cib_1_split_functional_1_pan4_csp_cib_1_split11_70337 = 0x5000;
const uint32_t sub_0026_address_functional_1_pan4_csp_cib_1_add_70343 = 0x1000;

