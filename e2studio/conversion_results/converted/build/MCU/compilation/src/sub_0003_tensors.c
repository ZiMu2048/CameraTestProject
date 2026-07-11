#include "sub_0003_tensors.h"

const TensorInfo sub_0003_tensors[] = {
  { "_split_1_command_stream", 4, 1292, "COMMAND_STREAM", 0xffffffff },
  { "_split_1_flash", 5, 3120, "MODEL", 0xffffffff },
  { "_split_1_scratch", 6, 278784, "ARENA", 0x0 },
  { "_split_1_scratch_fast", 7, 278784, "FAST_SCRATCH", 0x0 },
  { "_backbone_stage2_stage2_1_Slice_output_0_70250_70593_11105_70310", 3, 46464, "INPUT_TENSOR", 0xb580 },
  { "_backbone_stage2_stage2_1_Slice_1_output_0_70251_70594_11101_70304", 2, 46464, "INPUT_TENSOR", 0x16b00 },
  { "_backbone_stage2_stage2_1_Concat_output_0_70257_70624_11097", 1, 92928, "OUTPUT_TENSOR", 0x16b00 },
  { "_backbone_stage2_stage2_1_Concat_output_0_70257_70623_11093", 0, 92928, "OUTPUT_TENSOR", 0x0 },
};

const size_t sub_0003_tensors_count = sizeof(sub_0003_tensors) / sizeof(sub_0003_tensors[0]);

// Addresses for each input and output buffer inside of the arena
const uint32_t sub_0003_address__backbone_stage2_stage2_1_Slice_output_0_70250_70593_11105_70310 = 0xb580;
const uint32_t sub_0003_address__backbone_stage2_stage2_1_Slice_1_output_0_70251_70594_11101_70304 = 0x16b00;
const uint32_t sub_0003_address__backbone_stage2_stage2_1_Concat_output_0_70257_70624_11097 = 0x16b00;
const uint32_t sub_0003_address__backbone_stage2_stage2_1_Concat_output_0_70257_70623_11093 = 0x0;

