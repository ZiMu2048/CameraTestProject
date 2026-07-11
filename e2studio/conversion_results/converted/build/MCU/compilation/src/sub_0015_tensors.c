#include "sub_0015_tensors.h"

const TensorInfo sub_0015_tensors[] = {
  { "_split_1_command_stream", 4, 1144, "COMMAND_STREAM", 0xffffffff },
  { "_split_1_flash", 5, 8800, "MODEL", 0xffffffff },
  { "_split_1_scratch", 6, 139392, "ARENA", 0x0 },
  { "_split_1_scratch_fast", 7, 139392, "FAST_SCRATCH", 0x0 },
  { "_backbone_stage3_stage3_4_Slice_output_0_70307_70605_11201_70454", 3, 23232, "INPUT_TENSOR", 0x0 },
  { "_backbone_stage3_stage3_4_Slice_1_output_0_70308_70606_11197_70448", 2, 23232, "INPUT_TENSOR", 0xb580 },
  { "_backbone_stage3_stage3_4_Concat_output_0_70314_70636_11193", 1, 46464, "OUTPUT_TENSOR", 0xb580 },
  { "_backbone_stage3_stage3_4_Concat_output_0_70314_70635_11189", 0, 46464, "OUTPUT_TENSOR", 0x0 },
};

const size_t sub_0015_tensors_count = sizeof(sub_0015_tensors) / sizeof(sub_0015_tensors[0]);

// Addresses for each input and output buffer inside of the arena
const uint32_t sub_0015_address__backbone_stage3_stage3_4_Slice_output_0_70307_70605_11201_70454 = 0x0;
const uint32_t sub_0015_address__backbone_stage3_stage3_4_Slice_1_output_0_70308_70606_11197_70448 = 0xb580;
const uint32_t sub_0015_address__backbone_stage3_stage3_4_Concat_output_0_70314_70636_11193 = 0xb580;
const uint32_t sub_0015_address__backbone_stage3_stage3_4_Concat_output_0_70314_70635_11189 = 0x0;

