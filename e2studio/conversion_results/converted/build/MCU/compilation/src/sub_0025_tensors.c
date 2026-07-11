#include "sub_0025_tensors.h"

const TensorInfo sub_0025_tensors[] = {
  { "_split_1_command_stream", 4, 1276, "COMMAND_STREAM", 0xffffffff },
  { "_split_1_flash", 5, 24512, "MODEL", 0xffffffff },
  { "_split_1_scratch", 6, 69696, "ARENA", 0x0 },
  { "_split_1_scratch_fast", 7, 69696, "FAST_SCRATCH", 0x0 },
  { "_backbone_stage4_stage4_2_Slice_output_0_70356_70615_11281_70574", 3, 11616, "INPUT_TENSOR", 0x0 },
  { "_backbone_stage4_stage4_2_Slice_1_output_0_70357_70616_11277_70568", 2, 11616, "INPUT_TENSOR", 0x5ac0 },
  { "_backbone_stage4_stage4_2_Concat_output_0_70363_70646_11273", 1, 23232, "OUTPUT_TENSOR", 0x5ac0 },
  { "_backbone_stage4_stage4_2_Concat_output_0_70363_70645_11269", 0, 23232, "OUTPUT_TENSOR", 0x0 },
};

const size_t sub_0025_tensors_count = sizeof(sub_0025_tensors) / sizeof(sub_0025_tensors[0]);

// Addresses for each input and output buffer inside of the arena
const uint32_t sub_0025_address__backbone_stage4_stage4_2_Slice_output_0_70356_70615_11281_70574 = 0x0;
const uint32_t sub_0025_address__backbone_stage4_stage4_2_Slice_1_output_0_70357_70616_11277_70568 = 0x5ac0;
const uint32_t sub_0025_address__backbone_stage4_stage4_2_Concat_output_0_70363_70646_11273 = 0x5ac0;
const uint32_t sub_0025_address__backbone_stage4_stage4_2_Concat_output_0_70363_70645_11269 = 0x0;

