#include "sub_0005_tensors.h"

const TensorInfo sub_0005_tensors[] = {
  { "_split_1_command_stream", 4, 1292, "COMMAND_STREAM", 0xffffffff },
  { "_split_1_flash", 5, 3104, "MODEL", 0xffffffff },
  { "_split_1_scratch", 6, 278784, "ARENA", 0x0 },
  { "_split_1_scratch_fast", 7, 278784, "FAST_SCRATCH", 0x0 },
  { "_backbone_stage2_stage2_2_Slice_output_0_70258_70595_11121_70334", 3, 46464, "INPUT_TENSOR", 0xb580 },
  { "_backbone_stage2_stage2_2_Slice_1_output_0_70259_70596_11117_70328", 2, 46464, "INPUT_TENSOR", 0x16b00 },
  { "_backbone_stage2_stage2_2_Concat_output_0_70265_70626_11113", 1, 92928, "OUTPUT_TENSOR", 0x16b00 },
  { "_backbone_stage2_stage2_2_Concat_output_0_70265_70625_11109", 0, 92928, "OUTPUT_TENSOR", 0x0 },
};

const size_t sub_0005_tensors_count = sizeof(sub_0005_tensors) / sizeof(sub_0005_tensors[0]);

// Addresses for each input and output buffer inside of the arena
const uint32_t sub_0005_address__backbone_stage2_stage2_2_Slice_output_0_70258_70595_11121_70334 = 0xb580;
const uint32_t sub_0005_address__backbone_stage2_stage2_2_Slice_1_output_0_70259_70596_11117_70328 = 0x16b00;
const uint32_t sub_0005_address__backbone_stage2_stage2_2_Concat_output_0_70265_70626_11113 = 0x16b00;
const uint32_t sub_0005_address__backbone_stage2_stage2_2_Concat_output_0_70265_70625_11109 = 0x0;

