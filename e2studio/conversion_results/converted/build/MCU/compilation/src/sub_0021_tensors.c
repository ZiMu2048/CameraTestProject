#include "sub_0021_tensors.h"

const TensorInfo sub_0021_tensors[] = {
  { "_split_1_command_stream", 5, 2248, "COMMAND_STREAM", 0xffffffff },
  { "_split_1_flash", 6, 46688, "MODEL", 0xffffffff },
  { "_split_1_scratch", 7, 127776, "ARENA", 0x0 },
  { "_split_1_scratch_fast", 8, 127776, "FAST_SCRATCH", 0x0 },
  { "_backbone_stage3_stage3_7_Slice_output_0_70331_70611_11241_70514", 2, 23232, "INPUT_TENSOR", 0x0 },
  { "_backbone_stage3_stage3_7_Slice_1_output_0_70332_70612_11237_70508", 1, 23232, "INPUT_TENSOR", 0xb580 },
  { "_backbone_stage3_stage3_7_Concat_output_0_70338_11341", 0, 46464, "OUTPUT_TENSOR", 0x0 },
  { "_backbone_stage4_stage4_0_Concat_output_0_70347_70642_11249", 4, 23232, "OUTPUT_TENSOR", 0x11040 },
  { "_backbone_stage4_stage4_0_Concat_output_0_70347_70641_11245", 3, 23232, "OUTPUT_TENSOR", 0xb580 },
};

const size_t sub_0021_tensors_count = sizeof(sub_0021_tensors) / sizeof(sub_0021_tensors[0]);

// Addresses for each input and output buffer inside of the arena
const uint32_t sub_0021_address__backbone_stage3_stage3_7_Slice_output_0_70331_70611_11241_70514 = 0x0;
const uint32_t sub_0021_address__backbone_stage3_stage3_7_Slice_1_output_0_70332_70612_11237_70508 = 0xb580;
const uint32_t sub_0021_address__backbone_stage3_stage3_7_Concat_output_0_70338_11341 = 0x0;
const uint32_t sub_0021_address__backbone_stage4_stage4_0_Concat_output_0_70347_70642_11249 = 0x11040;
const uint32_t sub_0021_address__backbone_stage4_stage4_0_Concat_output_0_70347_70641_11245 = 0xb580;

