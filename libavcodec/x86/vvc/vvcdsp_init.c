/*
 * VVC DSP init for x86
 *
 * Copyright (C) 2022-2023 Nuo Mi
 * Copyright (c) 2023 Wu Jianhua <toqsxw@outlook.com>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "config.h"

#include "libavutil/cpu.h"
#include "libavcodec/vvc/vvcdec.h"
#include "libavcodec/vvc/vvcdsp.h"
#include "libavutil/x86/asm.h"
#include "libavutil/x86/cpu.h"
#include <stdlib.h>
#include <time.h>

#define bf(fn, bd,  opt) fn##_##bd##_##opt
#define BF(fn, bpc, opt) fn##_##bpc##bpc_##opt

#define ALF_BPC_FUNCS(bpc, opt)                                                                                         \
void BF(ff_vvc_alf_filter_luma, bpc, opt)(uint8_t *dst, ptrdiff_t dst_stride,                                           \
    const uint8_t *src, ptrdiff_t src_stride, ptrdiff_t width, ptrdiff_t height,                                        \
    const int16_t *filter, const int16_t *clip, ptrdiff_t stride, ptrdiff_t vb_pos, ptrdiff_t pixel_max);               \
void BF(ff_vvc_alf_filter_chroma, bpc, opt)(uint8_t *dst, ptrdiff_t dst_stride,                                         \
    const uint8_t *src, ptrdiff_t src_stride, ptrdiff_t width, ptrdiff_t height,                                        \
    const int16_t *filter, const int16_t *clip, ptrdiff_t stride, ptrdiff_t vb_pos, ptrdiff_t pixel_max);               \
void BF(ff_vvc_alf_classify_grad, bpc, opt)(int *gradient_sum,                                                          \
    const uint8_t *src, ptrdiff_t src_stride, intptr_t width, intptr_t height, intptr_t vb_pos);                        \
void BF(ff_vvc_alf_classify, bpc, opt)(int *class_idx, int *transpose_idx, const int *gradient_sum,                     \
    intptr_t width, intptr_t height, intptr_t vb_pos, intptr_t bit_depth);                                              \

#define ALF_FUNCS(bpc, bd, opt)                                                                                         \
static void bf(alf_classify, bd, opt)(int *class_idx, int *transpose_idx,                                               \
    const uint8_t *src, ptrdiff_t src_stride, int width, int height, int vb_pos, int *gradient_tmp)                     \
{                                                                                                                       \
    BF(ff_vvc_alf_classify_grad, bpc, opt)(gradient_tmp, src, src_stride, width, height, vb_pos);                       \
    BF(ff_vvc_alf_classify, bpc, opt)(class_idx, transpose_idx, gradient_tmp, width, height, vb_pos, bd);               \
}                                                                                                                       \
static void bf(alf_filter_luma, bd, opt)(uint8_t *dst, ptrdiff_t dst_stride, const uint8_t *src, ptrdiff_t src_stride,  \
    int width, int height, const int16_t *filter, const int16_t *clip, const int vb_pos)                                \
{                                                                                                                       \
    const int param_stride  = (width >> 2) * ALF_NUM_COEFF_LUMA;                                                        \
    BF(ff_vvc_alf_filter_luma, bpc, opt)(dst, dst_stride, src, src_stride, width, height,                               \
        filter, clip, param_stride, vb_pos, (1 << bd)  - 1);                                                            \
}                                                                                                                       \
static void bf(alf_filter_chroma, bd, opt)(uint8_t *dst, ptrdiff_t dst_stride, const uint8_t *src, ptrdiff_t src_stride,\
    int width, int height, const int16_t *filter, const int16_t *clip, const int vb_pos)                                \
{                                                                                                                       \
    BF(ff_vvc_alf_filter_chroma, bpc, opt)(dst, dst_stride, src, src_stride, width, height,                             \
        filter, clip, 0, vb_pos,(1 << bd)  - 1);                                                                        \
}                                                                                                                       \

ALF_BPC_FUNCS(8,  avx2)
ALF_BPC_FUNCS(16, avx2)

ALF_FUNCS(8,  8,  avx2)
ALF_FUNCS(16, 10, avx2)
ALF_FUNCS(16, 12, avx2)

#define ALF_INIT(bd) do {                                                       \
        c->alf.filter[LUMA] = alf_filter_luma_##bd##_avx2;                      \
        c->alf.filter[CHROMA] = alf_filter_chroma_##bd##_avx2;                  \
        c->alf.classify = alf_classify_##bd##_avx2;                             \
    } while (0)


#define SAO_FILTER_FUNCS(w, bd, opt)                                                  \
void ff_vvc_sao_band_filter_##w##_##bd##_##opt(                                       \
    uint8_t *_dst, const uint8_t *_src, ptrdiff_t _stride_dst, ptrdiff_t _stride_src, \
    const int16_t *sao_offset_val, int sao_left_class, int width, int height);        \
void ff_vvc_sao_edge_filter_##w##_##bd##_##opt(                                       \
    uint8_t *_dst, const uint8_t *_src, ptrdiff_t stride_dst,                         \
    const int16_t *sao_offset_val, int eo, int width, int height);                    \

#define SAO_FUNCS(bd, opt)                                                            \
    SAO_FILTER_FUNCS(8,   bd, opt)                                                    \
    SAO_FILTER_FUNCS(16,  bd, opt)                                                    \
    SAO_FILTER_FUNCS(32,  bd, opt)                                                    \
    SAO_FILTER_FUNCS(48,  bd, opt)                                                    \
    SAO_FILTER_FUNCS(64,  bd, opt)                                                    \
    SAO_FILTER_FUNCS(80,  bd, opt)                                                    \
    SAO_FILTER_FUNCS(96,  bd, opt)                                                    \
    SAO_FILTER_FUNCS(112, bd, opt)                                                    \
    SAO_FILTER_FUNCS(128, bd, opt)                                                    \

SAO_FUNCS(8,  avx2)
SAO_FUNCS(10, avx2)
SAO_FUNCS(12, avx2)

#define SAO_FILTER_INIT(type, bd, opt) do {                                       \
    c->sao.type##_filter[0]       = ff_vvc_sao_##type##_filter_8_##bd##_##opt;    \
    c->sao.type##_filter[1]       = ff_vvc_sao_##type##_filter_16_##bd##_##opt;   \
    c->sao.type##_filter[2]       = ff_vvc_sao_##type##_filter_32_##bd##_##opt;   \
    c->sao.type##_filter[3]       = ff_vvc_sao_##type##_filter_48_##bd##_##opt;   \
    c->sao.type##_filter[4]       = ff_vvc_sao_##type##_filter_64_##bd##_##opt;   \
    c->sao.type##_filter[5]       = ff_vvc_sao_##type##_filter_80_##bd##_##opt;   \
    c->sao.type##_filter[6]       = ff_vvc_sao_##type##_filter_96_##bd##_##opt;   \
    c->sao.type##_filter[7]       = ff_vvc_sao_##type##_filter_112_##bd##_##opt;  \
    c->sao.type##_filter[8]       = ff_vvc_sao_##type##_filter_128_##bd##_##opt;  \
} while (0)

#define SAO_INIT(bd, opt) do {                                                    \
    SAO_FILTER_INIT(edge, bd, opt);                                               \
    SAO_FILTER_INIT(band, bd, opt);                                               \
} while (0)

#define PEL_LINK(dst, C, W, idx1, idx2, name, D, opt)                                   \
    dst[C][W][idx1][idx2] = ff_vvc_put_## name ## _ ## D ## _##opt;                     \
    dst ## _uni[C][W][idx1][idx2] = ff_vvc_put_uni_ ## name ## _ ## D ## _##opt;        \

#define MC_TAP_LINKS(pointer, C, my, mx, fname, bitd, opt )         \
    PEL_LINK(pointer, C, 1, my , mx , fname##4 ,  bitd, opt );      \
    PEL_LINK(pointer, C, 2, my , mx , fname##8 ,  bitd, opt );      \
    PEL_LINK(pointer, C, 3, my , mx , fname##16,  bitd, opt );      \
    PEL_LINK(pointer, C, 4, my , mx , fname##32,  bitd, opt );      \
    PEL_LINK(pointer, C, 5, my , mx , fname##64,  bitd, opt );      \
    PEL_LINK(pointer, C, 6, my , mx , fname##128, bitd, opt );      \

#define MC_8TAP_LINKS(pointer, my, mx, fname, bitd, opt)            \
    MC_TAP_LINKS(pointer, LUMA, my, mx, fname, bitd, opt)           \

#define MC_8TAP_LINKS_SSE4(bd)                                      \
    MC_8TAP_LINKS(c->inter.put, 0, 0, pixels, bd, sse4);            \
    MC_8TAP_LINKS(c->inter.put, 0, 1, 8tap_h, bd, sse4);            \
    MC_8TAP_LINKS(c->inter.put, 1, 0, 8tap_v, bd, sse4);            \
    MC_8TAP_LINKS(c->inter.put, 1, 1, 8tap_hv, bd, sse4)            \

#define MC_4TAP_LINKS(pointer, my, mx, fname, bitd, opt)            \
    PEL_LINK(pointer, CHROMA, 0, my , mx , fname##2 ,  bitd, opt ); \
    MC_TAP_LINKS(pointer, CHROMA, my, mx, fname, bitd, opt)         \

#define MC_4TAP_LINKS_SSE4(bd)                                      \
    MC_4TAP_LINKS(c->inter.put, 0, 0, pixels, bd, sse4);            \
    MC_4TAP_LINKS(c->inter.put, 0, 1, 4tap_h, bd, sse4);            \
    MC_4TAP_LINKS(c->inter.put, 1, 0, 4tap_v, bd, sse4);            \
    MC_4TAP_LINKS(c->inter.put, 1, 1, 4tap_hv, bd, sse4)            \

#define MC_LINK_SSE4(bd)                                            \
    MC_4TAP_LINKS_SSE4(bd)                                          \
    MC_8TAP_LINKS_SSE4(bd)

#define PEL_PROTOTYPE(name, D, opt) \
void ff_vvc_put_ ## name ## _ ## D ## _##opt(int16_t *dst, const uint8_t *_src, ptrdiff_t _srcstride, int height, const int8_t *hf, const int8_t *vf, int width);                               \
void ff_vvc_put_uni_ ## name ## _ ## D ## _##opt(uint8_t *_dst, ptrdiff_t _dststride, const uint8_t *_src, ptrdiff_t _srcstride, int height, const int8_t *hf, const int8_t *vf, int width);    \

#define MC_8TAP_PROTOTYPES(fname, bitd, opt)    \
    PEL_PROTOTYPE(fname##4,  bitd, opt);        \
    PEL_PROTOTYPE(fname##8,  bitd, opt);        \
    PEL_PROTOTYPE(fname##16, bitd, opt);        \
    PEL_PROTOTYPE(fname##32, bitd, opt);        \
    PEL_PROTOTYPE(fname##64, bitd, opt);        \
    PEL_PROTOTYPE(fname##128, bitd, opt)

MC_8TAP_PROTOTYPES(pixels  ,  8, sse4);
MC_8TAP_PROTOTYPES(pixels  , 10, sse4);
MC_8TAP_PROTOTYPES(pixels  , 12, sse4);
MC_8TAP_PROTOTYPES(8tap_h  ,  8, sse4);
MC_8TAP_PROTOTYPES(8tap_h  , 10, sse4);
MC_8TAP_PROTOTYPES(8tap_h  , 12, sse4);
MC_8TAP_PROTOTYPES(8tap_v  ,  8, sse4);
MC_8TAP_PROTOTYPES(8tap_v  , 10, sse4);
MC_8TAP_PROTOTYPES(8tap_v  , 12, sse4);
MC_8TAP_PROTOTYPES(8tap_hv ,  8, sse4);
MC_8TAP_PROTOTYPES(8tap_hv , 10, sse4);
MC_8TAP_PROTOTYPES(8tap_hv , 12, sse4);

#define MC_4TAP_PROTOTYPES(fname, bitd, opt)    \
    PEL_PROTOTYPE(fname##2,  bitd, opt);        \
    PEL_PROTOTYPE(fname##4,  bitd, opt);        \
    PEL_PROTOTYPE(fname##8,  bitd, opt);        \
    PEL_PROTOTYPE(fname##16, bitd, opt);        \
    PEL_PROTOTYPE(fname##32, bitd, opt);        \
    PEL_PROTOTYPE(fname##64, bitd, opt);        \
    PEL_PROTOTYPE(fname##128, bitd, opt)

#define MC_4TAP_PROTOTYPES_SSE4(bitd)           \
    PEL_PROTOTYPE(pixels2, bitd, sse4);         \
    MC_4TAP_PROTOTYPES(4tap_h, bitd, sse4);     \
    MC_4TAP_PROTOTYPES(4tap_v, bitd, sse4);     \
    MC_4TAP_PROTOTYPES(4tap_hv, bitd, sse4);    \

MC_4TAP_PROTOTYPES_SSE4(8)
MC_4TAP_PROTOTYPES_SSE4(10)
MC_4TAP_PROTOTYPES_SSE4(12)

#if HAVE_AVX2_EXTERNAL
#define MC_8TAP_PROTOTYPES_AVX2(fname)              \
    PEL_PROTOTYPE(fname##32 , 8, avx2);             \
    PEL_PROTOTYPE(fname##64 , 8, avx2);             \
    PEL_PROTOTYPE(fname##128, 8, avx2);             \
    PEL_PROTOTYPE(fname##16 ,10, avx2);             \
    PEL_PROTOTYPE(fname##32 ,10, avx2);             \
    PEL_PROTOTYPE(fname##64 ,10, avx2);             \
    PEL_PROTOTYPE(fname##128,10, avx2);             \
    PEL_PROTOTYPE(fname##16 ,12, avx2);             \
    PEL_PROTOTYPE(fname##32 ,12, avx2);             \
    PEL_PROTOTYPE(fname##64 ,12, avx2);             \
    PEL_PROTOTYPE(fname##128,12, avx2)              \

MC_8TAP_PROTOTYPES_AVX2(pixels);
MC_8TAP_PROTOTYPES_AVX2(8tap_h);
MC_8TAP_PROTOTYPES_AVX2(8tap_v);
MC_8TAP_PROTOTYPES_AVX2(8tap_hv);
PEL_PROTOTYPE(8tap_hv16, 8, avx2);

MC_8TAP_PROTOTYPES_AVX2(4tap_h);
MC_8TAP_PROTOTYPES_AVX2(4tap_v);
MC_8TAP_PROTOTYPES_AVX2(4tap_hv);
#endif

#define mc_rep_func(name, bitd, step, W, opt) \
void ff_vvc_put_##name##W##_##bitd##_##opt(int16_t *_dst,                                                       \
    const uint8_t *_src, ptrdiff_t _srcstride, int height, const int8_t *hf, const int8_t *vf, int width)       \
{                                                                                                               \
    int i;                                                                                                      \
    int16_t *dst;                                                                                               \
    for (i = 0; i < W; i += step) {                                                                             \
        const uint8_t *src  = _src + (i * ((bitd + 7) / 8));                                                    \
        dst = _dst + i;                                                                                         \
        ff_vvc_put_##name##step##_##bitd##_##opt(dst, src, _srcstride, height, hf, vf, width);                  \
    }                                                                                                           \
}
#define mc_rep_uni_func(name, bitd, step, W, opt) \
void ff_vvc_put_uni_##name##W##_##bitd##_##opt(uint8_t *_dst, ptrdiff_t dststride,                              \
    const uint8_t *_src, ptrdiff_t _srcstride, int height, const int8_t *hf, const int8_t *vf, int width)       \
{                                                                                                               \
    int i;                                                                                                      \
    uint8_t *dst;                                                                                               \
    for (i = 0; i < W; i += step) {                                                                             \
        const uint8_t *src = _src + (i * ((bitd + 7) / 8));                                                     \
        dst = _dst + (i * ((bitd + 7) / 8));                                                                    \
        ff_vvc_put_uni_##name##step##_##bitd##_##opt(dst, dststride, src, _srcstride,                           \
                                                          height, hf, vf, width);                               \
    }                                                                                                           \
}

#define mc_rep_funcs(name, bitd, step, W, opt)      \
    mc_rep_func(name, bitd, step, W, opt)           \
    mc_rep_uni_func(name, bitd, step, W, opt)

#define MC_REP_FUNCS_SSE4(fname)                \
    mc_rep_funcs(fname, 8, 16,128, sse4)        \
    mc_rep_funcs(fname, 8, 16, 64, sse4)        \
    mc_rep_funcs(fname, 8, 16, 32, sse4)        \
    mc_rep_funcs(fname,10,  8,128, sse4)        \
    mc_rep_funcs(fname,10,  8, 64, sse4)        \
    mc_rep_funcs(fname,10,  8, 32, sse4)        \
    mc_rep_funcs(fname,10,  8, 16, sse4)        \
    mc_rep_funcs(fname,12,  8,128, sse4)        \
    mc_rep_funcs(fname,12,  8, 64, sse4)        \
    mc_rep_funcs(fname,12,  8, 32, sse4)        \
    mc_rep_funcs(fname,12,  8, 16, sse4)        \

MC_REP_FUNCS_SSE4(pixels)
MC_REP_FUNCS_SSE4(4tap_h)
MC_REP_FUNCS_SSE4(4tap_v)
MC_REP_FUNCS_SSE4(4tap_hv)
MC_REP_FUNCS_SSE4(8tap_h)
MC_REP_FUNCS_SSE4(8tap_v)
MC_REP_FUNCS_SSE4(8tap_hv)
mc_rep_funcs(8tap_hv, 8, 8, 16, sse4)

#if HAVE_AVX2_EXTERNAL

#define MC_REP_FUNCS_AVX2(fname)                \
    mc_rep_funcs(fname, 8, 32, 64, avx2)       \
    mc_rep_funcs(fname, 8, 32,128, avx2)       \
    mc_rep_funcs(fname,10, 16, 32, avx2)       \
    mc_rep_funcs(fname,10, 16, 64, avx2)       \
    mc_rep_funcs(fname,10, 16,128, avx2)       \
    mc_rep_funcs(fname,12, 16, 32, avx2)       \
    mc_rep_funcs(fname,12, 16, 64, avx2)       \
    mc_rep_funcs(fname,12, 16,128, avx2)       \

MC_REP_FUNCS_AVX2(pixels)
MC_REP_FUNCS_AVX2(8tap_h)
MC_REP_FUNCS_AVX2(8tap_v)
MC_REP_FUNCS_AVX2(8tap_hv)
MC_REP_FUNCS_AVX2(4tap_h)
MC_REP_FUNCS_AVX2(4tap_v)
MC_REP_FUNCS_AVX2(4tap_hv)
#endif

#define AVG_BPC_FUNC(bpc, opt)                                                                      \
void BF(ff_vvc_avg, bpc, opt)(uint8_t *dst, ptrdiff_t dst_stride,                                   \
    const int16_t *src0, const int16_t *src1, intptr_t width, intptr_t height, intptr_t pixel_max); \
void BF(ff_vvc_w_avg, bpc, opt)(uint8_t *dst, ptrdiff_t dst_stride,                                 \
    const int16_t *src0, const int16_t *src1, intptr_t width, intptr_t height,                      \
    intptr_t denom, intptr_t w0, intptr_t w1,  intptr_t o0, intptr_t o1, intptr_t pixel_max);       \


#define AVG_FUNCS(bpc, bd, opt)                                                                     \
static void bf(avg, bd, opt)(uint8_t *dst, ptrdiff_t dst_stride,                                    \
    const int16_t *src0, const int16_t *src1, int width, int height)                                \
{                                                                                                   \
    BF(ff_vvc_avg, bpc, opt)(dst, dst_stride, src0, src1, width, height, (1 << bd)  - 1);           \
}                                                                                                   \
static void bf(w_avg, bd, opt)(uint8_t *dst, ptrdiff_t dst_stride,                                  \
    const int16_t *src0, const int16_t *src1, int width, int height,                                \
    int denom, int w0, int w1, int o0, int o1)                                                      \
{                                                                                                   \
    BF(ff_vvc_w_avg, bpc, opt)(dst, dst_stride, src0, src1, width, height,                          \
        denom, w0, w1, o0, o1, (1 << bd)  - 1);                                                     \
}

AVG_BPC_FUNC(8,   avx2)
AVG_BPC_FUNC(16,  avx2)

AVG_FUNCS(8,  8,  avx2)
AVG_FUNCS(16, 10, avx2)
AVG_FUNCS(16, 12, avx2)

#define AVG_INIT(bd, opt) do {                                          \
    c->inter.avg    = bf(avg, bd, opt);                                 \
    c->inter.w_avg  = bf(w_avg, bd, opt);                               \
} while (0)

#define MC_TAP_LINKS_AVX2(C,tap,bd) do {                                        \
        PEL_LINK(c->inter.put, C, 4, 0, 0, pixels32, bd, avx2)                  \
        PEL_LINK(c->inter.put, C, 5, 0, 0, pixels64, bd, avx2)                  \
        PEL_LINK(c->inter.put, C, 6, 0, 0, pixels128, bd, avx2)                 \
        PEL_LINK(c->inter.put, C, 4, 0, 1, tap##tap_h32, bd, avx2)              \
        PEL_LINK(c->inter.put, C, 5, 0, 1, tap##tap_h64, bd, avx2)              \
        PEL_LINK(c->inter.put, C, 6, 0, 1, tap##tap_h128, bd, avx2)             \
        PEL_LINK(c->inter.put, C, 4, 1, 0, tap##tap_v32, bd, avx2)              \
        PEL_LINK(c->inter.put, C, 5, 1, 0, tap##tap_v64, bd, avx2)              \
        PEL_LINK(c->inter.put, C, 6, 1, 0, tap##tap_v128, bd, avx2)             \
    } while (0)

#define MC_LINKS_AVX2(bd)                                                       \
    MC_TAP_LINKS_AVX2(LUMA,   8, bd);                                           \
    MC_TAP_LINKS_AVX2(CHROMA, 4, bd);                                           \

#define MC_TAP_LINKS_16BPC_AVX2(C, tap, bd) do {                                \
        PEL_LINK(c->inter.put, C, 3, 0, 0, pixels16, bd, avx2)                  \
        PEL_LINK(c->inter.put, C, 3, 0, 1, tap##tap_h16, bd, avx2)              \
        PEL_LINK(c->inter.put, C, 3, 1, 0, tap##tap_v16, bd, avx2)              \
        PEL_LINK(c->inter.put, C, 3, 1, 1, tap##tap_hv16, bd, avx2)             \
        PEL_LINK(c->inter.put, C, 4, 1, 1, tap##tap_hv32, bd, avx2)             \
        PEL_LINK(c->inter.put, C, 5, 1, 1, tap##tap_hv64, bd, avx2)             \
        PEL_LINK(c->inter.put, C, 6, 1, 1, tap##tap_hv128, bd, avx2)            \
    } while (0)

#define MC_LINKS_16BPC_AVX2(bd)                                                 \
    MC_TAP_LINKS_16BPC_AVX2(LUMA,   8, bd);                                     \
    MC_TAP_LINKS_16BPC_AVX2(CHROMA, 4, bd);                                     \

void ff_vvc_dsp_init_x86(VVCDSPContext *const c, const int bd)
{
    const int cpu_flags = av_get_cpu_flags();

    if (bd == 8) {
        if (EXTERNAL_SSE4(cpu_flags)) {
            MC_LINK_SSE4(8);
        }
        if (EXTERNAL_AVX2_FAST(cpu_flags)) {
            MC_LINKS_AVX2(8);
            //hevc avx2 8 bits hv still have issues, will need skip it
        }
    } else if (bd == 10) {
        if (EXTERNAL_SSE4(cpu_flags)) {
            MC_LINK_SSE4(10)
        }
        if (EXTERNAL_AVX2_FAST(cpu_flags)) {
            MC_LINKS_AVX2(10);
            MC_LINKS_16BPC_AVX2(10);
        }
    } else if (bd == 12) {
        if (EXTERNAL_SSE4(cpu_flags)) {
            MC_LINK_SSE4(12);
        }
        if (EXTERNAL_AVX2_FAST(cpu_flags)) {
            MC_LINKS_AVX2(12);
            MC_LINKS_16BPC_AVX2(12);
        }
    }

    if (EXTERNAL_AVX2(cpu_flags)) {
        switch (bd) {
            case 8:
                ALF_INIT(8);
                AVG_INIT(8, avx2);
                c->sao.band_filter[0] = ff_vvc_sao_band_filter_8_8_avx2;
                c->sao.band_filter[1] = ff_vvc_sao_band_filter_16_8_avx2;
                break;
            case 10:
                ALF_INIT(10);
                AVG_INIT(10, avx2);
                c->sao.band_filter[0] = ff_vvc_sao_band_filter_8_10_avx2;
                break;
            case 12:
                ALF_INIT(12);
                AVG_INIT(12, avx2);
                break;
            default:
                break;
        }
    }
    if (EXTERNAL_AVX2_FAST(cpu_flags)) {
        switch (bd) {
            case 8:
                SAO_INIT(8, avx2);
                break;
            case 10:
                SAO_INIT(10, avx2);
                break;
            case 12:
                SAO_INIT(12, avx2);
            default:
                break;
        }
    }
}
