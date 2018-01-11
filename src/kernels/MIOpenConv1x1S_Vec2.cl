/*******************************************************************************
 *
 * MIT License
 *
 * Copyright (c) 2017 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *******************************************************************************/

#define PPCAT_NX(A, B) A##B
#define PPCAT(A, B) PPCAT_NX(A, B)
#define TWO 2
#define FOUR 4
#define EIGHT 8

#if MIOPEN_USE_FP16 == 1
#pragma OPENCL EXTENSION cl_khr_fp16 : enable
#define _FLOAT half
#ifndef HALF_MAX
#define MAX_VAL 65504 /* max value */
#else
#define MAX_VAL HALF_MAX
#endif
#endif
#if MIOPEN_USE_FP32 == 1
#define _FLOAT float
#ifndef FLT_MAX
#define MAX_VAL 3.402823466e+38F /* max value */
#else
#define MAX_VAL FLT_MAX
#endif
#endif

#define _FLOAT2 PPCAT(_FLOAT, TWO)
#define _FLOAT4 PPCAT(_FLOAT, FOUR)
#define _FLOAT8 PPCAT(_FLOAT, EIGHT)

#define UNUSED __attribute__((__unused__))

#define DBG_OUT_OF_RNGE 0

// calculating the size of the area for weights prefetch

__attribute__((always_inline)) uint iDiv(uint v, uint d)
{
    uint r = (uint)((float)v * (1.0f / (float)d) + 0.00001f);
    return (r);
}

__attribute__((always_inline)) uint iMod(uint v, uint u, uint d)
{
    uint r = v - mul24(u, d);
    return (r);
}

__attribute__((reqd_work_group_size(MLO_GRP_SZ0, MLO_GRP_SZ1, MLO_GRP_SZ2))) __kernel void
MIOpenConv1x1(const __global _FLOAT* __restrict in_ptr,
              __constant _FLOAT* __restrict wei_ptr,
#if MLO_CONV_BIAS
              const __global _FLOAT* __restrict bias,
#endif
              __global _FLOAT* __restrict out_ptr,
              UNUSED _FLOAT dummy_val // nothing
              )
{

    _FLOAT2 weights[MLO_N_LCL_OUT_MAPS][MLO_N_LCL_IN_MAPS];

    uint gbl_id0  = get_global_id(0);
    uint batch_id = gbl_id0 / MLO_MAP_SZ4; // batch
    uint pos      = gbl_id0 % MLO_MAP_SZ4;

    uint out_grp_block = get_group_id(1); // block of outputs for the entire group
    uint out_id        = out_grp_block * MLO_N_LCL_OUT_MAPS;

<<<<<<< HEAD
    uint2 gbl_in_off   = (uint2)(batch_id * MLO_IN_BATCH_STRIDE + pos * MLO_READ_UNIT) +
                       (uint2)(0, MLO_N_LCL_IN_MAPS * MLO_IN_CHANNEL_STRIDE);

    uint2 wei_off = (uint2)(out_id) *
#if MLO_DIR_FORWARD == 1
                        (uint2)(MLO_WEI_BSTRIDE) +
                    (uint2)(0, MLO_N_LCL_IN_MAPS * MLO_WEI_CHANNEL_STRIDE)
#else
                        (uint2)(MLO_WEI_CHANNEL_STRIDE) +
                    (uint2)(0, MLO_N_LCL_IN_MAPS * MLO_WEI_BSTRIDE)
#endif
        ;

    _FLOAT2 accum[MLO_N_LCL_OUT_MAPS][MLO_READ_UNIT] = {(_FLOAT2)(0, 0)};
    _FLOAT2 dat[MLO_N_LCL_IN_MAPS][MLO_READ_UNIT];

    for(uint o = 0; o < MLO_N_LCL_OUT_MAPS; ++o)
    {
        for(uint i = 0; i < MLO_READ_UNIT; ++i)
        {
            accum[o][i] = (_FLOAT2)(0, 0);
        }
    }
=======
    uint2 gbl_in_off = (uint2)(batch_id * MLO_IN_BATCH_STRIDE + pos * MLO_READ_UNIT,
                               batch_id * MLO_IN_BATCH_STRIDE + pos * MLO_READ_UNIT +
                                   MLO_N_LCL_IN_MAPS * MLO_IN_CHANNEL_STRIDE);

#if MLO_DIR_FORWARD == 1
    uint2 wei_off = (uint2)(out_id * MLO_WEI_BSTRIDE,
                            out_id * MLO_WEI_BSTRIDE + MLO_N_LCL_IN_MAPS * MLO_WEI_CHANNEL_STRIDE);
#else
    uint2 wei_off = (uint2)(out_id * MLO_WEI_CHANNEL_STRIDE,
                            out_id * MLO_WEI_CHANNEL_STRIDE + MLO_N_LCL_IN_MAPS * MLO_WEI_BSTRIDE);
#endif

#if MIOPEN_USE_FP32 == 1
    _FLOAT2 accum[MLO_N_LCL_OUT_MAPS][MLO_READ_UNIT] = {(_FLOAT2)(0, 0)};
    _FLOAT2 dat[MLO_N_LCL_IN_MAPS][MLO_READ_UNIT];
#endif
#if MIOPEN_USE_FP16 == 1
    _FLOAT2 accum[MLO_N_LCL_OUT_MAPS * MLO_READ_UNIT] = {(_FLOAT2)(0, 0)};
    _FLOAT2 dat[MLO_N_LCL_IN_MAPS * MLO_READ_UNIT];
#endif

    // for(uint o = 0; o < MLO_N_LCL_OUT_MAPS; ++o)
    //{
    //    for(uint i = 0; i < MLO_READ_UNIT; ++i)
    //    {
    //#if MIOPEN_USE_FP32 == 1
    //        accum[o][i] = (_FLOAT2) (0, 0);
    //#endif
    //#if MIOPEN_USE_FP16 == 1
    //        accum[o * MLO_READ_UNIT + i] = (_FLOAT2) (0, 0);
    //#endif
    //    }
    //}
>>>>>>> fp16_kernels

    for(uint ci = 0; ci < MLO_CLOOP0;
        ci += 2,
             // move input offset
<<<<<<< HEAD
        gbl_in_off += (uint2)(2 * MLO_N_LCL_IN_MAPS * MLO_IN_CHANNEL_STRIDE),

             // move weights offset
        wei_off += (uint2)(2 * MLO_N_LCL_IN_MAPS) *
#if MLO_DIR_FORWARD == 1
                   (uint2)(MLO_WEI_CHANNEL_STRIDE)
#else
                   (uint2)(MLO_WEI_BSTRIDE)
=======
        gbl_in_off += (uint2)(MLO_N_LCL_IN_MAPS * MLO_IN_CHANNEL_STRIDE),

             // move weights offset
        wei_off +=
#if MLO_DIR_FORWARD == 1
        (uint2)(MLO_N_LCL_IN_MAPS * MLO_WEI_CHANNEL_STRIDE)
#else
        (uint2)(MLO_N_LCL_IN_MAPS * MLO_WEI_BSTRIDE)
>>>>>>> fp16_kernels
#endif
            )
    {
        // read weights

        uint o;
        uint2 wei_off1;
<<<<<<< HEAD
        for(o = 0, wei_off1 = wei_off; o < MLO_N_LCL_OUT_MAPS; ++o,
        wei_off1 +=
#if MLO_DIR_FORWARD == 1
                                                               (uint2)(MLO_WEI_BSTRIDE)
#else
                                                               (uint2)(MLO_WEI_CHANNEL_STRIDE)
=======
        for(o = 0, wei_off1 = wei_off; o < MLO_N_LCL_OUT_MAPS;
            ++o,
        wei_off1 +=
#if MLO_DIR_FORWARD == 1
            (uint2)(MLO_WEI_BSTRIDE, MLO_WEI_BSTRIDE)
#else
            (uint2)(MLO_WEI_CHANNEL_STRIDE, MLO_WEI_CHANNEL_STRIDE)
>>>>>>> fp16_kernels
#endif
                )
        {
            uint c;
            uint2 wei_off2;
<<<<<<< HEAD
            for(c = 0, wei_off2 = wei_off1; c < MLO_N_LCL_IN_MAPS; ++c,
            wei_off2 +=
#if MLO_DIR_FORWARD == 1
                                                                   (uint2)(MLO_WEI_CHANNEL_STRIDE)
#else
                                                                   (uint2)(MLO_WEI_BSTRIDE)
=======
            for(c = 0, wei_off2 = wei_off1; c < MLO_N_LCL_IN_MAPS;
                ++c,
            wei_off2 +=
#if MLO_DIR_FORWARD == 1
                (uint2)(MLO_WEI_CHANNEL_STRIDE, MLO_WEI_CHANNEL_STRIDE)
#else
                (uint2)(MLO_WEI_BSTRIDE, MLO_WEI_BSTRIDE)
>>>>>>> fp16_kernels
#endif
                    )
            {
#if MLO_CLOOP0 % 2 == 0
                weights[o][c] = (_FLOAT2)(wei_ptr[wei_off2.x], wei_ptr[wei_off2.y]);
#else
                weights[o][c].x = wei_ptr[wei_off2.x];
                weights[o][c].y = (ci + 1 >= MLO_CLOOP0) ? (_FLOAT)(0) : wei_ptr[wei_off2.y];
#endif
#if DBG_OUT_OF_RNGE
                if((wei_off2.x >= MLO_N_INPUTS * MLO_N_OUTPUTS) ||
                   (wei_off2.y >= MLO_N_INPUTS * MLO_N_OUTPUTS))
                {
                    printf("K:oor: weights\n");
                }
#endif
            }
        }

        // convolve with all weights
        // read data
        uint j;
        uint2 gbl_in_off1;
        for(j = 0, gbl_in_off1 = gbl_in_off; j < MLO_N_LCL_IN_MAPS;
<<<<<<< HEAD
            ++j, gbl_in_off1 += (uint2)(MLO_IN_CHANNEL_STRIDE))
=======
            ++j, gbl_in_off1 += (uint2)(MLO_IN_CHANNEL_STRIDE, MLO_IN_CHANNEL_STRIDE))
>>>>>>> fp16_kernels
        {
            for(uint i = 0; i < MLO_READ_UNIT; ++i)
            {
#if MLO_CLOOP0 % 2 == 0
<<<<<<< HEAD
                dat[j][i] = (_FLOAT2)(in_ptr[gbl_in_off1.x + i], in_ptr[gbl_in_off1.y + i]);
#else
                dat[j][i].x     = in_ptr[gbl_in_off1.x + i];
                dat[j][i].y     = (ci + 1 >= MLO_CLOOP0) ? (_FLOAT)(0) : in_ptr[gbl_in_off1.y + i];
=======
#if MIOPEN_USE_FP32 == 1
                dat[j][i] = (_FLOAT2)(in_ptr[gbl_in_off1.x + i], in_ptr[gbl_in_off1.y + i]);
#endif
#if MIOPEN_USE_FP16 == 1
                dat[j * MLO_READ_UNIT + i] =
                    (_FLOAT2)(in_ptr[gbl_in_off1.x + i], in_ptr[gbl_in_off1.y + i]);
#endif
#else
#if MIOPEN_USE_FP32 == 1
                dat[j][i].x     = in_ptr[gbl_in_off1.x + i];
                dat[j][i].y = (ci + 1 >= MLO_CLOOP0) ? (_FLOAT)(0) : in_ptr[gbl_in_off1.y + i];
#endif
#if MIOPEN_USE_FP16 == 1
                dat[j * MLO_READ_UNIT + i].x = in_ptr[gbl_in_off1.x + i];
                dat[j * MLO_READ_UNIT + i].y =
                    (ci + 1 >= MLO_CLOOP0) ? (_FLOAT)(0) : in_ptr[gbl_in_off1.y + i];
#endif
>>>>>>> fp16_kernels
#endif
#if DBG_OUT_OF_RNGE
                if((gbl_in_off1.x + i >= MLO_IN_BATCH_STRIDE * MLO_BATCH_SZ) ||
                   (gbl_in_off1.y + i >= MLO_IN_BATCH_STRIDE * MLO_BATCH_SZ))
                {
                    printf("K:oor: inputs\n");
                }
#endif
            }
        }

        // convolve
        for(uint o = 0; o < MLO_N_LCL_OUT_MAPS; ++o)
        {
<<<<<<< HEAD
            _FLOAT2 acc[MLO_READ_UNIT] = {(_FLOAT2)(0, 0)};
            for(uint c = 0; c < MLO_N_LCL_IN_MAPS; ++c)
            {
                _FLOAT2 we = weights[o][c];
                _FLOAT2* d = &dat[c][0];
                for(uint i = 0; i < MLO_READ_UNIT; ++i)
                {
                    acc[i] += d[i] * we;
                }
            }
            for(uint i = 0; i < MLO_READ_UNIT; ++i)
                accum[o][i] += acc[i];
=======
            for(uint c = 0; c < MLO_N_LCL_IN_MAPS; ++c)
            {
                for(uint i = 0; i < MLO_READ_UNIT; ++i)
                {
#if MLO_CLOOP0 % 2 == 0
#if MIOPEN_USE_FP32 == 1
                    accum[o][i] += dat[c][i] * weights[o][c];
#endif
#if MIOPEN_USE_FP16 == 1
                    accum[o * MLO_READ_UNIT + i] += dat[c * MLO_READ_UNIT + i] * weights[o][c];
#endif
#else
#if MIOPEN_USE_FP32 == 1
                    accum[o][i].x += dat[c][i].x * weights[o][c].x;
                    accum[o][i].y +=
                        (ci + 1 >= MLO_CLOOP0) ? (_FLOAT)(0) : dat[c][i].y * weights[o][c].y;
#endif
#if MIOPEN_USE_FP16 == 1
                    accum[o * MLO_READ_UNIT + i].x +=
                        dat[c * MLO_READ_UNIT + i].x * weights[o][c].x;
                    accum[o * MLO_READ_UNIT + i].y +=
                        (ci + 1 >= MLO_CLOOP0) ? (_FLOAT)(0)
                                               : dat[c * MLO_READ_UNIT + i].y * weights[o][c].y;
#endif
#endif
                }
            }
>>>>>>> fp16_kernels
        }
    }

    uint gbl_out_off =
        batch_id * MLO_OUT_BATCH_STRIDE + pos * MLO_READ_UNIT + out_id * MLO_OUT_CHANNEL_STRIDE;
    for(uint o = 0, gbl_out_off1 = gbl_out_off; o < MLO_N_LCL_OUT_MAPS;
        ++o, gbl_out_off1 += MLO_OUT_CHANNEL_STRIDE)
    {
        for(uint i = 0; i < MLO_READ_UNIT; ++i)
        {
<<<<<<< HEAD
            out_ptr[gbl_out_off1 + i] = accum[o][i].x + accum[o][i].y;
=======
#if MIOPEN_USE_FP32 == 1
            out_ptr[gbl_out_off1 + i] = accum[o][i].x + accum[o][i].y;
#endif
#if MIOPEN_USE_FP16 == 1
            out_ptr[gbl_out_off1 + i] =
                accum[o * MLO_READ_UNIT + i].x + accum[o * MLO_READ_UNIT + i].y;
#endif
>>>>>>> fp16_kernels
        }
    }
}

/************************************************************************
stride and padding
*************************************************************************/
__attribute__((reqd_work_group_size(MLO_GRP_SZ0, MLO_GRP_SZ1, MLO_GRP_SZ2))) __kernel void
MIOpenConv1x1pquv(const __global _FLOAT* __restrict in_ptr,
                  __constant _FLOAT* __restrict wei_ptr,
#if MLO_CONV_BIAS
                  const __global _FLOAT* __restrict bias,
#endif
                  __global _FLOAT* __restrict out_ptr,
                  UNUSED _FLOAT dummy_val // nothing
                  )
{

    _FLOAT weights[MLO_N_LCL_OUT_MAPS][MLO_N_LCL_IN_MAPS];

    uint gbl_id0 = get_global_id(0);

    uint batch_id  = gbl_id0 / MLO_MAP_SZ4; // batch
    uint pos       = gbl_id0 % MLO_MAP_SZ4;
    uint pos_out_y = pos / MLO_OUT_WIDTH4;
    uint pos_out_x = pos % MLO_OUT_WIDTH4;

#if MLO_DIR_FORWARD == 1
    uint pos_in_y = pos_out_y * MLO_FILTER_STRIDE1;
    uint pos_in_x = pos_out_x * MLO_FILTER_STRIDE0;
#else
<<<<<<< HEAD
    uint pos_in_y               = pos_out_y; /// MLO_FILTER_STRIDE1;   - divided already
    uint pos_in_x               = pos_out_x; // MLO_FILTER_STRIDE0;  - divided already
=======
    uint pos_in_y = pos_out_y; /// MLO_FILTER_STRIDE1;   - divided already
    uint pos_in_x = pos_out_x; // MLO_FILTER_STRIDE0;  - divided already
>>>>>>> fp16_kernels
#endif

    uint out_grp_block = get_group_id(1); // block of outputs for the entire group
    uint out_id        = out_grp_block * MLO_N_LCL_OUT_MAPS;

    uint gbl_in_off =
        batch_id * MLO_IN_BATCH_STRIDE + pos_in_y * MLO_IN_STRIDE + pos_in_x * MLO_READ_UNIT;
    //	bool vis = (pos_in_y < MLO_IN_HEIGHT);
    //	gbl_in_off = (vis) ? gbl_in_off : 0;

    uint wei_off = out_id *
#if MLO_DIR_FORWARD == 1
                   MLO_WEI_BSTRIDE
#else
                   MLO_WEI_CHANNEL_STRIDE
#endif
        ;

    _FLOAT accum[MLO_N_LCL_OUT_MAPS][MLO_READ_UNIT];
    _FLOAT dat[MLO_N_LCL_IN_MAPS][MLO_READ_UNIT];

    for(uint o = 0; o < MLO_N_LCL_OUT_MAPS; ++o)
    {
        for(uint i = 0; i < MLO_READ_UNIT; ++i)
        {
<<<<<<< HEAD
            accum[o][i] = (_FLOAT)(0);
        }
    }

    const __global _FLOAT* i_ptr = in_ptr + gbl_in_off;
    __constant _FLOAT* w_ptr     = wei_ptr + wei_off;
=======
            accum[o][i] = 0;
        }
    }

    const __global _FLOAT* i_ptr = &in_ptr[gbl_in_off];
    __constant _FLOAT* w_ptr     = &wei_ptr[wei_off];
>>>>>>> fp16_kernels
    for(uint ci = 0; ci < MLO_CLOOP0; ++ci)
    {

        // convolve with all weights
        // read data

        for(uint j = 0; j < MLO_N_LCL_IN_MAPS; ++j)
        {
            uint i = 0;
#if MLO_READ_UNIT > 1
            for(; i < MLO_READ_UNIT - 1; ++i)
            {
                uint off = i
#if MLO_DIR_FORWARD == 1
                           * MLO_FILTER_STRIDE0
#endif
                    ;
<<<<<<< HEAD
                dat[j][i] = *(i_ptr + off);
=======
                dat[j][i] = i_ptr[off];
>>>>>>> fp16_kernels
            }
#endif

            for(; i < MLO_READ_UNIT; ++i)
            {
<<<<<<< HEAD
                //				vis &= (pos_in_x + i*MLO_FILTER_STRIDE0 <
=======
                //                                vis &= (pos_in_x + i*MLO_FILTER_STRIDE0 <
>>>>>>> fp16_kernels
                // MLO_IN_WIDTH);
                uint off = i
#if MLO_DIR_FORWARD == 1
                           * MLO_FILTER_STRIDE0
#endif
                    ;
<<<<<<< HEAD
                //				off = (vis) ? off : 0;
                _FLOAT val = *(i_ptr + off);
                dat[j][i]  = val;
                //              dat[j][i] = (vis)? dat[j][i] : (_FLOAT)(0);
=======
                //                                off = (vis) ? off : 0;
                _FLOAT val = i_ptr[off];
                dat[j][i]  = val;
                //              dat[j][i] = (vis)? dat[j][i] : 0;
>>>>>>> fp16_kernels
            }

            i_ptr += MLO_IN_CHANNEL_STRIDE;
        }
        // read weights
        __constant _FLOAT* w_ptr0 = w_ptr;

        for(uint o = 0; o < MLO_N_LCL_OUT_MAPS; ++o)
        {

            __constant _FLOAT* w_ptr1 = w_ptr0;

            for(uint c = 0; c < MLO_N_LCL_IN_MAPS; ++c)
            {
                weights[o][c] = *w_ptr1;
                w_ptr1 +=
#if MLO_DIR_FORWARD == 1
                    MLO_WEI_CHANNEL_STRIDE
#else
                    MLO_WEI_BSTRIDE
#endif
                    ;
            }

            w_ptr0 +=
#if MLO_DIR_FORWARD == 1
                MLO_WEI_BSTRIDE
#else
                MLO_WEI_CHANNEL_STRIDE
#endif
                ;
        }

        w_ptr += MLO_N_LCL_IN_MAPS *
#if MLO_DIR_FORWARD == 1
                 MLO_WEI_CHANNEL_STRIDE
#else
                 MLO_WEI_BSTRIDE
#endif
            ;
        // convolve
        for(uint o = 0; o < MLO_N_LCL_OUT_MAPS; ++o)
        {
            for(uint c = 0; c < MLO_N_LCL_IN_MAPS; ++c)
            {
                for(uint i = 0; i < MLO_READ_UNIT; ++i)
                {
                    accum[o][i] += dat[c][i] * weights[o][c];
#if 0
                    if (pos_out_y == 2 && pos_out_x == 0)
                    {
<<<<<<< HEAD
                        printf((__constant char *)"K:c: %f %f %f %f\n",
                        accum[o][i],
                        dat[c][i] * weights[o][c],
                        dat[c][i],
                        weights[o][c]
                        );
=======
                            printf((__constant char *)"K:c: %f %f %f %f\n",
                            accum[o][i],
                            dat[c][i] * weights[o][c],
                            dat[c][i],
                            weights[o][c]
                            );
>>>>>>> fp16_kernels
                    }
#endif
                }
            }
        }
    }

    uint out_y = pos_out_y
#if MLO_DIR_FORWARD == 0
                 * MLO_FILTER_STRIDE1
#endif
        ;
    uint out_x = pos_out_x
#if MLO_DIR_FORWARD == 0
                 * MLO_FILTER_STRIDE0
#endif
        ;

    uint gbl_out_off = batch_id * MLO_OUT_BATCH_STRIDE + out_id * MLO_OUT_CHANNEL_STRIDE +
                       out_y * MLO_OUT_STRIDE + out_x * MLO_READ_UNIT;

<<<<<<< HEAD
    __global _FLOAT* q = out_ptr + gbl_out_off;

    for(uint o = 0; o < MLO_N_LCL_OUT_MAPS; ++o, q += MLO_OUT_CHANNEL_STRIDE)
=======
    for(uint o = 0, gbl_out_off1 = gbl_out_off; o < MLO_N_LCL_OUT_MAPS;
        ++o, gbl_out_off1 += MLO_OUT_CHANNEL_STRIDE)
>>>>>>> fp16_kernels
    {

        for(uint i = 0; i < MLO_READ_UNIT; ++i)
        {
<<<<<<< HEAD
            __global _FLOAT* q1 = q;
            q1 += i
#if MLO_DIR_FORWARD == 0

                  * MLO_FILTER_STRIDE0;
#endif
            ;
            *q1 = accum[o][i];
=======
            uint out_off = gbl_out_off1 +
                           i
#if MLO_DIR_FORWARD == 0
                               * MLO_FILTER_STRIDE0
#endif
                ;
            out_ptr[out_off] = accum[o][i];
>>>>>>> fp16_kernels

#if MLO_DIR_FORWARD == 0
            for(uint s = 1; s < MLO_FILTER_STRIDE0; ++s)
            {
#if MLO_HORIZ_ALIGNED == 0
                if(out_x + s < MLO_OUT_WIDTH)
#endif
                {
<<<<<<< HEAD
                    *(q1 + s) = (_FLOAT)(0);
=======
                    out_ptr[out_off + s] = 0;
>>>>>>> fp16_kernels
                }
            }
#endif
        }

#if MLO_DIR_FORWARD == 0
<<<<<<< HEAD
        __global _FLOAT* q2 = q;
        for(uint j = 1; j < MLO_FILTER_STRIDE1; ++j)
        {
            q2 += MLO_OUT_STRIDE;
=======
        for(uint j = 1; j < MLO_FILTER_STRIDE1; ++j)
        {
>>>>>>> fp16_kernels
#if MLO_VERT_ALIGNED == 0
            if(out_y + j < MLO_OUT_HEIGHT)
#endif
            {
<<<<<<< HEAD

=======
                uint out_off = gbl_out_off1 + j * MLO_OUT_STRIDE;
>>>>>>> fp16_kernels
                for(uint s = 0; s < MLO_READ_UNIT * MLO_FILTER_STRIDE0; ++s)
                {
#if MLO_HORIZ_ALIGNED == 0
                    if(out_x + s < MLO_OUT_WIDTH)
#endif
                    {
<<<<<<< HEAD
                        *(q2 + s) = (_FLOAT)(0);
=======
                        out_ptr[out_off + s] = 0;
>>>>>>> fp16_kernels
                    }
                }
            }
        }
#endif
    }
}
