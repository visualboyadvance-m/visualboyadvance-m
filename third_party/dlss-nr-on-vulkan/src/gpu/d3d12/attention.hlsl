/*
 * attention — the row-wise operators, `attention.comp` in HLSL: the cosine publish and
 * the bit-affine softmax, one thread per row, the group's rows staged through group
 * shared memory so the global traffic is contiguous. -DSOFTMAX_AB is the `attention_ab`
 * build, whose bit-for-bit reference transform is test-only.
 *
 * Both reductions are reproduced in the order the reference uses: `sum(dtype=float16)`
 * in numpy runs sequentially in float32 and rounds once at the end, and the cosine
 * normalise uses a fixed fragment tree of half operations whose shape is the kernel's.
 * The staging is fenced with group-wide barriers, as the GLSL is since `phase73`.
 */
#include "nr_d3d.hlsli"
#include "nr_epilogue.hlsli"      /* hmul/hadd/hfma and cosine_reciprocal: one definition */

static const uint COSINE_PUBLISH = 0u, SOFTMAX = 1u, QKV_PREPARE = 2u;

groupshared float stage[32 * 64];

/* `count` consecutive floats from `base`, spread across the 32-wide group. */
void gather_part(uint flags, uint lid, uint base, uint count, uint part) {
    uint A = pc.oa.x;
    if ((flags & 0x20000u) != 0u) {
        // Q/K directly from (window, token, 3, head, channel). The cosine operation
        // performs the split's half rounding before its reduction.
        if (pc.n % 32u == 0u) {
            // A group's 32 rows stay in one head: compute its base once.
            uint row = base / 32u;
            uint head = (row / pc.n) % pc.batch, token = row % pc.n;
            uint window = row / (pc.n * pc.batch);
            uint at = ((window * pc.n + token) * 3u * pc.batch + part * pc.batch + head) * 32u;
            for (uint i = lid; i < count; i += 32u)
                stage[i] = ld_f32(bufA, A, at + (i / 32u) * pc.batch * 96u + i % 32u);
        } else {
            for (uint i = lid; i < count; i += 32u) {
                uint row = (base + i) / 32u, channel = (base + i) % 32u;
                uint head = (row / pc.n) % pc.batch, token = row % pc.n;
                uint window = row / (pc.n * pc.batch);
                uint at = ((window * pc.n + token) * 3u * pc.batch + part * pc.batch + head) * 32u + channel;
                stage[i] = ld_f32(bufA, A, at);
            }
        }
    } else if ((flags & 0x8000u) != 0u) {
        /* Bit 15: the input is float16. The cosine publish rounds its input to half as
         * its first act, so a producer that already wrote half loses nothing by it. */
        for (uint i = lid; i < count; i += 32u) stage[i] = ld_f16(bufA, A, base + i);
    } else {
        for (uint i = lid; i < count; i += 32u) stage[i] = ld_f32(bufA, A, base + i);
    }
    GroupMemoryBarrierWithGroupSync();
}

void gather(uint flags, uint lid, uint base, uint count) {
    gather_part(flags, lid, base, count, (flags >> 18) & 1u);
}

/* `stage` to one of the operand resources: 1 = b, 2 = c, 3 = d. The target is uniform
 * over the group, so each branch names one resource, as HLSL wants it. */
void scatter_rows(RWByteAddressBuffer target, uint offset, bool narrow, uint lid, uint count, uint base) {
    if (narrow) {
        for (uint i = lid; i < count; i += 32u) st_f16(target, offset, base + i, stage[i]);
    } else {
        for (uint i = lid; i < count; i += 32u) st_f32(target, offset, base + i, stage[i]);
    }
}

void scatter_to(uint flags, uint lid, uint base, uint count, uint which) {
    GroupMemoryBarrierWithGroupSync();
    bool narrow = (flags & 0x1000u) != 0u;
    if (which == 1u)      scatter_rows(bufB, pc.ob.x, narrow, lid, count, base);
    else if (which == 3u) scatter_rows(bufD, pc.od.x, narrow, lid, count, base);
    else                  scatter_rows(bufC, pc.oc.x, narrow, lid, count, base);
}

void scatter(uint flags, uint lid, uint base, uint count) { scatter_to(flags, lid, base, count, 2u); }

/* The publish this pass ends with: bits 8-11 of `flags` pick the transform and bit 12
 * narrows the output to float16. Both of these rows end in an E4M3 value, which is exact
 * in half, so narrowing here changes nothing. */
void store(uint flags, uint index, float value) {
    uint epilogue = (flags >> 8) & 0xFu;
    if (epilogue == 1u) value = e4m3(value);
    else if (epilogue == 4u) value = half_round(value);
    if ((flags & 0x1000u) != 0u) st_f16(bufC, pc.oc.x, index, value);
    else                         st_f32(bufC, pc.oc.x, index, value);
}

/* `scale_from`: 0 none, 1 the scale in d (the plain cosine publish, `pc.k != 0`), 2 the
 * scale at push offset 120 (QKV_PREPARE, where d is the V target). */
void cosine_publish(uint row, uint local, uint scale_from) {
    uint base = local * 32u;
    float h[32];
    [unroll] for (uint i = 0u; i < 32u; i++) h[i] = half_round(stage[base + i]);
    float reciprocal = cosine_reciprocal(h);          // nr_epilogue.hlsli, cosine_tree.glsl
    float scale = 1.0;
    uint head = (row / pc.n) % pc.batch;              // rows are (batch, head, token)
    if (scale_from == 1u) scale = half_round(ld_f32(bufD, pc.od.x, head));
    else if (scale_from == 2u) scale = half_round(ld_f32(bufF, pc.of.x, head));
    [unroll] for (uint i = 0u; i < 32u; i++) {
        float value = hmul(h[i], reciprocal);
        if (scale_from != 0u) value = hmul(value, scale);
        stage[base + i] = e4m3(value);
    }
}

/* One pair of attention weights. The vendor's exp is a bit-affine map on an `f16x2`
 * register, so the pair is coupled — the shift moves bits across the halves and the add
 * can carry between them — and the two elements cannot be computed apart
 * (notes/phase5-softmax-found.md). The clamped affine values encode as 0x3c20..0x3e47,
 * and for every pair the transform below yields finite normal half values, so the native
 * pack and unpack reproduce the bit reconstruction exactly, carry included. */
void weights_at_fast(uint flags, bool staged, uint base, uint bias, uint i, out float w0, out float w1) {
    float affine[2];
    [unroll] for (uint j = 0u; j < 2u; j++) {
        float logit = staged ? stage[base + i + j] : ld_f32(bufA, pc.oa.x, base + i + j);
        if ((flags & 0x2000u) != 0u) logit += ld_f32(bufB, pc.ob.x, bias + i + j);
        if (pc.p0 != 0.0) logit = clamp(logit, -pc.p0, pc.p0);
        precise float scaled = half_round(logit) * 0.044921875;
        scaled += 1.30078125;
        affine[j] = clamp(scaled, 1.03125, 1.5693359375);
    }
    uint packed = f32tof16(affine[0]) | (f32tof16(affine[1]) << 16);
    uint transformed = (packed << 5) + 0x7FF88000u;
    w0 = f16tof32(transformed & 0xFFFFu);
    w1 = f16tof32(transformed >> 16);
}

#ifdef SOFTMAX_AB
void weights_at_reference(uint flags, bool staged, uint base, uint bias, uint i, out float w0, out float w1) {
    uint bits[2];
    [unroll] for (uint j = 0u; j < 2u; j++) {
        float logit = staged ? stage[base + i + j] : ld_f32(bufA, pc.oa.x, base + i + j);
        /* The per-head attention bias, added here rather than by a pass of its own:
         * it is a few tens of kilobytes and stays in cache, so folding it in costs
         * nothing and saves a whole read-modify-write of the scores. */
        if ((flags & 0x2000u) != 0u) logit += ld_f32(bufB, pc.ob.x, bias + i + j);
        if (pc.p0 != 0.0) logit = clamp(logit, -pc.p0, pc.p0);
        precise float scaled = half_round(logit) * 0.044921875;
        scaled += 1.30078125;
        float affine = clamp(scaled, 1.03125, 1.5693359375);
        // the float16 bit pattern of the affine value
        int f = asint(half_round(affine));
        bits[j] = uint(((f >> 13) & 0x3FF) | (((((f >> 23) & 0xFF) - 112) & 0x1F) << 10)
                       | ((f < 0) ? 0x8000 : 0));
    }
    uint transformed = ((bits[0] | (bits[1] << 16)) << 5) + 0x7FF88000u;
    float out2[2];
    [unroll] for (uint j = 0u; j < 2u; j++) {
        uint half_bits = (j == 0u) ? (transformed & 0xFFFFu) : ((transformed >> 16) & 0xFFFFu);
        uint exponent = (half_bits >> 10) & 0x1Fu, mantissa = half_bits & 0x3FFu;
        float weight = (exponent == 0u)
            ? float(mantissa) * 5.9604644775390625e-08
            : asfloat(int(((exponent + 112u) << 23) | (mantissa << 13)));
        out2[j] = ((half_bits & 0x8000u) != 0u) ? -weight : weight;
    }
    w0 = out2[0]; w1 = out2[1];
}
#endif

void weights_at(uint flags, bool staged, uint base, uint bias, uint i, out float w0, out float w1) {
#ifdef SOFTMAX_AB
    // Test-only specialization bit; removed entirely from the shipped shader.
    if ((flags & 0x40000000u) != 0u) {
        weights_at_reference(flags, staged, base, bias, i, w0, w1);
        return;
    }
#endif
    weights_at_fast(flags, staged, base, bias, i, w0, w1);
}

void softmax(uint flags, uint row, uint local) {
    /* `n` is the real token count and `sa` the row stride, which differ for the
     * global blocks: their token count is the bottleneck's pixel count and need not
     * be a multiple of the GEMM tile, so the scores are padded. The pad must not enter
     * the sum, and is zeroed so the following P@V contributes nothing. `p0` is the
     * symmetric logit clamp the vit_1d kernels apply; 0 means none.
     *
     * The weights are computed twice rather than parked in the output buffer between
     * the sum and the normalise: that buffer is the one being narrowed, and reading a
     * half value back would round the total. */
    uint stride = pc.sa != 0u ? pc.sa : pc.n;
    bool staged = stride == pc.n && stride <= 64u;
    uint base = staged ? local * stride : row * stride;
    /* scores are (windows*heads, tokens, tokens) and the bias (heads, tokens, tokens) */
    uint bias = ((row / pc.n) % max(pc.batch, 1u)) * pc.n * pc.n + (row % pc.n) * pc.n;
    if (!staged) for (uint z = pc.n; z < stride; z++) store(flags, base + z, 0.0);
    float total = 0.0;
    float w0, w1;
    for (uint i = 0u; i < pc.n; i += 2u) {
        weights_at(flags, staged, base, bias, i, w0, w1);
        /* The weights are parked where the logits were, so the normalising pass does
         * not repeat the bit-affine transform. Only shared memory can hold them: the
         * output buffer is the one being narrowed to float16, and a rounded weight
         * would give a different product. The rows too wide to stage pay the transform
         * twice instead. */
        if (staged) { stage[base + i] = w0; stage[base + i + 1u] = w1; }
        total += w0;                              // float32 accumulation, as numpy does
        total += w1;
    }
    precise float reciprocal = 1.0 / half_round(total);
    reciprocal = half_round(reciprocal);
    if (staged) {
        for (uint i = 0u; i < pc.n; i++)
            stage[base + i] = e4m3(hmul(stage[base + i], reciprocal));
    } else {
        for (uint i = 0u; i < pc.n; i += 2u) {
            weights_at(flags, false, base, bias, i, w0, w1);
            store(flags, base + i,      e4m3(hmul(w0, reciprocal)));
            store(flags, base + i + 1u, e4m3(hmul(w1, reciprocal)));
        }
    }
}

[numthreads(32, 1, 1)]
void main(uint3 gid : SV_GroupID, uint lid : SV_GroupIndex) {
    uint group = gid.x + pc.spare;
    uint row = group * 32u + lid;
    uint flags = operation_flags();
    uint kind = flags & 0xFFu;
    if (kind == QKV_PREPARE) {
        /* attention.comp's QKV_PREPARE: three independent group planes on y — Q and K
         * normalised (Q scaled), V published — the arithmetic of the two cosine
         * publishes and the V split, recorded as one dispatch. Targets b, c, d. */
        uint part = gid.y;
        uint base = group * 32u * 32u;
        uint count = min(32u * 32u, pc.m * 32u - base);
        gather_part(flags, lid, base, count, part);
        if (part < 2u) {
            if (row < pc.m) cosine_publish(row, lid, part == 0u ? 2u : 0u);
        } else {
            for (uint i = lid; i < count; i += 32u) stage[i] = e4m3(stage[i]);
        }
        scatter_to(flags, lid, base, count, part + 1u);
    } else if (kind == COSINE_PUBLISH) {
        uint base = group * 32u * 32u;
        gather(flags, lid, base, min(32u * 32u, pc.m * 32u - base));
        if (row < pc.m) cosine_publish(row, lid, pc.k != 0u ? 1u : 0u);
        scatter(flags, lid, base, min(32u * 32u, pc.m * 32u - base));
    } else if (kind == SOFTMAX) {
        uint stride = pc.sa != 0u ? pc.sa : pc.n;
        if (stride == pc.n && stride <= 64u) {
            uint base = group * 32u * stride;
            uint count = min(32u * stride, pc.m * stride - base);
            gather(flags, lid, base, count);
            if (row < pc.m) softmax(flags, row, lid);
            scatter(flags, lid, base, count);
        } else {
            if (row < pc.m) softmax(flags, row, lid);
        }
    }
}
