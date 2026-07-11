/*
 * This file is developed by EdgeCortix Inc. to be used with certain Renesas Electronics Hardware only.
 *
 * Copyright © 2025 EdgeCortix Inc. Licensed to Renesas Electronics Corporation with the
 * right to sublicense under the Apache License, Version 2.0.
 *
 * This file also includes source code originally developed by the Renesas Electronics Corporation.
 * The Renesas disclaimer below applies to any Renesas-originated portions for usage of the code.
 *
 * The Renesas Electronics Corporation
 * DISCLAIMER
 * This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products. No
 * other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all
 * applicable laws, including copyright laws.
 * THIS SOFTWARE IS PROVIDED 'AS IS' AND RENESAS MAKES NO WARRANTIES REGARDING
 * THIS SOFTWARE, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED. TO THE MAXIMUM
 * EXTENT PERMITTED NOT PROHIBITED BY LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES
 * SHALL BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO THIS
 * SOFTWARE, EVEN IF RENESAS OR ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 * Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability of
 * this software. By using this software, you agree to the additional terms and conditions found by accessing the
 * following link:
 * http://www.renesas.com/disclaimer
 *
 * Changed from original python code to C source code.
 * Copyright (C) 2017 Renesas Electronics Corporation. All rights reserved.
 *
 * This file also includes source codes originally developed by the TensorFlow Authors which were distributed under the following conditions.
 *
 * The TensorFlow Authors
 * Copyright 2023 The Apache Software Foundation
 *
 * This product includes software developed at
 * The Apache Software Foundation (http://www.apache.org/).
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "model.h"

// CPU compute declarations
#include "compute_sub_0000.h"
#include "sub_0001_invoke.h"
#include "compute_sub_0002.h"
#include "sub_0003_invoke.h"
#include "compute_sub_0004.h"
#include "sub_0005_invoke.h"
#include "compute_sub_0006.h"
#include "sub_0007_invoke.h"
#include "compute_sub_0008.h"
#include "sub_0009_invoke.h"
#include "compute_sub_0010.h"
#include "sub_0011_invoke.h"
#include "compute_sub_0012.h"
#include "sub_0013_invoke.h"
#include "compute_sub_0014.h"
#include "sub_0015_invoke.h"
#include "compute_sub_0016.h"
#include "sub_0017_invoke.h"
#include "compute_sub_0018.h"
#include "sub_0019_invoke.h"
#include "compute_sub_0020.h"
#include "sub_0021_invoke.h"
#include "compute_sub_0022.h"
#include "sub_0023_invoke.h"
#include "compute_sub_0024.h"
#include "sub_0025_invoke.h"
#include "compute_sub_0026.h"
#include "sub_0027_invoke.h"
#include "compute_sub_0028.h"
#include "sub_0029_invoke.h"
#include "compute_sub_0030.h"

// Buffers for CPU units
float buf_input_1[196608];
int8_t buf_input_1_70592_11293[196608];
int8_t buf__backbone_stage2_stage2_1_Slice_1_output_0_70251_70594_11101_70304[24576];
int8_t buf__backbone_stage2_stage2_1_Slice_output_0_70250_70593_11105_70310[24576];
int8_t buf__backbone_stage2_stage2_2_Slice_1_output_0_70259_70596_11117_70328[24576];
int8_t buf__backbone_stage2_stage2_2_Slice_output_0_70258_70595_11121_70334[24576];
int8_t buf__backbone_stage2_stage2_3_Slice_1_output_0_70267_70598_11125_70340[24576];
int8_t buf__backbone_stage2_stage2_3_Slice_output_0_70266_70597_11129_70346[24576];
int8_t buf__backbone_stage3_stage3_1_Slice_1_output_0_70284_70600_11149_70376[12288];
int8_t buf__backbone_stage3_stage3_1_Slice_output_0_70283_70599_11153_70382[12288];
int8_t buf__backbone_stage3_stage3_2_Slice_1_output_0_70292_70602_11165_70400[12288];
int8_t buf__backbone_stage3_stage3_2_Slice_output_0_70291_70601_11169_70406[12288];
int8_t buf__backbone_stage3_stage3_3_Slice_1_output_0_70300_70604_11181_70424[12288];
int8_t buf__backbone_stage3_stage3_3_Slice_output_0_70299_70603_11185_70430[12288];
int8_t buf__backbone_stage3_stage3_4_Slice_1_output_0_70308_70606_11197_70448[12288];
int8_t buf__backbone_stage3_stage3_4_Slice_output_0_70307_70605_11201_70454[12288];
int8_t buf__backbone_stage3_stage3_5_Slice_1_output_0_70316_70608_11213_70472[12288];
int8_t buf__backbone_stage3_stage3_5_Slice_output_0_70315_70607_11217_70478[12288];
int8_t buf__backbone_stage3_stage3_6_Slice_1_output_0_70324_70610_11229_70496[12288];
int8_t buf__backbone_stage3_stage3_6_Slice_output_0_70323_70609_11233_70502[12288];
int8_t buf__backbone_stage3_stage3_7_Slice_1_output_0_70332_70612_11237_70508[12288];
int8_t buf__backbone_stage3_stage3_7_Slice_output_0_70331_70611_11241_70514[12288];
int8_t buf__backbone_stage4_stage4_1_Slice_1_output_0_70349_70614_11261_70544[6144];
int8_t buf__backbone_stage4_stage4_1_Slice_output_0_70348_70613_11265_70550[6144];
int8_t buf__backbone_stage4_stage4_2_Slice_1_output_0_70357_70616_11277_70568[6144];
int8_t buf__backbone_stage4_stage4_2_Slice_output_0_70356_70615_11281_70574[6144];
int8_t buf__backbone_stage4_stage4_3_Slice_1_output_0_70365_70618_11285_70580[6144];
int8_t buf__backbone_stage4_stage4_3_Slice_output_0_70364_70617_11289_70586[6144];
int8_t buf__fpn_Resize_output_0_70381_10947[49152];
float buf__722_70391_70619[6912];
float buf__723_70392_70620[1728];

// Arenas for CPU units
uint8_t compute_arena_sub_0000[kBufferSize_sub_0000];
uint8_t compute_arena_sub_0002[kBufferSize_sub_0002];
uint8_t compute_arena_sub_0004[kBufferSize_sub_0004];
uint8_t compute_arena_sub_0006[kBufferSize_sub_0006];
uint8_t compute_arena_sub_0008[kBufferSize_sub_0008];
uint8_t compute_arena_sub_0010[kBufferSize_sub_0010];
uint8_t compute_arena_sub_0012[kBufferSize_sub_0012];
uint8_t compute_arena_sub_0014[kBufferSize_sub_0014];
uint8_t compute_arena_sub_0016[kBufferSize_sub_0016];
uint8_t compute_arena_sub_0018[kBufferSize_sub_0018];
uint8_t compute_arena_sub_0020[kBufferSize_sub_0020];
uint8_t compute_arena_sub_0022[kBufferSize_sub_0022];
uint8_t compute_arena_sub_0024[kBufferSize_sub_0024];
uint8_t compute_arena_sub_0026[kBufferSize_sub_0026];
uint8_t compute_arena_sub_0028[kBufferSize_sub_0028];
uint8_t compute_arena_sub_0030[kBufferSize_sub_0030];

  // Model input pointers
float* GetModelInputPtr_input_1() {
  return buf_input_1;
}


  // Model output pointers
float* GetModelOutputPtr__722_70391_70619() {
  return buf__722_70391_70619;
}

float* GetModelOutputPtr__723_70392_70620() {
  return buf__723_70392_70620;
}


void RunModel(bool clean_outputs) {
  // Buffers for NPU units
  int8_t* buf__backbone_stage2_stage2_0_Concat_output_0_70249_70621_11085 = (int8_t*) (sub_0001_arena + sub_0001_address__backbone_stage2_stage2_0_Concat_output_0_70249_70621_11085);
  int8_t* buf__backbone_stage2_stage2_0_Concat_output_0_70249_70622_11089 = (int8_t*) (sub_0001_arena + sub_0001_address__backbone_stage2_stage2_0_Concat_output_0_70249_70622_11089);
  int8_t* buf__backbone_stage2_stage2_1_Concat_output_0_70257_70623_11093 = (int8_t*) (sub_0003_arena + sub_0003_address__backbone_stage2_stage2_1_Concat_output_0_70257_70623_11093);
  int8_t* buf__backbone_stage2_stage2_1_Concat_output_0_70257_70624_11097 = (int8_t*) (sub_0003_arena + sub_0003_address__backbone_stage2_stage2_1_Concat_output_0_70257_70624_11097);
  int8_t* buf__backbone_stage2_stage2_2_Concat_output_0_70265_70625_11109 = (int8_t*) (sub_0005_arena + sub_0005_address__backbone_stage2_stage2_2_Concat_output_0_70265_70625_11109);
  int8_t* buf__backbone_stage2_stage2_2_Concat_output_0_70265_70626_11113 = (int8_t*) (sub_0005_arena + sub_0005_address__backbone_stage2_stage2_2_Concat_output_0_70265_70626_11113);
  int8_t* buf__backbone_stage3_stage3_0_Concat_output_0_70282_70627_11133 = (int8_t*) (sub_0007_arena + sub_0007_address__backbone_stage3_stage3_0_Concat_output_0_70282_70627_11133);
  int8_t* buf__backbone_stage3_stage3_0_Concat_output_0_70282_70628_11137 = (int8_t*) (sub_0007_arena + sub_0007_address__backbone_stage3_stage3_0_Concat_output_0_70282_70628_11137);
  int8_t* buf__backbone_stage3_stage3_1_Concat_output_0_70290_70629_11141 = (int8_t*) (sub_0009_arena + sub_0009_address__backbone_stage3_stage3_1_Concat_output_0_70290_70629_11141);
  int8_t* buf__backbone_stage3_stage3_1_Concat_output_0_70290_70630_11145 = (int8_t*) (sub_0009_arena + sub_0009_address__backbone_stage3_stage3_1_Concat_output_0_70290_70630_11145);
  int8_t* buf__backbone_stage3_stage3_2_Concat_output_0_70298_70631_11157 = (int8_t*) (sub_0011_arena + sub_0011_address__backbone_stage3_stage3_2_Concat_output_0_70298_70631_11157);
  int8_t* buf__backbone_stage3_stage3_2_Concat_output_0_70298_70632_11161 = (int8_t*) (sub_0011_arena + sub_0011_address__backbone_stage3_stage3_2_Concat_output_0_70298_70632_11161);
  int8_t* buf__backbone_stage3_stage3_3_Concat_output_0_70306_70633_11173 = (int8_t*) (sub_0013_arena + sub_0013_address__backbone_stage3_stage3_3_Concat_output_0_70306_70633_11173);
  int8_t* buf__backbone_stage3_stage3_3_Concat_output_0_70306_70634_11177 = (int8_t*) (sub_0013_arena + sub_0013_address__backbone_stage3_stage3_3_Concat_output_0_70306_70634_11177);
  int8_t* buf__backbone_stage3_stage3_4_Concat_output_0_70314_70635_11189 = (int8_t*) (sub_0015_arena + sub_0015_address__backbone_stage3_stage3_4_Concat_output_0_70314_70635_11189);
  int8_t* buf__backbone_stage3_stage3_4_Concat_output_0_70314_70636_11193 = (int8_t*) (sub_0015_arena + sub_0015_address__backbone_stage3_stage3_4_Concat_output_0_70314_70636_11193);
  int8_t* buf__backbone_stage3_stage3_5_Concat_output_0_70322_70637_11205 = (int8_t*) (sub_0017_arena + sub_0017_address__backbone_stage3_stage3_5_Concat_output_0_70322_70637_11205);
  int8_t* buf__backbone_stage3_stage3_5_Concat_output_0_70322_70638_11209 = (int8_t*) (sub_0017_arena + sub_0017_address__backbone_stage3_stage3_5_Concat_output_0_70322_70638_11209);
  int8_t* buf__backbone_stage3_stage3_6_Concat_output_0_70330_70639_11221 = (int8_t*) (sub_0019_arena + sub_0019_address__backbone_stage3_stage3_6_Concat_output_0_70330_70639_11221);
  int8_t* buf__backbone_stage3_stage3_6_Concat_output_0_70330_70640_11225 = (int8_t*) (sub_0019_arena + sub_0019_address__backbone_stage3_stage3_6_Concat_output_0_70330_70640_11225);
  int8_t* buf__backbone_stage3_stage3_7_Concat_output_0_70338_11341 = (int8_t*) (sub_0021_arena + sub_0021_address__backbone_stage3_stage3_7_Concat_output_0_70338_11341);
  int8_t* buf__backbone_stage4_stage4_0_Concat_output_0_70347_70641_11245 = (int8_t*) (sub_0021_arena + sub_0021_address__backbone_stage4_stage4_0_Concat_output_0_70347_70641_11245);
  int8_t* buf__backbone_stage4_stage4_0_Concat_output_0_70347_70642_11249 = (int8_t*) (sub_0021_arena + sub_0021_address__backbone_stage4_stage4_0_Concat_output_0_70347_70642_11249);
  int8_t* buf__backbone_stage4_stage4_1_Concat_output_0_70355_70643_11253 = (int8_t*) (sub_0023_arena + sub_0023_address__backbone_stage4_stage4_1_Concat_output_0_70355_70643_11253);
  int8_t* buf__backbone_stage4_stage4_1_Concat_output_0_70355_70644_11257 = (int8_t*) (sub_0023_arena + sub_0023_address__backbone_stage4_stage4_1_Concat_output_0_70355_70644_11257);
  int8_t* buf__backbone_stage4_stage4_2_Concat_output_0_70363_70645_11269 = (int8_t*) (sub_0025_arena + sub_0025_address__backbone_stage4_stage4_2_Concat_output_0_70363_70645_11269);
  int8_t* buf__backbone_stage4_stage4_2_Concat_output_0_70363_70646_11273 = (int8_t*) (sub_0025_arena + sub_0025_address__backbone_stage4_stage4_2_Concat_output_0_70363_70646_11273);
  int8_t* buf__723_70392_70620_11081 = (int8_t*) (sub_0027_arena + sub_0027_address__723_70392_70620_11081);
  int8_t* buf__backbone_stage4_stage4_3_Concat_output_0_70371_11357 = (int8_t*) (sub_0027_arena + sub_0027_address__backbone_stage4_stage4_3_Concat_output_0_70371_11357);
  int8_t* buf__722_70391_70619_11077 = (int8_t*) (sub_0029_arena + sub_0029_address__722_70391_70619_11077);

  // CPU Unit
  compute_sub_0000(compute_arena_sub_0000, buf_input_1, buf_input_1_70592_11293  );

  memcpy((sub_0001_arena + sub_0001_address_input_1_70592_11293), buf_input_1_70592_11293, 196608);
  // NPU Unit
  sub_0001_invoke(clean_outputs);

  // CPU Unit
  compute_sub_0002(compute_arena_sub_0002, buf__backbone_stage2_stage2_0_Concat_output_0_70249_70621_11085, buf__backbone_stage2_stage2_0_Concat_output_0_70249_70622_11089, buf__backbone_stage2_stage2_1_Slice_1_output_0_70251_70594_11101_70304, buf__backbone_stage2_stage2_1_Slice_output_0_70250_70593_11105_70310  );

  memcpy((sub_0003_arena + sub_0003_address__backbone_stage2_stage2_1_Slice_1_output_0_70251_70594_11101_70304), buf__backbone_stage2_stage2_1_Slice_1_output_0_70251_70594_11101_70304, 24576);
  memcpy((sub_0003_arena + sub_0003_address__backbone_stage2_stage2_1_Slice_output_0_70250_70593_11105_70310), buf__backbone_stage2_stage2_1_Slice_output_0_70250_70593_11105_70310, 24576);
  // NPU Unit
  sub_0003_invoke(clean_outputs);

  // CPU Unit
  compute_sub_0004(compute_arena_sub_0004, buf__backbone_stage2_stage2_1_Concat_output_0_70257_70623_11093, buf__backbone_stage2_stage2_1_Concat_output_0_70257_70624_11097, buf__backbone_stage2_stage2_2_Slice_1_output_0_70259_70596_11117_70328, buf__backbone_stage2_stage2_2_Slice_output_0_70258_70595_11121_70334  );

  memcpy((sub_0005_arena + sub_0005_address__backbone_stage2_stage2_2_Slice_1_output_0_70259_70596_11117_70328), buf__backbone_stage2_stage2_2_Slice_1_output_0_70259_70596_11117_70328, 24576);
  memcpy((sub_0005_arena + sub_0005_address__backbone_stage2_stage2_2_Slice_output_0_70258_70595_11121_70334), buf__backbone_stage2_stage2_2_Slice_output_0_70258_70595_11121_70334, 24576);
  // NPU Unit
  sub_0005_invoke(clean_outputs);

  // CPU Unit
  compute_sub_0006(compute_arena_sub_0006, buf__backbone_stage2_stage2_2_Concat_output_0_70265_70625_11109, buf__backbone_stage2_stage2_2_Concat_output_0_70265_70626_11113, buf__backbone_stage2_stage2_3_Slice_1_output_0_70267_70598_11125_70340, buf__backbone_stage2_stage2_3_Slice_output_0_70266_70597_11129_70346  );

  memcpy((sub_0007_arena + sub_0007_address__backbone_stage2_stage2_3_Slice_1_output_0_70267_70598_11125_70340), buf__backbone_stage2_stage2_3_Slice_1_output_0_70267_70598_11125_70340, 24576);
  memcpy((sub_0007_arena + sub_0007_address__backbone_stage2_stage2_3_Slice_output_0_70266_70597_11129_70346), buf__backbone_stage2_stage2_3_Slice_output_0_70266_70597_11129_70346, 24576);
  // NPU Unit
  sub_0007_invoke(clean_outputs);

  // CPU Unit
  compute_sub_0008(compute_arena_sub_0008, buf__backbone_stage3_stage3_0_Concat_output_0_70282_70627_11133, buf__backbone_stage3_stage3_0_Concat_output_0_70282_70628_11137, buf__backbone_stage3_stage3_1_Slice_1_output_0_70284_70600_11149_70376, buf__backbone_stage3_stage3_1_Slice_output_0_70283_70599_11153_70382  );

  memcpy((sub_0009_arena + sub_0009_address__backbone_stage3_stage3_1_Slice_1_output_0_70284_70600_11149_70376), buf__backbone_stage3_stage3_1_Slice_1_output_0_70284_70600_11149_70376, 12288);
  memcpy((sub_0009_arena + sub_0009_address__backbone_stage3_stage3_1_Slice_output_0_70283_70599_11153_70382), buf__backbone_stage3_stage3_1_Slice_output_0_70283_70599_11153_70382, 12288);
  // NPU Unit
  sub_0009_invoke(clean_outputs);

  // CPU Unit
  compute_sub_0010(compute_arena_sub_0010, buf__backbone_stage3_stage3_1_Concat_output_0_70290_70629_11141, buf__backbone_stage3_stage3_1_Concat_output_0_70290_70630_11145, buf__backbone_stage3_stage3_2_Slice_1_output_0_70292_70602_11165_70400, buf__backbone_stage3_stage3_2_Slice_output_0_70291_70601_11169_70406  );

  memcpy((sub_0011_arena + sub_0011_address__backbone_stage3_stage3_2_Slice_1_output_0_70292_70602_11165_70400), buf__backbone_stage3_stage3_2_Slice_1_output_0_70292_70602_11165_70400, 12288);
  memcpy((sub_0011_arena + sub_0011_address__backbone_stage3_stage3_2_Slice_output_0_70291_70601_11169_70406), buf__backbone_stage3_stage3_2_Slice_output_0_70291_70601_11169_70406, 12288);
  // NPU Unit
  sub_0011_invoke(clean_outputs);

  // CPU Unit
  compute_sub_0012(compute_arena_sub_0012, buf__backbone_stage3_stage3_2_Concat_output_0_70298_70631_11157, buf__backbone_stage3_stage3_2_Concat_output_0_70298_70632_11161, buf__backbone_stage3_stage3_3_Slice_1_output_0_70300_70604_11181_70424, buf__backbone_stage3_stage3_3_Slice_output_0_70299_70603_11185_70430  );

  memcpy((sub_0013_arena + sub_0013_address__backbone_stage3_stage3_3_Slice_1_output_0_70300_70604_11181_70424), buf__backbone_stage3_stage3_3_Slice_1_output_0_70300_70604_11181_70424, 12288);
  memcpy((sub_0013_arena + sub_0013_address__backbone_stage3_stage3_3_Slice_output_0_70299_70603_11185_70430), buf__backbone_stage3_stage3_3_Slice_output_0_70299_70603_11185_70430, 12288);
  // NPU Unit
  sub_0013_invoke(clean_outputs);

  // CPU Unit
  compute_sub_0014(compute_arena_sub_0014, buf__backbone_stage3_stage3_3_Concat_output_0_70306_70633_11173, buf__backbone_stage3_stage3_3_Concat_output_0_70306_70634_11177, buf__backbone_stage3_stage3_4_Slice_1_output_0_70308_70606_11197_70448, buf__backbone_stage3_stage3_4_Slice_output_0_70307_70605_11201_70454  );

  memcpy((sub_0015_arena + sub_0015_address__backbone_stage3_stage3_4_Slice_1_output_0_70308_70606_11197_70448), buf__backbone_stage3_stage3_4_Slice_1_output_0_70308_70606_11197_70448, 12288);
  memcpy((sub_0015_arena + sub_0015_address__backbone_stage3_stage3_4_Slice_output_0_70307_70605_11201_70454), buf__backbone_stage3_stage3_4_Slice_output_0_70307_70605_11201_70454, 12288);
  // NPU Unit
  sub_0015_invoke(clean_outputs);

  // CPU Unit
  compute_sub_0016(compute_arena_sub_0016, buf__backbone_stage3_stage3_4_Concat_output_0_70314_70635_11189, buf__backbone_stage3_stage3_4_Concat_output_0_70314_70636_11193, buf__backbone_stage3_stage3_5_Slice_1_output_0_70316_70608_11213_70472, buf__backbone_stage3_stage3_5_Slice_output_0_70315_70607_11217_70478  );

  memcpy((sub_0017_arena + sub_0017_address__backbone_stage3_stage3_5_Slice_1_output_0_70316_70608_11213_70472), buf__backbone_stage3_stage3_5_Slice_1_output_0_70316_70608_11213_70472, 12288);
  memcpy((sub_0017_arena + sub_0017_address__backbone_stage3_stage3_5_Slice_output_0_70315_70607_11217_70478), buf__backbone_stage3_stage3_5_Slice_output_0_70315_70607_11217_70478, 12288);
  // NPU Unit
  sub_0017_invoke(clean_outputs);

  // CPU Unit
  compute_sub_0018(compute_arena_sub_0018, buf__backbone_stage3_stage3_5_Concat_output_0_70322_70637_11205, buf__backbone_stage3_stage3_5_Concat_output_0_70322_70638_11209, buf__backbone_stage3_stage3_6_Slice_1_output_0_70324_70610_11229_70496, buf__backbone_stage3_stage3_6_Slice_output_0_70323_70609_11233_70502  );

  memcpy((sub_0019_arena + sub_0019_address__backbone_stage3_stage3_6_Slice_1_output_0_70324_70610_11229_70496), buf__backbone_stage3_stage3_6_Slice_1_output_0_70324_70610_11229_70496, 12288);
  memcpy((sub_0019_arena + sub_0019_address__backbone_stage3_stage3_6_Slice_output_0_70323_70609_11233_70502), buf__backbone_stage3_stage3_6_Slice_output_0_70323_70609_11233_70502, 12288);
  // NPU Unit
  sub_0019_invoke(clean_outputs);

  // CPU Unit
  compute_sub_0020(compute_arena_sub_0020, buf__backbone_stage3_stage3_6_Concat_output_0_70330_70639_11221, buf__backbone_stage3_stage3_6_Concat_output_0_70330_70640_11225, buf__backbone_stage3_stage3_7_Slice_1_output_0_70332_70612_11237_70508, buf__backbone_stage3_stage3_7_Slice_output_0_70331_70611_11241_70514  );

  memcpy((sub_0021_arena + sub_0021_address__backbone_stage3_stage3_7_Slice_1_output_0_70332_70612_11237_70508), buf__backbone_stage3_stage3_7_Slice_1_output_0_70332_70612_11237_70508, 12288);
  memcpy((sub_0021_arena + sub_0021_address__backbone_stage3_stage3_7_Slice_output_0_70331_70611_11241_70514), buf__backbone_stage3_stage3_7_Slice_output_0_70331_70611_11241_70514, 12288);
  // NPU Unit
  sub_0021_invoke(clean_outputs);

  // CPU Unit
  compute_sub_0022(compute_arena_sub_0022, buf__backbone_stage4_stage4_0_Concat_output_0_70347_70641_11245, buf__backbone_stage4_stage4_0_Concat_output_0_70347_70642_11249, buf__backbone_stage4_stage4_1_Slice_1_output_0_70349_70614_11261_70544, buf__backbone_stage4_stage4_1_Slice_output_0_70348_70613_11265_70550  );

  memcpy((sub_0023_arena + sub_0023_address__backbone_stage4_stage4_1_Slice_1_output_0_70349_70614_11261_70544), buf__backbone_stage4_stage4_1_Slice_1_output_0_70349_70614_11261_70544, 6144);
  memcpy((sub_0023_arena + sub_0023_address__backbone_stage4_stage4_1_Slice_output_0_70348_70613_11265_70550), buf__backbone_stage4_stage4_1_Slice_output_0_70348_70613_11265_70550, 6144);
  // NPU Unit
  sub_0023_invoke(clean_outputs);

  // CPU Unit
  compute_sub_0024(compute_arena_sub_0024, buf__backbone_stage4_stage4_1_Concat_output_0_70355_70643_11253, buf__backbone_stage4_stage4_1_Concat_output_0_70355_70644_11257, buf__backbone_stage4_stage4_2_Slice_1_output_0_70357_70616_11277_70568, buf__backbone_stage4_stage4_2_Slice_output_0_70356_70615_11281_70574  );

  memcpy((sub_0025_arena + sub_0025_address__backbone_stage4_stage4_2_Slice_1_output_0_70357_70616_11277_70568), buf__backbone_stage4_stage4_2_Slice_1_output_0_70357_70616_11277_70568, 6144);
  memcpy((sub_0025_arena + sub_0025_address__backbone_stage4_stage4_2_Slice_output_0_70356_70615_11281_70574), buf__backbone_stage4_stage4_2_Slice_output_0_70356_70615_11281_70574, 6144);
  // NPU Unit
  sub_0025_invoke(clean_outputs);

  // CPU Unit
  compute_sub_0026(compute_arena_sub_0026, buf__backbone_stage4_stage4_2_Concat_output_0_70363_70645_11269, buf__backbone_stage4_stage4_2_Concat_output_0_70363_70646_11273, buf__backbone_stage4_stage4_3_Slice_1_output_0_70365_70618_11285_70580, buf__backbone_stage4_stage4_3_Slice_output_0_70364_70617_11289_70586  );

  memcpy((sub_0027_arena + sub_0027_address__backbone_stage4_stage4_3_Slice_1_output_0_70365_70618_11285_70580), buf__backbone_stage4_stage4_3_Slice_1_output_0_70365_70618_11285_70580, 6144);
  memcpy((sub_0027_arena + sub_0027_address__backbone_stage4_stage4_3_Slice_output_0_70364_70617_11289_70586), buf__backbone_stage4_stage4_3_Slice_output_0_70364_70617_11289_70586, 6144);
  // NPU Unit
  sub_0027_invoke(clean_outputs);

  // CPU Unit
  compute_sub_0028(compute_arena_sub_0028, buf__723_70392_70620_11081, buf__backbone_stage4_stage4_3_Concat_output_0_70371_11357, buf__723_70392_70620, buf__fpn_Resize_output_0_70381_10947  );

  memcpy((sub_0029_arena + sub_0029_address__backbone_stage3_stage3_7_Concat_output_0_70338_11341), buf__backbone_stage3_stage3_7_Concat_output_0_70338_11341, 24576);
  memcpy((sub_0029_arena + sub_0029_address__fpn_Resize_output_0_70381_10947), buf__fpn_Resize_output_0_70381_10947, 49152);
  // NPU Unit
  sub_0029_invoke(clean_outputs);

  // CPU Unit
  compute_sub_0030(compute_arena_sub_0030, buf__722_70391_70619_11077, buf__722_70391_70619  );

}
