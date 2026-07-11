#include "sub_0017_tensors.h"

const TensorInfo sub_0017_tensors[] = {
  { "_split_1_command_stream", 4, 1128, "COMMAND_STREAM", 0xffffffff },
  { "_split_1_flash", 5, 8656, "MODEL", 0xffffffff },
  { "_split_1_scratch", 6, 139392, "ARENA", 0x0 },
  { "_split_1_scratch_fast", 7, 139392, "FAST_SCRATCH", 0x0 },
  { "_backbone_stage3_stage3_5_Slice_output_0_70315_70607_11217_70478", 3, 23232, "INPUT_TENSOR", 0x0 },
  { "_backbone_stage3_stage3_5_Slice_1_output_0_70316_70608_11213_70472", 2, 23232, "INPUT_TENSOR", 0xb580 },
  { "_backbone_stage3_stage3_5_Concat_output_0_70322_70638_11209", 1, 46464, "OUTPUT_TENSOR", 0xb580 },
  { "_backbone_stage3_stage3_5_Concat_output_0_70322_70637_11205", 0, 46464, "OUTPUT_TENSOR", 0x0 },
};

const size_t sub_0017_tensors_count = sizeof(sub_0017_tensors) / sizeof(sub_0017_tensors[0]);

// Addresses for each input and output buffer inside of the arena
const uint32_t sub_0017_address__backbone_stage3_stage3_5_Slice_output_0_70315_70607_11217_70478 = 0x0;
const uint32_t sub_0017_address__backbone_stage3_stage3_5_Slice_1_output_0_70316_70608_11213_70472 = 0xb580;
const uint32_t sub_0017_address__backbone_stage3_stage3_5_Concat_output_0_70322_70638_11209 = 0xb580;
const uint32_t sub_0017_address__backbone_stage3_stage3_5_Concat_output_0_70322_70637_11205 = 0x0;

