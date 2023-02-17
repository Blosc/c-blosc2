// Copyright (c) 2019 - 2021, Osamu Watanabe
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
//    modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
//    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

// Delimiting markers and marker segments
#define _SOC 0xFF4F
#define _SOT 0xFF90
#define _SOD 0xFF93
#define _EOC 0xFFD9

// Fixed information marker segments
#define _SIZ 0xFF51
#define _PRF 0xFF56
#define _CAP 0xFF50

// Functional marker segments
#define _COD 0xFF52
#define _COC 0xFF53
#define _RGN 0xFF5E
#define _QCD 0xFF5C
#define _QCC 0xFF5D
#define _POC 0xFF5F

// Pointer marker segments
#define _TLM 0xFF55
#define _PLM 0xFF57
#define _PLT 0xFF58
#define _PPM 0xFF60
#define _PPT 0xFF61

// In-bit-stream markers and marker segments
#define _SOP 0xFF91
#define _EPH 0xFF92

// Informational marker segments
#define _CRG 0xFF63
#define _COM 0xFF64

// Part 15 marker segment
#define _CPF 0xFF59
