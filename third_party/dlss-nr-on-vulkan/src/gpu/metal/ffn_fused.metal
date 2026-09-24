/*
 * ffn_fused — a feed-forward in one pass, the hidden layer kept on chip:
 * `ffn_fused.comp` in MSL, simdgroup (`ffn_fused`) and portable (`ffn_fused_portable`).
 *
 * Bit-identical to this runtime's two GEMMs (the expand with the gate and E4M3 publish
 * into a half hidden buffer, then the projection with the residual in its epilogue, or
 * the branched blocks' two grouped GEMMs): each accumulator takes the same terms in the
 * same order the GEMM kernel of the same path gives it — ascending 8-wide K slices on
 * the simdgroup path (every simdgroup GEMM here, tiled or staged, goes up K that way),
 * one term at a time on the portable path — and the publishes and the residual are
 * nr_epilogue.h's.
 *
 * Push: a input (half, m x n), b expand (groups x n x k, group stride sa), qkv_scale slot
 * the projection (groups x k x 32, group stride sb), c output (row stride ldc, or 32;
 * group g at columns 32g), d skip and residual_cos cosine with 0x20000. Grid
 * (m/16, groups), 32 threads.
 */
#include "nr_epilogue.h"

constant uint F_ROWS = 16u, F_CHUNK = 32u, F_OUT = 32u;

/* The output block, raw on `stage` (16 x 32, row-major): the residual and the publish per
 * element, the store four at a time. */
inline void ffn_store(constant Push &pc, uint flags, threadgroup const float *stage,
                      uint row, uint group, uint ldc, uint lid) {
    uint epilogue = (flags >> 8) & 0xFu;
    bool narrow = (flags & 0x1000u) != 0u;
    for (uint e = lid * 4u; e < F_ROWS * F_OUT; e += 128u) {
        uint at = (row + e / F_OUT) * ldc + group * F_OUT + e % F_OUT;
        float4 out4;
        for (uint q = 0u; q < 4u; q++) out4[q] = publish(epilogue, add_gemm_residual(pc, flags, stage[e + q], at + q));
        if (!narrow) {
            for (uint q = 0u; q < 4u; q++) float_out(pc.c)[at + q] = out4[q];
        } else if ((at & 3u) == 0u && (pc.c & 7u) == 0u) {
            reinterpret_cast<device half4 *>(half_out(pc.c))[at >> 2] = half4(out4);
        } else {
            for (uint q = 0u; q < 4u; q++) half_out(pc.c)[at + q] = half(out4[q]);
        }
    }
}

kernel void ffn_fused(constant Push &pc [[buffer(0)]],
                      uint3 wg [[threadgroup_position_in_grid]],
                      uint lid [[thread_index_in_threadgroup]]) {
    threadgroup float stage[F_ROWS * F_CHUNK];
    threadgroup half hidden[F_ROWS * F_CHUNK];
    uint row = wg.x * F_ROWS, group = wg.y;
    if (row >= pc.m) return;
    uint flags = operation_flags(pc);
    uint cin = pc.n, width = pc.k, ldc = pc.ldc != 0u ? pc.ldc : F_OUT;
    device const half *A = half_ptr(pc.a);
    device const half *E = half_ptr(pc.b) + group * pc.sa;
    device const half *P = half_ptr(pc.qkv_scale) + group * pc.sb;

    simdgroup_float8x8 result[2][4];
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 4; j++) result[i][j] = simdgroup_float8x8(0.0f);
    for (uint chunk = 0u; chunk < width; chunk += F_CHUNK) {
        simdgroup_float8x8 h[2][4];
        for (int i = 0; i < 2; i++)
            for (int j = 0; j < 4; j++) h[i][j] = simdgroup_float8x8(0.0f);
        for (uint kk = 0u; kk < cin; kk += 8u) {
            simdgroup_half8x8 x[2], w[4];
            for (uint i = 0u; i < 2u; i++) simdgroup_load(x[i], A + (row + i * 8u) * cin + kk, cin);
            for (uint j = 0u; j < 4u; j++) simdgroup_load(w[j], E + kk * width + chunk + j * 8u, width);
            for (int i = 0; i < 2; i++)
                for (int j = 0; j < 4; j++) simdgroup_multiply_accumulate(h[i][j], x[i], w[j], h[i][j]);
        }
        for (uint i = 0u; i < 2u; i++)
            for (uint j = 0u; j < 4u; j++) simdgroup_store(h[i][j], stage + i * 8u * F_CHUNK + j * 8u, F_CHUNK);
        threadgroup_barrier(mem_flags::mem_threadgroup);
        for (uint e = lid; e < F_ROWS * F_CHUNK; e += 32u) hidden[e] = half(publish(3u, stage[e]));
        threadgroup_barrier(mem_flags::mem_threadgroup);
        for (uint kk = 0u; kk < F_CHUNK; kk += 8u) {
            simdgroup_half8x8 g[2], p[4];
            for (uint i = 0u; i < 2u; i++) simdgroup_load(g[i], hidden + i * 8u * F_CHUNK + kk, F_CHUNK);
            for (uint j = 0u; j < 4u; j++) simdgroup_load(p[j], P + (chunk + kk) * F_OUT + j * 8u, F_OUT);
            for (int i = 0; i < 2; i++)
                for (int j = 0; j < 4; j++) simdgroup_multiply_accumulate(result[i][j], g[i], p[j], result[i][j]);
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);   /* the next chunk overwrites both */
    }
    for (uint i = 0u; i < 2u; i++)
        for (uint j = 0u; j < 4u; j++) simdgroup_store(result[i][j], stage + i * 8u * F_OUT + j * 8u, F_OUT);
    threadgroup_barrier(mem_flags::mem_threadgroup);
    ffn_store(pc, flags, stage, row, group, ldc, lid);
}

kernel void ffn_fused_portable(constant Push &pc [[buffer(0)]],
                               uint3 wg [[threadgroup_position_in_grid]],
                               uint lid [[thread_index_in_threadgroup]]) {
    threadgroup float stage[F_ROWS * F_CHUNK];
    uint row = wg.x * F_ROWS, group = wg.y;
    if (row >= pc.m) return;
    uint flags = operation_flags(pc);
    uint cin = pc.n, width = pc.k, ldc = pc.ldc != 0u ? pc.ldc : F_OUT;
    device const half *A = half_ptr(pc.a);
    device const half *E = half_ptr(pc.b) + group * pc.sa;
    device const half *P = half_ptr(pc.qkv_scale) + group * pc.sb;
    /* the portable 16x32 GEMM's lane: rows r and r + 8, columns cq.. and 16 + cq.. */
    uint r = lid >> 2u, cq = (lid & 3u) * 4u;

    float4 result[2][2];
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++) result[i][j] = float4(0.0f);
    for (uint chunk = 0u; chunk < width; chunk += F_CHUNK) {
        float4 h[2][2];
        for (int i = 0; i < 2; i++)
            for (int j = 0; j < 2; j++) h[i][j] = float4(0.0f);
        for (uint k = 0u; k < cin; k++) {
            float4 bv[2];
            for (uint j = 0u; j < 2u; j++) {
                uint at = k * width + chunk + cq + j * 16u;
                bv[j] = float4(float(E[at]), float(E[at + 1u]), float(E[at + 2u]), float(E[at + 3u]));
            }
            for (uint i = 0u; i < 2u; i++) {
                float av = float(A[(row + r + i * 8u) * cin + k]);
                for (uint j = 0u; j < 2u; j++) h[i][j] += av * bv[j];
            }
        }
        for (uint i = 0u; i < 2u; i++)
            for (uint j = 0u; j < 2u; j++)
                for (uint e = 0u; e < 4u; e++)
                    stage[(r + i * 8u) * F_CHUNK + cq + j * 16u + e] = float(half(publish(3u, h[i][j][e])));
        threadgroup_barrier(mem_flags::mem_threadgroup);
        for (uint k = 0u; k < F_CHUNK; k++) {
            float4 bv[2];
            for (uint j = 0u; j < 2u; j++) {
                uint at = (chunk + k) * F_OUT + cq + j * 16u;
                bv[j] = float4(float(P[at]), float(P[at + 1u]), float(P[at + 2u]), float(P[at + 3u]));
            }
            for (uint i = 0u; i < 2u; i++) {
                float av = stage[(r + i * 8u) * F_CHUNK + k];
                for (uint j = 0u; j < 2u; j++) result[i][j] += av * bv[j];
            }
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
    for (uint i = 0u; i < 2u; i++)
        for (uint j = 0u; j < 2u; j++)
            for (uint e = 0u; e < 4u; e++)
                stage[(r + i * 8u) * F_OUT + cq + j * 16u + e] = result[i][j][e];
    threadgroup_barrier(mem_flags::mem_threadgroup);
    ffn_store(pc, flags, stage, row, group, ldc, lid);
}
