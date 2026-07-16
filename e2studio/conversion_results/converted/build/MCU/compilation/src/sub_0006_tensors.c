#include "sub_0006_tensors.h"

const TensorInfo sub_0006_tensors[] = {
  { "_split_1_command_stream", 0, 1232, "COMMAND_STREAM", 0xffffffff },
  { "_split_1_flash", 1, 111136, "MODEL", 0xffffffff },
  { "_split_1_scratch", 2, 57344, "ARENA", 0x0 },
  { "_split_1_scratch_fast", 3, 57344, "FAST_SCRATCH", 0x0 },
  { "functional_1_bb06_csp_bn_1_concatenate_132_1_concat_70224", 5, 32768, "INPUT_TENSOR", 0x4000 },
  { "functional_1_bb06_csp_bn_1_bb06_csp_bn_conv_out_1_activation_201_1_Relu6_functional_1_bb06_csp_bn_1_bb06_csp_bn_conv_out_1_batch_normalization_208_1_batchnorm_add_1_functional_1_bb06_csp_bn_1_bb06_csp_bn_conv_out_1_conv2d_189_1_convolution_functional_1_bb06_csp_bn_1_bb06_csp_bn_conv_out_1_batch_normalization_208_1_batchnorm_sub_70225", 4, 16384, "OUTPUT_TENSOR", 0x0 },
  { "functional_1_bb08_csp_bn_1_split_functional_1_bb08_csp_bn_1_split1_70231", 8, 4096, "OUTPUT_TENSOR", 0x4000 },
  { "functional_1_bb08_csp_bn_1_split_functional_1_bb08_csp_bn_1_split11_70234", 7, 4096, "OUTPUT_TENSOR", 0x5000 },
  { "functional_1_bb08_csp_bn_1_bb08_csp_bn_bottleneck_0_1_add_70237", 6, 4096, "OUTPUT_TENSOR", 0x6000 },
};

const size_t sub_0006_tensors_count = sizeof(sub_0006_tensors) / sizeof(sub_0006_tensors[0]);

// Addresses for each input and output buffer inside of the arena
const uint32_t sub_0006_address_functional_1_bb06_csp_bn_1_concatenate_132_1_concat_70224 = 0x4000;
const uint32_t sub_0006_address_functional_1_bb06_csp_bn_1_bb06_csp_bn_conv_out_1_activation_201_1_Relu6_functional_1_bb06_csp_bn_1_bb06_csp_bn_conv_out_1_batch_normalization_208_1_batchnorm_add_1_functional_1_bb06_csp_bn_1_bb06_csp_bn_conv_out_1_conv2d_189_1_convolution_functional_1_bb06_csp_bn_1_bb06_csp_bn_conv_out_1_batch_normalization_208_1_batchnorm_sub_70225 = 0x0;
const uint32_t sub_0006_address_functional_1_bb08_csp_bn_1_split_functional_1_bb08_csp_bn_1_split1_70231 = 0x4000;
const uint32_t sub_0006_address_functional_1_bb08_csp_bn_1_split_functional_1_bb08_csp_bn_1_split11_70234 = 0x5000;
const uint32_t sub_0006_address_functional_1_bb08_csp_bn_1_bb08_csp_bn_bottleneck_0_1_add_70237 = 0x6000;

