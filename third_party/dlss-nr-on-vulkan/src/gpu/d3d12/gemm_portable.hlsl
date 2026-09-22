/*
 * gemm_portable — the resident GEMM without a matrix unit, `gemm_portable.comp` in HLSL:
 * FP16 operands promoted to FP32 and accumulated with ordinary multiply-adds, one lane
 * owning four consecutive columns of one row of an 8x16 block per 32-lane group (16x32
 * with -DRM=2 -DRN=2, the `gemm_tiled` slot). Same push block, flags (bit 0 a transposed
 * B, bits 8-11 the epilogue, bit 12 a half output), strides, batch on SV_GroupID.z,
 * epilogue and store as the SPIR-V, so the runtime dispatches it with the geometry it uses
 * there. This is the only GEMM on Direct3D 12: HLSL has no shipped cooperative matrix
 * (see libd3dmx.c), so `gemm_resident`, `gemm_tiled` and `gemm_staged` all resolve here.
 */
#include "nr_d3d.hlsli"

#ifndef RM
#define RM 1
#endif
#ifndef RN
#define RN 1
#endif
static const uint TM = 8, TN = 16;
static const uint BM = TM * RM, BN = TN * RN;

[numthreads(32, 1, 1)]
void main(uint3 gid : SV_GroupID, uint lid : SV_GroupIndex) {
    uint row = (gid.y + pc.spare) * BM, col = gid.x * BN;
    if (row >= pc.m || col >= pc.n) return;
    uint flags = operation_flags();
    bool transposed = (flags & 1u) != 0u;
    uint batch = gid.z;
    uint ao = batch * pc.sa, bo = batch * pc.sb, co = batch * pc.sc;
    uint lda = pc.lda != 0u ? pc.lda : pc.k;
    uint ldb = pc.ldb != 0u ? pc.ldb : (transposed ? pc.k : pc.n);
    uint ldc = pc.ldc != 0u ? pc.ldc : pc.n;
    uint A = pc.oa.x, B = pc.ob.x, C = pc.oc.x;

    /* Lane l owns row l/4 of every 8-row tile and columns 4*(l%4)..+3 of every 16-column
     * tile: the four lanes of a row share its A element, and a lane's four B elements are
     * consecutive in a row-major B. */
    uint r = lid >> 2u, cq = (lid & 3u) * 4u;

    float4 acc[RM][RN];
    [unroll] for (uint i0 = 0; i0 < RM; i0++)
        [unroll] for (uint j0 = 0; j0 < RN; j0++)
            acc[i0][j0] = float4(0.0, 0.0, 0.0, 0.0);

    if (transposed) {
        /* B is stored (N, K): a lane's four columns are four rows of it. */
        for (uint k = 0; k < pc.k; k++) {
            float4 bv[RN];
            [unroll] for (uint j = 0; j < RN; j++) {
                uint at = bo + (col + cq + j * TN) * ldb + k;
                bv[j] = float4(ld_f16(bufB, B, at), ld_f16(bufB, B, at + ldb),
                               ld_f16(bufB, B, at + 2u * ldb), ld_f16(bufB, B, at + 3u * ldb));
            }
            [unroll] for (uint i = 0; i < RM; i++) {
                float av = ld_f16(bufA, A, ao + (row + r + i * TM) * lda + k);
                [unroll] for (uint j = 0; j < RN; j++)
                    acc[i][j] += av * bv[j];
            }
        }
    } else {
        for (uint k = 0; k < pc.k; k++) {
            float4 bv[RN];
            [unroll] for (uint j = 0; j < RN; j++) {
                uint at = bo + k * ldb + col + cq + j * TN;
                bv[j] = float4(ld_f16(bufB, B, at), ld_f16(bufB, B, at + 1u),
                               ld_f16(bufB, B, at + 2u), ld_f16(bufB, B, at + 3u));
            }
            [unroll] for (uint i = 0; i < RM; i++) {
                float av = ld_f16(bufA, A, ao + (row + r + i * TM) * lda + k);
                [unroll] for (uint j = 0; j < RN; j++)
                    acc[i][j] += av * bv[j];
            }
        }
    }

    uint epilogue = (flags >> 8) & 0xFu;
    bool narrow = (flags & 0x1000u) != 0u;
    [unroll] for (uint i = 0; i < RM; i++)
        [unroll] for (uint j = 0; j < RN; j++) {
            float4 v = acc[i][j];
            if (epilogue != 0u)
                [unroll] for (uint e = 0; e < 4u; e++)
                    v[e] = publish(epilogue, v[e]);
            uint at = co + (row + r + i * TM) * ldc + col + cq + j * TN;
            if (narrow) {
                /* Four consecutive halves per lane, as one 64-bit store when aligned. */
                if ((at & 3u) == 0u && (C & 7u) == 0u)
                    st_f16x4(bufC, C + at * 2u, v);
                else
                    [unroll] for (uint e = 0; e < 4u; e++)
                        st_f16(bufC, C, at + e, v[e]);
            } else {
                if ((at & 3u) == 0u && (C & 15u) == 0u)
                    st_f32x4(bufC, C + at * 4u, v);
                else
                    [unroll] for (uint e = 0; e < 4u; e++)
                        st_f32(bufC, C, at + e, v[e]);
            }
        }
}
