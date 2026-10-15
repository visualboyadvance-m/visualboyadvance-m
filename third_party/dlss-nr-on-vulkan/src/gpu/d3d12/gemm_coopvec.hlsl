/*
 * gemm_coopvec — the resident GEMM on Direct3D 12's vector-matrix multiply: Shader Model
 * 6.10's linear algebra (`dx/linalg.h`), a *thread-scope* matrix times a per-thread vector,
 * which is what Direct3D calls a cooperative vector. The one GEMM-family operation HLSL has
 * that reaches a device's matrix units from a single thread, and the first matrix path on
 * this runtime (libd3dmx.c, notes/phase77).
 *
 * A thread owns one row of C and a column slice of it: it loads its row of A as a long
 * vector, loads the slice of B it faces as a thread-scope matrix straight from the weight
 * buffer — B's own row-major (K, N) layout read column-major, or a transposed (N, K) B read
 * row-major, so no copy and no conversion — and asks the device for the product. K is taken
 * 128 deep at a time (the thread-scope matrix's largest K; then 64, 32, 16 for what is left,
 * every K here being a multiple of 16), each product a vector summed into the thread's
 * float32 accumulator. N is taken 64 wide at a time, then 32, then 16. The same push block,
 * flags, strides, batch on SV_GroupID.z, epilogues and stores as `gemm_portable.hlsl`:
 *
 *   bit 0     B stored (N, K)              0x1f00     epilogue (bits 8-11) and a half output
 *   0x10000   the compact 32 -> 4 head      0x20000    the residual `+ skip * cosine` before the
 *   0x40000   a half skip                             publish, 0x80000 written back through the
 *   0x100000  the QKV epilogue (0x2000 the ViT's)     window layout into the unpadded image
 *   0x200000  a published half copy into d  0x400000   A gathered from the image in window
 *   0x4000    ... published on the way in              order (0x8000 a half image)
 *
 * Four builds of this file: the plain row loads, and with -DGATHER the window-gathered A
 * (0x400000; libd3dmx dispatches that build for it) — kept apart because the gather unrolls
 * a conversion per element for every slice shape; each with -DRESULT16, where the device
 * offers the fp16 x fp16 product as a half vector only, summed into float32 just the same.
 *
 * libd3dmx routes a GEMM here only when every matrix slice it would load starts on a
 * 128-byte boundary and the stride is 16-byte aligned (the thread-scope matrix's contract);
 * the pooled window residual (0x800000) and whatever does not fit stay on the portable
 * kernels. The group is 32 rows (`SV_GroupID.y`, in pieces through `spare`) by one column
 * slice (`SV_GroupID.x`); a row past M returns.
 *
 * The numerics are the device's: the API fixes the inputs (half) and the result type, and
 * leaves the order and the width of the accumulation inside one product to the
 * implementation. So this kernel is *not* bit-identical to the multiply-add kernels, as the
 * cooperative-matrix path on RDNA3 is not (HANDOFF, 2026-10-12); the epilogues after the
 * product are the same code as everywhere else on this runtime.
 */
#include "nr_d3d.hlsli"
#include <dx/linalg.h>
#include "nr_epilogue.hlsli"

using namespace dx::linalg;

/* The weight matrix again, as the shader resource a thread-scope matrix must be loaded
 * from (a thread-scope `Matrix::Load` takes a ByteAddressBuffer, never an RW one): the
 * runtime binds the operand's resource to root parameter 9 (t0) as well, and transitions it
 * to the non-pixel shader resource state for this pass. */
ByteAddressBuffer matB : register(t0);

static const uint ROWS = 32;              /* rows a group */

#ifdef RESULT16
typedef half PRODUCT;
#else
typedef float PRODUCT;
#endif

/* Four consecutive elements to c, half or float, as one wide store when aligned
 * (gemm_portable.hlsl). */
void store4(uint C, uint at, float4 v, bool narrow) {
    if (narrow) {
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

void store_half4(RWByteAddressBuffer target, uint base, uint at, float4 v) {
    if ((base & 7u) == 0u) st_f16x4(target, base + at * 2u, v);
    else [unroll] for (uint i = 0u; i < 4u; i++) st_f16(target, base, at + i, v[i]);
}

/* What a thread's row of A is: a plain row at `abase` (elements, lda folded in), or a
 * window block's row gathered from the image (-DGATHER, 0x400000), zero outside it. */
struct RowA {
    uint abase;
    bool inside, image_half, image_publish;
};

/* Elements k0 .. k0 + KC of the thread's row, as the half vector the product takes. */
template <int KC>
vector<half, KC> row_chunk(RowA a, uint k0) {
    vector<half, KC> av;
#ifndef GATHER
    av = bufA.template Load<vector<half, KC> >(pc.oa.x + (a.abase + k0) * 2u);
#else
    /* the partition's load, four at a time (a row of the image starts 32 bytes aligned, as
     * libd3dmx checks): zero outside the image, a float32 image rounded to half on the way
     * in, or published to E4M3 with 0x4000 */
    [unroll] for (uint i = 0; i < KC; i += 4u) {
        float4 x = (float4)0.0;
        if (a.inside)
            x = a.image_half ? ld_f16x4(bufA, pc.oa.x + (a.abase + k0 + i) * 2u)
                             : asfloat(bufA.Load4(pc.oa.x + (a.abase + k0 + i) * 4u));
        [unroll] for (uint q = 0; q < 4u; q++)
            av[i + q] = (half)(a.image_publish ? e4m3(x[q]) : half_round(x[q]));
    }
#endif
    return av;
}

/* One product: K elements k0..k0+KC of the thread's row times the NC x KC slice of B at
 * (k0, n0), summed into `acc`. B row-major (K, N): the slice's element (m, k) is
 * B[(k0 + k) * ldb + n0 + m], a column-major NC x KC matrix with the row stride as its
 * column stride. Transposed, B is (N, K) and the slice is row-major from (n0, k0). */
template <int NC, int KC>
void multiply_chunk(inout vector<float, NC> acc, RowA a, uint k0, uint B, uint bo,
                    uint ldb, uint n0, bool transposed) {
    vector<half, KC> av = row_chunk<KC>(a, k0);
    uint start = B + (bo + (transposed ? n0 * ldb + k0 : k0 * ldb + n0)) * 2u;
    uint stride = ldb * 2u;
    vector<PRODUCT, NC> r;
    if (transposed)
        r = Multiply<PRODUCT>(
            Matrix<ComponentType::F16, NC, KC, MatrixUse::A, MatrixScope::Thread>
                ::template Load<MatrixLayout::RowMajor>(matB, start, stride), av);
    else
        r = Multiply<PRODUCT>(
            Matrix<ComponentType::F16, NC, KC, MatrixUse::A, MatrixScope::Thread>
                ::template Load<MatrixLayout::ColMajor>(matB, start, stride), av);
    acc += (vector<float, NC>)r;
}

/* Bit 0x100000: Q and K normalised and V published, a head at a time, straight into the
 * (window, head, token, 32) targets — `qkv_epilogue` of nr_epilogue.hlsli on a thread's own
 * row rather than a staged block. A 64-wide slice may hold a head of Q and a head of K:
 * each 32-wide half decides its part for itself. */
template <int NC>
void qkv_rows(vector<float, NC> acc, uint row, uint n0) {
    uint channels = pc.n / 3u, tokens = pc.image_h, heads = pc.image_w;
    bool vit = (operation_flags() & 0x2000u) != 0u;
    uint window = row / tokens, token = row % tokens;
    [unroll] for (uint h = 0; h < NC / 32; h++) {
        uint col = n0 + h * 32u, part = col / channels, head = (col % channels) / 32u;
        float hv[32];
        [unroll] for (uint i = 0u; i < 32u; i++) hv[i] = half_round(acc[h * 32 + i]);
        if (part < 2u) {
            float reciprocal = vit ? vit_reciprocal(hv) : cosine_reciprocal(hv);
            float scale = part == 0u ? half_round(ld_f32(bufF, pc.of.x, head)) : 1.0;
            [unroll] for (uint i = 0u; i < 32u; i++) {
                float value = hmul(hv[i], reciprocal);
                if (part == 0u) value = hmul(vit ? hmul(value, VIT_ROOT) : value, scale);
                hv[i] = value;                   // a half value: exact in float
            }
        }
        uint at = ((window * heads + head) * tokens + token) * 32u;
        [unroll] for (uint e = 0u; e < 32u; e += 4u) {
            float4 out4 = float4(e4m3(hv[e]), e4m3(hv[e + 1u]), e4m3(hv[e + 2u]), e4m3(hv[e + 3u]));
            if (part == 0u)      store_half4(bufC, pc.oc.x, at + e, out4);
            else if (part == 1u) store_half4(bufD, pc.od.x, at + e, out4);
            else                 store_half4(bufE, pc.oe.x, at + e, out4);
        }
    }
}

/* One row of C, columns n0 .. n0 + NC. */
template <int NC>
void run(uint row, uint n0, uint batch) {
    uint flags = operation_flags();
    bool transposed = (flags & 1u) != 0u;
    uint ao = batch * pc.sa, bo = batch * pc.sb, co = batch * pc.sc;
    uint lda = pc.lda != 0u ? pc.lda : pc.k;
    uint ldb = pc.ldb != 0u ? pc.ldb : (transposed ? pc.k : pc.n);
    uint ldc = pc.ldc != 0u ? pc.ldc : pc.n;
    uint B = pc.ob.x, C = pc.oc.x;

    RowA a;
    a.image_half = (flags & 0x8000u) != 0u;
    a.image_publish = (flags & 0x4000u) != 0u;
    a.inside = true;
#ifdef GATHER
    {
        /* gemm_portable.hlsl's gather: `sa` and `sb` the image's width and height, `sc`
         * the windows per row, `window_pad` (top << 16) | left; one batch */
        uint window = row / 64u, token = row % 64u;
        int y = int((window / pc.sc) * 8u + token / 8u) - int(pc.window_pad >> 16);
        int x = int((window % pc.sc) * 8u + token % 8u) - int(pc.window_pad & 0xffffu);
        a.inside = !(y < 0 || x < 0 || y >= int(pc.sb) || x >= int(pc.sa));
        a.abase = a.inside ? (uint(y) * pc.sa + uint(x)) * pc.k : 0u;
        bo = 0u;
    }
#else
    a.abase = ao + row * lda;
#endif

    vector<float, NC> acc = (vector<float, NC>)0.0;
    uint k0 = 0u;
    for (; pc.k - k0 >= 128u; k0 += 128u) multiply_chunk<NC, 128>(acc, a, k0, B, bo, ldb, n0, transposed);
    if (pc.k - k0 >= 64u) { multiply_chunk<NC, 64>(acc, a, k0, B, bo, ldb, n0, transposed); k0 += 64u; }
    if (pc.k - k0 >= 32u) { multiply_chunk<NC, 32>(acc, a, k0, B, bo, ldb, n0, transposed); k0 += 32u; }
    if (pc.k - k0 >= 16u) { multiply_chunk<NC, 16>(acc, a, k0, B, bo, ldb, n0, transposed); }

    if ((flags & 0x100000u) != 0u) { qkv_rows<NC>(acc, row, n0); return; }
    /* Bit 0x10000: the 32 -> 4 head writes only its four useful columns (ldc = 4) */
    if ((flags & 0x10000u) != 0u) {
        [unroll] for (uint q = 0; q < 4u; q++) st_f32(bufC, C, co + row * ldc + q, acc[q]);
        return;
    }
    uint epilogue = (flags >> 8) & 0xFu;
    bool narrow = (flags & 0x1000u) != 0u;
    [unroll] for (uint e = 0; e < NC; e += 4) {
        uint at = co + row * ldc + n0 + e;
        /* 0x80000 maps the window-ordered rows into the image and skips the padding; the
         * residual (0x20000) lands before the publish, as the two-pass path adds it */
        if (!residual_output_index(at)) continue;
        float4 v;
        [unroll] for (uint q = 0; q < 4u; q++)
            v[q] = publish(epilogue, add_gemm_residual(acc[e + q], at + q));
        store4(C, at, v, narrow);
        if ((flags & 0x200000u) != 0u) {
            uint D = pc.od.x;
            float4 p = float4(e4m3(v.x), e4m3(v.y), e4m3(v.z), e4m3(v.w));
            if ((at & 3u) == 0u && (D & 7u) == 0u) st_f16x4(bufD, D + at * 2u, p);
            else [unroll] for (uint i = 0; i < 4u; i++) st_f16(bufD, D, at + i, p[i]);
        }
    }
}

[numthreads(32, 1, 1)]
void main(uint3 gid : SV_GroupID, uint lid : SV_GroupIndex) {
    uint row = (gid.y + pc.spare) * ROWS + lid;
    if (row >= pc.m) return;
    /* The column slices, as libd3dmx counts them (coopvec_slices): whole 64s first, then a
     * 32 if 32 or more columns are left, then a 16 — so every slice but the last two starts
     * on a 64-column boundary, which is 128 bytes of a half row. */
    uint full = pc.n / 64u, rem = pc.n % 64u, j = gid.x;
    if (j < full)                       run<64>(row, j * 64u, gid.z);
    else if (j == full && rem >= 32u)   run<32>(row, full * 64u, gid.z);
    else if (j == full)                 run<16>(row, full * 64u, gid.z);
    else                                run<16>(row, full * 64u + 32u, gid.z);
}
