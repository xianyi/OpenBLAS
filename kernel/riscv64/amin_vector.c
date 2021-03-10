/***************************************************************************
Copyright (c) 2020, The OpenBLAS Project
All rights reserved.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
1. Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in
the documentation and/or other materials provided with the
distribution.
3. Neither the name of the OpenBLAS project nor the names of
its contributors may be used to endorse or promote products
derived from this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE OPENBLAS PROJECT OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#include "common.h"
#include <math.h>
#include <float.h>

#if !defined(DOUBLE)
#define RVV_EFLOAT RVV_E32
#define RVV_M RVV_M8
#define FLOAT_V_T float32xm8_t
#define VLEV_FLOAT vlev_float32xm8
#define VLSEV_FLOAT vlsev_float32xm8
#define VFREDMINVS_FLOAT vfredminvs_float32xm8
#define MASK_T e32xm8_t
#define VMFLTVF_FLOAT vmfltvf_e32xm8_float32xm8
#define VFMVVF_FLOAT vfmvvf_float32xm8
#define VFRSUBVF_MASK_FLOAT vfrsubvf_mask_float32xm8
#define VFMINVV_FLOAT vfminvv_float32xm8
#else
#define RVV_EFLOAT RVV_E64
#define RVV_M RVV_M8
#define FLOAT_V_T float64xm8_t
#define VLEV_FLOAT vlev_float64xm8
#define VLSEV_FLOAT vlsev_float64xm8
#define VFREDMINVS_FLOAT vfredminvs_float64xm8
#define MASK_T e64xm8_t
#define VMFLTVF_FLOAT vmfltvf_e64xm8_float64xm8
#define VFMVVF_FLOAT vfmvvf_float64xm8
#define VFRSUBVF_MASK_FLOAT vfrsubvf_mask_float64xm8
#define VFMINVV_FLOAT vfminvv_float64xm8
#endif

FLOAT CNAME(BLASLONG n, FLOAT *x, BLASLONG inc_x)
{
        BLASLONG i=0, j=0;
        if (n <= 0 || inc_x <= 0) 
                return(0.0);

        FLOAT minf=FLT_MAX;
        unsigned int gvl = 0;
        volatile FLOAT_V_T v0;
        FLOAT_V_T v_min, v1;
        FLOAT zero = 0.0;

        if(inc_x == 1){
                gvl = vsetvli(n, RVV_EFLOAT, RVV_M);
                if(gvl <= n/2){
                        v_min = VFMVVF_FLOAT(FLT_MAX, gvl);
                        for(i=0,j=0; i<n/(gvl*2); i++){

                                asm volatile(
                                        "vle.v          %1, (%4)                \n\t"
                                        "vle.v          %2, (%5)                \n\t"

                                        "vmflt.vf       v0, %1, %3              \n\t"
                                        "vfrsub.vf      %1, %1, %3, v0.t	\n\t"
                                        "vmflt.vf       v0, %2, %3              \n\t"
                                        "vfrsub.vf      %2, %2, %3, v0.t        \n\t"

                                        "vfmin.vv       %0, %0, %1              \n\t"
                                        "vfmin.vv       %0, %0, %2              \n\t"
                                        :"+v"(v_min)
                                        :"v"(v0), "v"(v1), "f"(zero), "r"(&x[j]), "r"(&x[j+gvl])
                                        :"v0"
                                );

                                j += gvl*2;
                        }
                        v1 = VFMVVF_FLOAT(FLT_MAX, gvl);
                        v0 = VFREDMINVS_FLOAT(v_min, v1, gvl);
                        minf = v0[0];
                }

                for(;j<n;){

                        gvl = vsetvli(n-j, RVV_EFLOAT, RVV_M);
                        asm volatile(
                                "vle.v          %0, (%2)                        \n\t"
                                "vmflt.vf       v0, %0, %1                      \n\t"
                                "vfrsub.vf      %0, %0, %1, v0.t                \n\t"
                                :"+v"(v0)
                                :"f"(zero), "r"(&x[j])
                                :"v0"
                        );

                        v1 = VFMVVF_FLOAT(FLT_MAX, gvl);
                        v0 = VFREDMINVS_FLOAT(v0, v1, gvl);
                        if(v0[0] < minf)
                                minf = v0[0];
                        j += gvl;
                }
        }else{
                gvl = vsetvli(n, RVV_EFLOAT, RVV_M);
                BLASLONG stride_x = inc_x * sizeof(FLOAT);

                if(gvl <= n/2){
                        BLASLONG idx = 0, inc_xv = inc_x * gvl;
                        v_min = VFMVVF_FLOAT(FLT_MAX, gvl);

                        for(i=0,j=0; i<n/(gvl*2); i++){

                                asm volatile(
                                        "vlse.v         %1, (%4), %6            \n\t"
                                        "vlse.v         %2, (%5), %6            \n\t"

                                        "vmflt.vf       v0, %1, %3              \n\t"
                                        "vfrsub.vf      %1, %1, %3, v0.t        \n\t"
                                        "vmflt.vf       v0, %2, %3              \n\t"
                                        "vfrsub.vf      %2, %2, %3, v0.t        \n\t"

                                        "vfmin.vv       %0, %0, %1              \n\t"
                                        "vfmin.vv       %0, %0, %2              \n\t"
                                        :"+v"(v_min)
                                        :"v"(v0), "v"(v1), "f"(zero), "r"(&x[idx]), "r"(&x[idx+inc_xv]), "r"(stride_x)
                                        :"v0"
                                        );

                                j += gvl*2;
                                idx += inc_xv*2;
                        }
                        v1 = VFMVVF_FLOAT(FLT_MAX, gvl);
                        v0 = VFREDMINVS_FLOAT(v_min, v1, gvl);
                        minf = v0[0];
                }

                for(;j<n;){
                        gvl = vsetvli(n-j, RVV_EFLOAT, RVV_M);
                        asm volatile(
                                "vlse.v         %0, (%2), %3            \n\t"
                                "vmflt.vf       v0, %0, %1              \n\t"
                                "vfrsub.vf      %0, %0, %1, v0.t        \n\t"
                                :"+v"(v0)
                                :"f"(zero), "r"(&x[j*inc_x]), "r"(stride_x)
                                :"v0"
                                );

                        v1 = VFMVVF_FLOAT(FLT_MAX, gvl);
                        v0 = VFREDMINVS_FLOAT(v0, v1, gvl);
                        if(v0[0] < minf)
                                minf = v0[0];
                        j += gvl;
                }
        }

        return(minf);
}


