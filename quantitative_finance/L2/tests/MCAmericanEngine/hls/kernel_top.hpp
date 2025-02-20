/*
 * Copyright 2019 Xilinx, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef _MCENGINE_TOP_H_
#define _MCENGINE_TOP_H_

#include "xf_fintech/mc_engine.hpp"
#include "xf_fintech/utils.hpp"

typedef double TEST_DT;

#define UN 1
#define UN_STEP 1
#define UN_PRICING 2
#define UN_MAX UN > UN_PRICING ? UN : UN_PRICING
#define iteration 4 //==calibsamples/1024
#define depthP 1024 * 100 * iteration
#define depthM 9 * 100

void kernel_top(TEST_DT underlying,
                TEST_DT volatility,
                TEST_DT riskFreeRate,
                TEST_DT dividendYield,
                TEST_DT timeLength,
                TEST_DT strike,
                bool optionType,
                ap_uint<32> seed[UN_MAX],
                ap_uint<UN * sizeof(TEST_DT) * 8> pdata[depthP],
                ap_uint<sizeof(TEST_DT) * 8> mdata[depthM],
                TEST_DT output[1],
                TEST_DT requiredTolerance,
                unsigned int calibSamples,
                unsigned int requiredSamples,
                unsigned int timeSteps);

#endif
