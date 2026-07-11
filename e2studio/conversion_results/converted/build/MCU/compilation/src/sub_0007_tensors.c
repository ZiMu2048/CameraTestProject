#include "sub_0007_tensors.h"

const TensorInfo sub_0007_tensors[] = {
  { "_split_1_command_stream", 4, 2064, "COMMAND_STREAM", 0xffffffff },
  { "_split_1_flash", 5, 16656, "MODEL", 0xffffffff },
  { "_split_1_scratch", 6, 232320, "ARENA", 0x0 },
  { "_split_1_scratch_fast", 7, 232320, "FAST_SCRATCH", 0x0 },
  { "_backbone_stage2_stage2_3_Slice_output_0_70266_70597_11129_70346", 1, 46464, "INPUT_TENSOR", 0x0 },
  { "_backbone_stage2_stage2_3_Slice_1_output_0_70267_70598_11125_70340", 0, 46464, "INPUT_TENSOR", 0x16b00 },
  { "_backbone_stage3_stage3_0_Concat_output_0_70282_70628_11137", 3, 46464, "OUTPUT_TENSOR", 0x0 },
  { "_backbone_stage3_stage3_0_Concat_output_0_70282_70627_11133", 2, 46464, "OUTPUT_TENSOR", 0x16b00 },
};

const size_t sub_0007_tensors_count = sizeof(sub_0007_tensors) / sizeof(sub_0007_tensors[0]);

// Addresses for each input and output buffer inside of the arena
const uint32_t sub_0007_address__backbone_stage2_stage2_3_Slice_output_0_70266_70597_11129_70346 = 0x0;
const uint32_t sub_0007_address__backbone_stage2_stage2_3_Slice_1_output_0_70267_70598_11125_70340 = 0x16b00;
const uint32_t sub_0007_address__backbone_stage3_stage3_0_Concat_output_0_70282_70628_11137 = 0x0;
const uint32_t sub_0007_address__backbone_stage3_stage3_0_Concat_output_0_70282_70627_11133 = 0x16b00;

