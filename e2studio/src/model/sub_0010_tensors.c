#include "sub_0010_tensors.h"

const TensorInfo sub_0010_tensors[] = {
  { "_split_1_command_stream", 0, 1256, "COMMAND_STREAM", 0xffffffff },
  { "_split_1_flash", 1, 82672, "MODEL", 0xffffffff },
  { "_split_1_scratch", 2, 24576, "ARENA", 0x0 },
  { "_split_1_scratch_fast", 3, 24576, "FAST_SCRATCH", 0x0 },
  { "functional_1_bb09_spp_1_concatenate_134_1_concat_70244", 4, 16384, "INPUT_TENSOR", 0x0 },
  { "functional_1_bb10_csp_cib_1_split_functional_1_bb10_csp_cib_1_split1_70249", 7, 4096, "OUTPUT_TENSOR", 0x0 },
  { "functional_1_bb10_csp_cib_1_split_functional_1_bb10_csp_cib_1_split11_70252", 6, 4096, "OUTPUT_TENSOR", 0x1000 },
  { "functional_1_bb10_csp_cib_1_add_70258", 5, 4096, "OUTPUT_TENSOR", 0x2000 },
};

const size_t sub_0010_tensors_count = sizeof(sub_0010_tensors) / sizeof(sub_0010_tensors[0]);

// Addresses for each input and output buffer inside of the arena
const uint32_t sub_0010_address_functional_1_bb09_spp_1_concatenate_134_1_concat_70244 = 0x0;
const uint32_t sub_0010_address_functional_1_bb10_csp_cib_1_split_functional_1_bb10_csp_cib_1_split1_70249 = 0x0;
const uint32_t sub_0010_address_functional_1_bb10_csp_cib_1_split_functional_1_bb10_csp_cib_1_split11_70252 = 0x1000;
const uint32_t sub_0010_address_functional_1_bb10_csp_cib_1_add_70258 = 0x2000;

