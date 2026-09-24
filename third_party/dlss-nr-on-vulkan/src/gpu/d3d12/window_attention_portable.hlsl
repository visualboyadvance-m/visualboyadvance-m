/*
 * window_attention_portable — `window_attention_portable.comp` in HLSL: QK^T, the
 * bit-affine softmax and PV for one 8x8 window in one dispatch, the scores and
 * probabilities in groupshared memory. -DMERGED_OUTPUT=1 is the build that also does the
 * head merge (the SPIR-V's specialisation constant 0; DXIL has none at run time).
 *
 * It must equal this backend's three-pass reference bit for bit — the batched
 * `gemm_portable.hlsl` for QK^T (transposed B), the softmax of `attention.hlsl`, the
 * batched `gemm_portable.hlsl` for PV — so the products are the portable GEMM's lane
 * layout and order (row r of the 8-row block, four consecutive columns cq, a float4
 * accumulator taking one K term at a time from zero, `acc += a * b`), and the weights,
 * the sum and the normalise are `attention.hlsl`'s expressions.
 *
 * Operands: u0 Q, u1 K, u3 V (half, (window, head, token, 32)); u2 the output, float32
 * context (batch, 64, 32) or, merged, half E4M3 in (window, token, C) order; u4 the bias
 * (float32, heads x 64 x 64) when pc.flags is 1. pc.n = heads, pc.batch = batches. The
 * batch rides on y and z as in the SPIR-V (y < 65535), 8 groups of 32 lanes on x.
 */
#include "nr_d3d.hlsli"

#ifndef MERGED_OUTPUT
#define MERGED_OUTPUT 0
#endif

groupshared float scores[8 * 64];
groupshared float reciprocals[8];

/* attention.hlsl's weights_at_fast, the logit read from the stage and the bias from u4 */
void weights(uint base, uint bias_base, uint i, out float w0, out float w1) {
    float affine[2];
    [unroll] for (uint j = 0u; j < 2u; j++) {
        float logit = scores[base + i + j];
        if (pc.flags != 0u) logit += ld_f32(bufE, pc.oe.x, bias_base + i + j);
        precise float scaled = half_round(logit) * 0.044921875;
        scaled += 1.30078125;
        affine[j] = clamp(scaled, 1.03125, 1.5693359375);
    }
    uint packed = f32tof16(affine[0]) | (f32tof16(affine[1]) << 16);
    uint transformed = (packed << 5) + 0x7FF88000u;
    w0 = f16tof32(transformed & 0xFFFFu);
    w1 = f16tof32(transformed >> 16);
}

[numthreads(32, 1, 1)]
void main(uint3 gid : SV_GroupID, uint local : SV_GroupIndex) {
    uint batch = gid.y + gid.z * 65535u;
    if (batch >= pc.batch) return;               // uniform across the group
    uint row = gid.x * 8u;
    uint offset = batch * 64u * 32u;
    uint head_bias = (batch % pc.n) * 4096u + row * 64u;
    uint r = local >> 2u, cq = (local & 3u) * 4u;
    uint Q = pc.oa.x, K = pc.ob.x, V = pc.od.x;

    /* QK^T as the transposed-B portable GEMM: acc[key] += q[row][c] * k[key][c], c in order */
    {
        float4 acc[4];
        [unroll] for (uint j0 = 0u; j0 < 4u; j0++) acc[j0] = float4(0.0, 0.0, 0.0, 0.0);
        uint qrow = offset + (row + r) * 32u;
        for (uint c = 0u; c < 32u; c++) {
            float4 kv[4];
            [unroll] for (uint j = 0u; j < 4u; j++) {
                uint at = offset + (cq + j * 16u) * 32u + c;
                kv[j] = float4(ld_f16(bufB, K, at), ld_f16(bufB, K, at + 32u),
                               ld_f16(bufB, K, at + 64u), ld_f16(bufB, K, at + 96u));
            }
            float qv = ld_f16(bufA, Q, qrow + c);
            [unroll] for (uint j1 = 0u; j1 < 4u; j1++)
                acc[j1] += qv * kv[j1];
        }
        [unroll] for (uint j2 = 0u; j2 < 4u; j2++)
            [unroll] for (uint e = 0u; e < 4u; e++)
                scores[r * 64u + cq + j2 * 16u + e] = acc[j2][e];
    }
    GroupMemoryBarrierWithGroupSync();
    /* every pair of keys becomes its weight in place, two adjacent keys a lane */
    for (uint at = local * 2u; at < 8u * 64u; at += 64u) {
        uint rr = at / 64u, key = at % 64u;
        float w0, w1;
        weights(rr * 64u, head_bias + rr * 64u, key, w0, w1);
        scores[at] = w0;                         // exact: a half value
        scores[at + 1u] = w1;
    }
    GroupMemoryBarrierWithGroupSync();
    /* the row's denominator in float32, in key order, as attention.hlsl sums it */
    if (local < 8u) {
        float total = 0.0;
        for (uint i = 0u; i < 64u; i += 2u) {
            total += scores[local * 64u + i];
            total += scores[local * 64u + i + 1u];
        }
        precise float reciprocal = 1.0 / half_round(total);
        reciprocals[local] = half_round(reciprocal);
    }
    GroupMemoryBarrierWithGroupSync();
    for (uint p = local; p < 8u * 64u; p += 32u) {
        precise float product = scores[p] * reciprocals[p / 64u];   // attention.hlsl's hmul
        scores[p] = e4m3(half_round(product));
    }
    GroupMemoryBarrierWithGroupSync();
    /* PV as the plain portable GEMM: acc[col] += p[row][key] * v[key][col], key in order */
    float4 ctx[2];
    [unroll] for (uint j3 = 0u; j3 < 2u; j3++) ctx[j3] = float4(0.0, 0.0, 0.0, 0.0);
    for (uint key = 0u; key < 64u; key++) {
        float4 vv[2];
        [unroll] for (uint j = 0u; j < 2u; j++) {
            uint at = offset + key * 32u + cq + j * 16u;
            vv[j] = float4(ld_f16(bufD, V, at), ld_f16(bufD, V, at + 1u),
                           ld_f16(bufD, V, at + 2u), ld_f16(bufD, V, at + 3u));
        }
        float pk = scores[r * 64u + key];
        [unroll] for (uint j4 = 0u; j4 < 2u; j4++)
            ctx[j4] += pk * vv[j4];
    }
    uint C = pc.oc.x;
    [unroll] for (uint j5 = 0u; j5 < 2u; j5++) {
#if MERGED_OUTPUT
        /* merge_heads(..., EPI_E4M3, narrow): E4M3 halves in (window, token, C) order */
        uint target = ((batch / pc.n) * 64u + row + r) * pc.n * 32u
                    + (batch % pc.n) * 32u + cq + j5 * 16u;
        float4 values;
        [unroll] for (uint e = 0u; e < 4u; e++) values[e] = e4m3(ctx[j5][e]);
        st_f16x4(bufC, C + target * 2u, values);
#else
        [unroll] for (uint e = 0u; e < 4u; e++)
            st_f32(bufC, C, offset + (row + r) * 32u + cq + j5 * 16u + e, ctx[j5][e]);
#endif
    }
}
