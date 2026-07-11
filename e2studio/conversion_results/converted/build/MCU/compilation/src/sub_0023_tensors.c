#include "sub_0023_tensors.h"

const TensorInfo sub_0023_tensors[] = {
  { "_split_1_command_stream", 4, 1284, "COMMAND_STREAM", 0xffffffff },
  { "_split_1_flash", 5, 24384, "MODEL", 0xffffffff },
  { "_split_1_scratch", 6, 69696, "ARENA", 0x0 },
  { "_split_1_scratch_fast", 7, 69696, "FAST_SCRATCH", 0x0 },
  { "_backbone_stage4_stage4_1_Slice_output_0_70348_70613_11265_70550", 3, 11616, "INPUT_TENSOR", 0x0 },
  { "_backbone_stage4_stage4_1_Slice_1_output_0_70349_70614_11261_70544", 2, 11616, "INPUT_TENSOR", 0x5ac0 },
  { "_backbone_stage4_stage4_1_Concat_output_0_70355_70644_11257", 1, 23232, "OUTPUT_TENSOR", 0x5ac0 },
  { "_backbone_stage4_stage4_1_Concat_output_0_70355_70643_11253", 0, 23232, "OUTPUT_TENSOR", 0x0 },
};

const size_t sub_0023_tensors_count = sizeof(sub_0023_tensors) / sizeof(sub_0023_tensors[0]);

// Addresses for each input and output buffer inside of the arena
const uint32_t sub_0023_address__backbone_stage4_stage4_1_Slice_output_0_70348_70613_11265_70550 = 0x0;
const uint32_t sub_0023_address__backbone_stage4_stage4_1_Slice_1_output_0_70349_70614_11261_70544 = 0x5ac0;
const uint32_t sub_0023_address__backbone_stage4_stage4_1_Concat_output_0_70355_70644_11257 = 0x5ac0;
const uint32_t sub_0023_address__backbone_stage4_stage4_1_Concat_output_0_70355_70643_11253 = 0x0;

