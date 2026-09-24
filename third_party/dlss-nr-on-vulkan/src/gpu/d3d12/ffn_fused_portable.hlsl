/*
 * ffn_fused_portable — `ffn_fused_portable.comp` in HLSL: a feed-forward in one pass, the
 * gated hidden layer kept in groupshared memory, 32 hidden columns at a time.
 *
 * It must equal this backend's two-GEMM reference bit for bit: the expand through the
 * tiled `gemm_portable.hlsl` with the gate and E4M3 publish into a half hidden buffer,
 * then the projection through `gemm_portable.hlsl` with the residual in its epilogue. So
 * both products are the portable GEMM's lane layout and order (rows r and r+8 of a 16-row
 * block, four consecutive columns cq and cq+16, float4 accumulators taking one K term at a
 * time from zero, `acc += a * b`); the projection's accumulator runs across the hidden
 * chunks in order, the order the reference sums the whole hidden width in.
 *
 * Operands: u0 the input (half, m x n), u1 the expand weights (groups x n x k, group stride
 * sa), u5 the projection weights (groups x k x 32, group stride sb; push offset 120), u2
 * the output (float32, half with 0x1000; row stride ldc, 32 when 0; group g at columns
 * 32g), u3 the residual's skip and u4 its per-channel cosine (flag 0x20000; a half skip
 * with 0x40000). One 16-row block a group on x, split by libd3dmx with pc.spare; the
 * feed-forward group on y.
 */
#include "nr_d3d.hlsli"
#include "nr_epilogue.hlsli"

static const uint ROWS = 16u, CHUNK = 32u, OUT = 32u, TM = 8u, TN = 16u;
static const uint EPI_GATE_E4M3 = 3u;              // the expand GEMM's publish

/* one chunk of the gated hidden layer: half values, held exactly as floats */
groupshared float hidden[ROWS * CHUNK];

[numthreads(32, 1, 1)]
void main(uint3 gid : SV_GroupID, uint lane : SV_GroupIndex) {
    uint row = (gid.x + pc.spare) * ROWS, group = gid.y;
    if (row >= pc.m) return;
    uint cin = pc.n, width = pc.k;
    uint ldc = pc.ldc != 0u ? pc.ldc : OUT;
    uint expand_base = group * pc.sa, project_base = group * pc.sb;
    uint r = lane >> 2u, cq = (lane & 3u) * 4u;
    uint A = pc.oa.x, Bx = pc.ob.x, P = pc.of.x, C = pc.oc.x;

    float4 result[2][2];
    [unroll] for (uint i0 = 0; i0 < 2u; i0++)
        [unroll] for (uint j0 = 0; j0 < 2u; j0++)
            result[i0][j0] = float4(0.0, 0.0, 0.0, 0.0);

    for (uint chunk = 0u; chunk < width; chunk += CHUNK) {
        /* the expand's 16x32 block for these hidden columns, as the tiled portable GEMM */
        float4 h[2][2];
        [unroll] for (uint i1 = 0; i1 < 2u; i1++)
            [unroll] for (uint j1 = 0; j1 < 2u; j1++)
                h[i1][j1] = float4(0.0, 0.0, 0.0, 0.0);
        for (uint k = 0; k < cin; k++) {
            float4 bv[2];
            [unroll] for (uint j = 0; j < 2u; j++) {
                uint at = expand_base + k * width + chunk + cq + j * TN;
                bv[j] = float4(ld_f16(bufB, Bx, at), ld_f16(bufB, Bx, at + 1u),
                               ld_f16(bufB, Bx, at + 2u), ld_f16(bufB, Bx, at + 3u));
            }
            [unroll] for (uint i = 0; i < 2u; i++) {
                float av = ld_f16(bufA, A, (row + r + i * TM) * cin + k);
                [unroll] for (uint j2 = 0; j2 < 2u; j2++)
                    h[i][j2] += av * bv[j2];
            }
        }
        /* the expand's publish, stored as the half buffer the reference writes holds it */
        [unroll] for (uint i2 = 0; i2 < 2u; i2++)
            [unroll] for (uint j3 = 0; j3 < 2u; j3++)
                [unroll] for (uint e = 0; e < 4u; e++)
                    hidden[(r + i2 * TM) * CHUNK + cq + j3 * TN + e] =
                        half_round(publish(EPI_GATE_E4M3, h[i2][j3][e]));
        GroupMemoryBarrierWithGroupSync();
        /* the projection over these 32 hidden rows, continuing the accumulator */
        for (uint kk = 0; kk < CHUNK; kk++) {
            float4 pv[2];
            [unroll] for (uint j = 0; j < 2u; j++) {
                uint at = project_base + (chunk + kk) * OUT + cq + j * TN;
                pv[j] = float4(ld_f16(bufF, P, at), ld_f16(bufF, P, at + 1u),
                               ld_f16(bufF, P, at + 2u), ld_f16(bufF, P, at + 3u));
            }
            [unroll] for (uint i = 0; i < 2u; i++) {
                float av = hidden[(r + i * TM) * CHUNK + kk];
                [unroll] for (uint j4 = 0; j4 < 2u; j4++)
                    result[i][j4] += av * pv[j4];
            }
        }
        GroupMemoryBarrierWithGroupSync();       // the next chunk overwrites `hidden`
    }

    uint epilogue = (operation_flags() >> 8) & 0xFu;
    bool narrow_out = (operation_flags() & 0x1000u) != 0u;
    [unroll] for (uint i3 = 0; i3 < 2u; i3++)
        [unroll] for (uint j5 = 0; j5 < 2u; j5++) {
            float4 v = result[i3][j5];
            uint at = (row + r + i3 * TM) * ldc + group * OUT + cq + j5 * TN;
            [unroll] for (uint e = 0; e < 4u; e++)
                v[e] = publish(epilogue, add_gemm_residual(v[e], at + e));
            if (!narrow_out) {
                [unroll] for (uint q = 0; q < 4u; q++) st_f32(bufC, C, at + q, v[q]);
            } else if ((at & 3u) == 0u && (C & 7u) == 0u) {
                st_f16x4(bufC, C + at * 2u, v);
            } else {
                [unroll] for (uint q = 0; q < 4u; q++) st_f16(bufC, C, at + q, v[q]);
            }
        }
}
