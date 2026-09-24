/*
 * window_attention — QK^T, the bit-affine softmax and PV for one 8x8 window in one
 * dispatch: `window_attention.comp` (and `window_attention_portable.comp`) in MSL.
 *
 * Each must equal, bit for bit, this runtime's own three-pass path on the same kernels:
 * `window_attention` the simdgroup GEMMs (8-wide K slices from 0 upwards, as gemm_simd
 * takes them, and the transposed key load of its B tile), `window_attention_portable` the
 * portable GEMMs (a float32 accumulator taking the K terms one at a time). The softmax
 * between them is the row pass's (`attention.metal`): the same coupled half-pair weights,
 * the float32 denominator in key order, `half_round(1 / half_round(total))`, and each
 * probability `e4m3(half_round(w * reciprocal))` held as half.
 *
 * Push: a = Q, b = K, d = V (half, (window, head, token, 32)), c = the output, n = heads,
 * batch = batches, flags = 1 with a bias at residual_cos (fp32, (head, query, key)).
 * Grid (8, batches up to 65535, the rest on z), 32 threads. Function constant 1 selects
 * the merged output: E4M3 halves in (window, token, C) order, merge_heads' work too.
 */
#include "nr_epilogue.h"

constant bool MERGED_CONSTANT [[function_constant(1)]];
constant bool merged_output = is_function_constant_defined(MERGED_CONSTANT) && MERGED_CONSTANT;

inline float2 wa_weights(constant Push &pc, threadgroup const float *scores, uint base, uint bias, uint i) {
    float2 affine;
    for (uint j = 0u; j < 2u; j++) {
        float logit = scores[base + i + j];
        if (pc.flags != 0u) logit += float_ptr(pc.residual_cos)[bias + i + j];
        float scaled = half_round(logit) * 0.044921875f;
        scaled += 1.30078125f;
        affine[j] = clamp(scaled, 1.03125f, 1.5693359375f);
    }
    uint transformed = (as_type<uint>(half2(affine)) << 5) + 0x7FF88000u;
    return float2(as_type<half2>(transformed));
}

/* The softmax on the staged logits, in place: weights, the per-row reciprocal, the
 * probabilities (half values, kept as float). Shared by both kernels. */
inline void wa_softmax(constant Push &pc, threadgroup float *scores, threadgroup float *reciprocal,
                       uint lid, uint head_bias) {
    for (uint at = lid * 2u; at < 8u * 64u; at += 64u) {
        uint rr = at / 64u, key = at % 64u;
        float2 w = wa_weights(pc, scores, rr * 64u, head_bias + rr * 64u, key);
        scores[at] = w.x;
        scores[at + 1u] = w.y;
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if (lid < 8u) {
        float total = 0.0f;
        for (uint i = 0u; i < 64u; i += 2u) {
            total += scores[lid * 64u + i];
            total += scores[lid * 64u + i + 1u];
        }
        reciprocal[lid] = half_round(1.0f / half_round(total));
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    for (uint at = lid; at < 8u * 64u; at += 32u)
        scores[at] = e4m3(hmul(scores[at], reciprocal[at / 64u]));
    threadgroup_barrier(mem_flags::mem_threadgroup);
}

inline uint merged_target(constant Push &pc, uint batch, uint row, uint r, uint c) {
    return ((batch / pc.n) * 64u + row + r) * pc.n * 32u + (batch % pc.n) * 32u + c;
}

kernel void window_attention(constant Push &pc [[buffer(0)]],
                             uint3 wg [[threadgroup_position_in_grid]],
                             uint lid [[thread_index_in_threadgroup]]) {
    threadgroup float scores[8 * 64];
    threadgroup half probs[8 * 64];
    threadgroup float reciprocal[8];
    uint batch = wg.y + wg.z * 65535u;
    if (batch >= pc.batch) return;
    uint row = wg.x * 8u, offset = batch * 2048u;
    uint head_bias = (batch % pc.n) * 4096u + row * 64u;
    device const half *Q = half_ptr(pc.a) + offset;
    device const half *K = half_ptr(pc.b) + offset;
    device const half *V = half_ptr(pc.d) + offset;

    simdgroup_float8x8 acc[8];
    for (int j = 0; j < 8; j++) acc[j] = simdgroup_float8x8(0.0f);
    for (uint c = 0u; c < 32u; c += 8u) {
        simdgroup_half8x8 q;
        simdgroup_load(q, Q + row * 32u + c, 32);
        for (uint j = 0u; j < 8u; j++) {
            simdgroup_half8x8 k;
            simdgroup_load(k, K + (j * 8u) * 32u + c, 32, ulong2(0, 0), true);
            simdgroup_multiply_accumulate(acc[j], q, k, acc[j]);
        }
    }
    for (uint j = 0u; j < 8u; j++) simdgroup_store(acc[j], scores + j * 8u, 64);
    threadgroup_barrier(mem_flags::mem_threadgroup);
    wa_softmax(pc, scores, reciprocal, lid, head_bias);
    for (uint at = lid; at < 8u * 64u; at += 32u) probs[at] = half(scores[at]);
    threadgroup_barrier(mem_flags::mem_threadgroup);

    simdgroup_float8x8 out[4];
    for (int j = 0; j < 4; j++) out[j] = simdgroup_float8x8(0.0f);
    for (uint c = 0u; c < 64u; c += 8u) {
        simdgroup_half8x8 p;
        simdgroup_load(p, probs + c, 64);
        for (uint j = 0u; j < 4u; j++) {
            simdgroup_half8x8 v;
            simdgroup_load(v, V + c * 32u + j * 8u, 32);
            simdgroup_multiply_accumulate(out[j], p, v, out[j]);
        }
    }
    if (!merged_output) {
        for (uint j = 0u; j < 4u; j++)
            simdgroup_store(out[j], float_out(pc.c) + offset + row * 32u + j * 8u, 32);
        return;
    }
    for (uint j = 0u; j < 4u; j++) simdgroup_store(out[j], scores + j * 8u, 32);
    threadgroup_barrier(mem_flags::mem_threadgroup);
    for (uint at = lid * 4u; at < 8u * 32u; at += 128u) {
        float4 values;
        for (uint q = 0u; q < 4u; q++) values[q] = e4m3(scores[at + q]);
        uint target = merged_target(pc, batch, row, at / 32u, at % 32u);
        reinterpret_cast<device half4 *>(half_out(pc.c))[target / 4u] = half4(values);
    }
}

kernel void window_attention_portable(constant Push &pc [[buffer(0)]],
                                      uint3 wg [[threadgroup_position_in_grid]],
                                      uint lid [[thread_index_in_threadgroup]]) {
    threadgroup float scores[8 * 64];
    threadgroup float reciprocal[8];
    uint batch = wg.y + wg.z * 65535u;
    if (batch >= pc.batch) return;
    uint row = wg.x * 8u, offset = batch * 2048u;
    uint head_bias = (batch % pc.n) * 4096u + row * 64u;
    device const half *Q = half_ptr(pc.a) + offset;
    device const half *K = half_ptr(pc.b) + offset;
    device const half *V = half_ptr(pc.d) + offset;
    /* the portable GEMM's lane: row r of the block, four consecutive columns cq.. */
    uint r = lid >> 2u, cq = (lid & 3u) * 4u;

    {   /* QK^T as the transposed-B portable GEMM: acc += q[row][c] * k[key][c], c in order */
        float4 acc[4];
        for (int j = 0; j < 4; j++) acc[j] = float4(0.0f);
        for (uint c = 0u; c < 32u; c++) {
            float4 kv[4];
            for (uint j = 0u; j < 4u; j++) {
                uint at = (cq + j * 16u) * 32u + c;
                kv[j] = float4(float(K[at]), float(K[at + 32u]), float(K[at + 64u]), float(K[at + 96u]));
            }
            float qv = float(Q[(row + r) * 32u + c]);
            for (uint j = 0u; j < 4u; j++) acc[j] += qv * kv[j];
        }
        for (uint j = 0u; j < 4u; j++)
            for (uint e = 0u; e < 4u; e++) scores[r * 64u + cq + j * 16u + e] = acc[j][e];
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    wa_softmax(pc, scores, reciprocal, lid, head_bias);

    /* PV as the plain portable GEMM: acc += p[row][key] * v[key][col], key in order */
    float4 acc[2];
    for (int j = 0; j < 2; j++) acc[j] = float4(0.0f);
    for (uint key = 0u; key < 64u; key++) {
        float4 vv[2];
        for (uint j = 0u; j < 2u; j++) {
            uint at = key * 32u + cq + j * 16u;
            vv[j] = float4(float(V[at]), float(V[at + 1u]), float(V[at + 2u]), float(V[at + 3u]));
        }
        float p = scores[r * 64u + key];
        for (uint j = 0u; j < 2u; j++) acc[j] += p * vv[j];
    }
    for (uint j = 0u; j < 2u; j++) {
        if (merged_output) {
            float4 values;
            for (uint q = 0u; q < 4u; q++) values[q] = e4m3(acc[j][q]);
            uint target = merged_target(pc, batch, row, r, cq + j * 16u);
            reinterpret_cast<device half4 *>(half_out(pc.c))[target / 4u] = half4(values);
        } else {
            for (uint q = 0u; q < 4u; q++)
                float_out(pc.c)[offset + (row + r) * 32u + cq + j * 16u + q] = acc[j][q];
        }
    }
}
