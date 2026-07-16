#include "sub_0028_tensors.h"

const TensorInfo sub_0028_tensors[] = {
  { "_split_1_command_stream", 0, 6468, "COMMAND_STREAM", 0xffffffff },
  { "_split_1_flash", 1, 146752, "MODEL", 0xffffffff },
  { "_split_1_scratch", 2, 1723008, "ARENA", 0x0 },
  { "_split_1_scratch_fast", 3, 1723008, "FAST_SCRATCH", 0x0 },
  { "functional_1_pan2_csp_bn_1_pan2_csp_bn_conv_out_1_activation_223_1_Relu6_functional_1_pan2_csp_bn_1_pan2_csp_bn_conv_out_1_batch_normalization_231_1_batchnorm_add_1_functional_1_pan2_csp_bn_1_pan2_csp_bn_conv_out_1_conv2d_208_1_convolution_functional_1_pan2_csp_bn_1_pan2_csp_bn_conv_out_1_batch_normalization_231_1_batchnorm_sub_70289", 6, 32768, "INPUT_TENSOR", 0x4a80 },
  { "functional_1_pan3_csp_bn_1_pan3_csp_bn_conv_out_1_activation_228_1_Relu6_functional_1_pan3_csp_bn_1_pan3_csp_bn_conv_out_1_batch_normalization_236_1_batchnorm_add_1_functional_1_pan3_csp_bn_1_pan3_csp_bn_conv_out_1_conv2d_213_1_convolution_functional_1_pan3_csp_bn_1_pan3_csp_bn_conv_out_1_batch_normalization_236_1_batchnorm_sub_70315", 7, 16384, "INPUT_TENSOR", 0xa80 },
  { "functional_1_pan4_csp_cib_1_concatenate_139_1_concat_70344", 8, 12288, "INPUT_TENSOR", 0xca80 },
  { "functional_1_o2m_1_o2m_class_concat_1_concat_70373", 5, 2688, "OUTPUT_TENSOR", 0x0 },
  { "functional_1_box_decoding_1_mul_70353", 4, 21504, "OUTPUT_TENSOR", 0x15a80 },
};

const size_t sub_0028_tensors_count = sizeof(sub_0028_tensors) / sizeof(sub_0028_tensors[0]);

// Addresses for each input and output buffer inside of the arena
const uint32_t sub_0028_address_functional_1_pan2_csp_bn_1_pan2_csp_bn_conv_out_1_activation_223_1_Relu6_functional_1_pan2_csp_bn_1_pan2_csp_bn_conv_out_1_batch_normalization_231_1_batchnorm_add_1_functional_1_pan2_csp_bn_1_pan2_csp_bn_conv_out_1_conv2d_208_1_convolution_functional_1_pan2_csp_bn_1_pan2_csp_bn_conv_out_1_batch_normalization_231_1_batchnorm_sub_70289 = 0x4a80;
const uint32_t sub_0028_address_functional_1_pan3_csp_bn_1_pan3_csp_bn_conv_out_1_activation_228_1_Relu6_functional_1_pan3_csp_bn_1_pan3_csp_bn_conv_out_1_batch_normalization_236_1_batchnorm_add_1_functional_1_pan3_csp_bn_1_pan3_csp_bn_conv_out_1_conv2d_213_1_convolution_functional_1_pan3_csp_bn_1_pan3_csp_bn_conv_out_1_batch_normalization_236_1_batchnorm_sub_70315 = 0xa80;
const uint32_t sub_0028_address_functional_1_pan4_csp_cib_1_concatenate_139_1_concat_70344 = 0xca80;
const uint32_t sub_0028_address_functional_1_o2m_1_o2m_class_concat_1_concat_70373 = 0x0;
const uint32_t sub_0028_address_functional_1_box_decoding_1_mul_70353 = 0x15a80;

