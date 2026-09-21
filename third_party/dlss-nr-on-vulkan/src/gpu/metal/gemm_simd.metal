/*
 * gemm_simd — the cooperative-matrix GEMMs on Apple's matrix path.
 *
 * Xe2 has `VK_KHR_cooperative_matrix` with one float shape, 8x16x16 fp16 -> fp32. Apple
 * silicon has `simdgroup_matrix`, one shape, 8x8, and `simdgroup_multiply_accumulate`
 * takes half operands into a float accumulator — the half x half product is exact in
 * float32, so the arithmetic claim is the one the XMX kernels make. An 8x16x16 tile is
 * therefore two 8x8 accumulators fed by two K slices: `TN / 8 * TK / 8` = four
 * multiply-accumulates where the GLSL has one `coopMatMulAdd`, and everything around
 * them — the flags, the strides, the batch on `threadgroup_position_in_grid.z`, the
 * epilogue and the narrow store through threadgroup memory (see the store below for why
 * not on the accumulator), the 8x16 / 16x32 / 64x32 block geometry — is `gemm_resident.comp`,
 * `gemm_staged.comp` and `gemm_coopmat*.comp` unchanged. The simdgroup is 32 wide, as
 * the subgroup is on Xe2; the runtime checks `threadExecutionWidth` and refuses otherwise.
 *
 * The transposed B (`flags & 1`, the key's own (N, K) layout) is the `transpose_matrix`
 * argument of `simdgroup_load`, where the SPIR-V uses a column-major `coopMatLoad`: no
 * transpose is copied on either.
 */
#include "nr_metal.h"

constant uint TM = 8, TN = 16, TK = 16;

/* One 8x16x16 step on 8x8 fragments: acc[jj] += A[kk] * B[kk][jj]. */
template <typename ACC>
inline void mma_tile(thread simdgroup_matrix<ACC, 8, 8> *acc,        /* [2]: the two column halves */
                     thread const simdgroup_half8x8 *a,               /* [2]: the two K slices */
                     thread const simdgroup_half8x8 *b) {             /* [4]: [kk * 2 + jj] */
    for (int kk = 0; kk < 2; kk++)
        for (int jj = 0; jj < 2; jj++)
            simdgroup_multiply_accumulate(acc[jj], a[kk], b[kk * 2 + jj], acc[jj]);
}

inline void load_a_tile(thread simdgroup_half8x8 *a, device const half *A, uint at, uint lda) {
    for (int kk = 0; kk < 2; kk++) simdgroup_load(a[kk], A + at + kk * 8, lda);
}
/* B row-major (K, N) at `at`, or (N, K) at `at` transposed on the way in. */
inline void load_b_tile(thread simdgroup_half8x8 *b, device const half *B, uint at, uint ldb, bool transposed) {
    for (int kk = 0; kk < 2; kk++)
        for (int jj = 0; jj < 2; jj++) {
            if (transposed) simdgroup_load(b[kk * 2 + jj], B + at + (jj * 8) * ldb + kk * 8, ldb, ulong2(0, 0), true);
            else            simdgroup_load(b[kk * 2 + jj], B + at + (kk * 8) * ldb + jj * 8, ldb);
        }
}

/* -- the resident GEMM: 8x16 (RM = RN = 1) and 16x32 (RM = RN = 2) blocks ------------- */

template <int RM, int RN>
kernel void gemm_resident_t(constant Push &pc [[buffer(0)]],
                            uint3 wg [[threadgroup_position_in_grid]],
                            uint lid [[thread_index_in_threadgroup]]) {
    const uint BM = TM * RM, BN = TN * RN;
    uint row = wg.y * BM, col = wg.x * BN;
    if (row >= pc.m || col >= pc.n) return;
    uint flags = operation_flags(pc);
    bool transposed = (flags & 1u) != 0u;
    uint batch = wg.z;
    uint ao = batch * pc.sa, bo = batch * pc.sb, co = batch * pc.sc;
    uint lda = pc.lda != 0u ? pc.lda : pc.k;
    uint ldb = pc.ldb != 0u ? pc.ldb : (transposed ? pc.k : pc.n);
    uint ldc = pc.ldc != 0u ? pc.ldc : pc.n;
    device const half *A = half_ptr(pc.a);
    device const half *B = half_ptr(pc.b);
    device float *C = float_out(pc.c);

    simdgroup_float8x8 acc[RM][RN][2];
    for (int i = 0; i < RM; i++)
        for (int j = 0; j < RN; j++)
            for (int jj = 0; jj < 2; jj++) acc[i][j][jj] = simdgroup_float8x8(0.0f);
    simdgroup_half8x8 a[RM][2];
    simdgroup_half8x8 b[RN][4];

    /* The transposed test stays outside the K loop, as in the GLSL, so the loads hoist. */
    if (transposed) {
        for (uint k = 0; k < pc.k; k += TK) {
            for (int i = 0; i < RM; i++) load_a_tile(a[i], A, ao + (row + i * TM) * lda + k, lda);
            for (int j = 0; j < RN; j++) load_b_tile(b[j], B, bo + (col + j * TN) * ldb + k, ldb, true);
            for (int i = 0; i < RM; i++)
                for (int j = 0; j < RN; j++) mma_tile(acc[i][j], a[i], b[j]);
        }
    } else {
        for (uint k = 0; k < pc.k; k += TK) {
            for (int i = 0; i < RM; i++) load_a_tile(a[i], A, ao + (row + i * TM) * lda + k, lda);
            for (int j = 0; j < RN; j++) load_b_tile(b[j], B, bo + k * ldb + col + j * TN, ldb, false);
            for (int i = 0; i < RM; i++)
                for (int j = 0; j < RN; j++) mma_tile(acc[i][j], a[i], b[j]);
        }
    }

    uint epilogue = (flags >> 8) & 0xFu;
    bool narrow = (flags & 0x1000u) != 0u;
    if (epilogue == 0u && !narrow) {
        for (int i = 0; i < RM; i++)
            for (int j = 0; j < RN; j++)
                for (int jj = 0; jj < 2; jj++)
                    simdgroup_store(acc[i][j][jj], C + co + (row + i * TM) * ldc + col + j * TN + jj * 8, ldc);
        return;
    }
    /* The publish runs on scalars, after an untouched store to threadgroup memory. The
     * GLSL applies it to the cooperative matrix's own components; the Metal equivalent,
     * `thread_elements()`, is a 64-wide vector whose hardware mapping the compiler hides,
     * and a per-element loop over it cost 19 ms a pass against 0.7 for the plain store —
     * 27x, and 80 % of a frame (notes/phase74). Through threadgroup memory the same pass
     * is 0.9 ms, and a narrow output gets its four-halves-a-lane store on the way. */
    threadgroup float stage[TM * RM * TN * RN];
    for (int i = 0; i < RM; i++)
        for (int j = 0; j < RN; j++)
            for (int jj = 0; jj < 2; jj++)
                simdgroup_store(acc[i][j][jj], stage + i * TM * BN + j * TN + jj * 8, BN);
    simdgroup_barrier(mem_flags::mem_threadgroup);

    device half *Ch = half_out(pc.c);
    for (uint e = lid * 4u; e < BM * BN; e += 32u * 4u) {
        float4 out4;
        for (uint q = 0; q < 4u; ++q) out4[q] = publish(epilogue, stage[e + q]);
        uint at = co + (row + e / BN) * ldc + col + e % BN;
        if (!narrow) {
            if ((at & 3u) == 0u && (pc.c & 15u) == 0u)
                reinterpret_cast<device float4 *>(C)[at >> 2] = out4;
            else
                for (uint q = 0; q < 4u; ++q) C[at + q] = out4[q];
        } else if ((at & 3u) == 0u && (pc.c & 7u) == 0u) {
            reinterpret_cast<device half4 *>(Ch)[at >> 2] = half4(out4);
        } else {
            for (uint q = 0; q < 4u; ++q) Ch[at + q] = half(out4[q]);
        }
    }
}

template [[host_name("gemm_resident")]] kernel void gemm_resident_t<1, 1>(constant Push &, uint3, uint);
template [[host_name("gemm_tiled")]]    kernel void gemm_resident_t<2, 2>(constant Push &, uint3, uint);

/* -- the staged GEMM: a 64x32 block, both operands through threadgroup memory ---------- */

constant uint S_WARPS = 4;
constant uint S_BM = 64, S_BN = 32, S_BK = 32;
constant uint S_WM = S_BM / S_WARPS, S_WN = S_BN;
constant uint S_RM = S_WM / TM, S_RN = S_WN / TN;
constant uint S_SA = S_BK + 8, S_SB = S_BN + 8;     /* eight halves of pad against bank conflicts */

kernel void gemm_staged(constant Push &pc [[buffer(0)]],
                        uint3 wg [[threadgroup_position_in_grid]],
                        uint thread_id [[thread_index_in_threadgroup]],
                        uint warp [[simdgroup_index_in_threadgroup]]) {
    threadgroup half buf_a[S_BM * S_SA];
    threadgroup half buf_b[S_BK * S_SB];
    threadgroup float stage[S_BM * S_BN];

    uint row = wg.y * S_BM, col = wg.x * S_BN;
    uint flags = operation_flags(pc);
    uint batch = wg.z;
    uint ao = batch * pc.sa, bo = batch * pc.sb, co = batch * pc.sc;
    uint lda = pc.lda != 0u ? pc.lda : pc.k;
    bool transposed = (flags & 1u) != 0u;
    uint ldb = pc.ldb != 0u ? pc.ldb : (transposed ? pc.k : pc.n);
    uint ldc = pc.ldc != 0u ? pc.ldc : pc.n;
    device const half *A = half_ptr(pc.a);
    device const half *B = half_ptr(pc.b);
    device float *C = float_out(pc.c);

    /* Four threads to a row of the tile, eight halves each. */
    uint lr = thread_id / 4u, lc = (thread_id % 4u) * 8u;

    simdgroup_float8x8 acc[S_RM][S_RN][2];
    for (uint i = 0; i < S_RM; i++)
        for (uint j = 0; j < S_RN; j++)
            for (int jj = 0; jj < 2; jj++) acc[i][j][jj] = simdgroup_float8x8(0.0f);
    simdgroup_half8x8 fa[S_RM][2];
    simdgroup_half8x8 fb[S_RN][4];

    /* Eight halves per thread as two 64-bit loads when the row stride allows it. */
    device const half4 *a4 = reinterpret_cast<device const half4 *>(A);
    device const half4 *b4 = reinterpret_cast<device const half4 *>(B);
    bool wide_a = (pc.a & 7u) == 0u && (lda & 3u) == 0u && (ao & 3u) == 0u;
    bool wide_b = (pc.b & 7u) == 0u && (ldb & 3u) == 0u && (bo & 3u) == 0u;

    for (uint k0 = 0; k0 < pc.k; k0 += S_BK) {
        threadgroup_barrier(mem_flags::mem_threadgroup);
        for (uint r = lr; r < S_BM; r += (32u * S_WARPS) / 4u) {
            uint at = ao + (row + r) * lda + k0 + lc;
            if (wide_a) {
                half4 lo = a4[at >> 2], hi = a4[(at >> 2) + 1u];
                for (uint e = 0; e < 4u; e++) buf_a[r * S_SA + lc + e] = lo[e];
                for (uint e = 0; e < 4u; e++) buf_a[r * S_SA + lc + 4u + e] = hi[e];
            } else {
                for (uint e = 0; e < 8u; e++) buf_a[r * S_SA + lc + e] = A[at + e];
            }
        }
        if (transposed) {
            for (uint nn = lr; nn < S_BN; nn += (32u * S_WARPS) / 4u) {
                uint at = bo + (col + nn) * ldb + k0 + lc;
                if (wide_b) {
                    half4 lo = b4[at >> 2], hi = b4[(at >> 2) + 1u];
                    for (uint e = 0; e < 4u; e++) buf_b[(lc + e) * S_SB + nn] = lo[e];
                    for (uint e = 0; e < 4u; e++) buf_b[(lc + 4u + e) * S_SB + nn] = hi[e];
                } else {
                    for (uint e = 0; e < 8u; e++) buf_b[(lc + e) * S_SB + nn] = B[at + e];
                }
            }
        } else {
            for (uint kk = lr; kk < S_BK; kk += (32u * S_WARPS) / 4u) {
                uint at = bo + (k0 + kk) * ldb + col + lc;
                if (wide_b) {
                    half4 lo = b4[at >> 2], hi = b4[(at >> 2) + 1u];
                    for (uint e = 0; e < 4u; e++) buf_b[kk * S_SB + lc + e] = lo[e];
                    for (uint e = 0; e < 4u; e++) buf_b[kk * S_SB + lc + 4u + e] = hi[e];
                } else {
                    for (uint e = 0; e < 8u; e++) buf_b[kk * S_SB + lc + e] = B[at + e];
                }
            }
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);

        for (uint kk = 0; kk < S_BK; kk += TK) {
            for (uint i = 0; i < S_RM; i++)
                for (int s = 0; s < 2; s++)
                    simdgroup_load(fa[i][s], buf_a + (warp * S_WM + i * TM) * S_SA + kk + s * 8, S_SA);
            for (uint j = 0; j < S_RN; j++)
                for (int s = 0; s < 2; s++)
                    for (int jj = 0; jj < 2; jj++)
                        simdgroup_load(fb[j][s * 2 + jj], buf_b + (kk + s * 8) * S_SB + j * TN + jj * 8, S_SB);
            for (uint i = 0; i < S_RM; i++)
                for (uint j = 0; j < S_RN; j++) mma_tile(acc[i][j], fa[i], fb[j]);
        }
    }

    uint epilogue = (flags >> 8) & 0xFu;
    bool narrow = (flags & 0x1000u) != 0u;
    if (epilogue == 0u && !narrow) {
        for (uint i = 0; i < S_RM; i++)
            for (uint j = 0; j < S_RN; j++)
                for (int jj = 0; jj < 2; jj++)
                    simdgroup_store(acc[i][j][jj],
                                    C + co + (row + warp * S_WM + i * TM) * ldc + col + j * TN + jj * 8, ldc);
        return;
    }
    /* The publish on scalars after an untouched store to threadgroup memory: the whole
     * workgroup, not one simdgroup, then covers the tile four elements a lane. */
    for (uint i = 0; i < S_RM; i++)
        for (uint j = 0; j < S_RN; j++)
            for (int jj = 0; jj < 2; jj++)
                simdgroup_store(acc[i][j][jj], stage + (warp * S_WM + i * TM) * S_BN + j * TN + jj * 8, S_BN);
    threadgroup_barrier(mem_flags::mem_threadgroup);

    device half *Ch = half_out(pc.c);
    for (uint e = thread_id * 4u; e < S_BM * S_BN; e += 32u * S_WARPS * 4u) {
        float4 out4;
        for (uint q = 0; q < 4u; ++q) out4[q] = publish(epilogue, stage[e + q]);
        uint at = co + (row + e / S_BN) * ldc + col + e % S_BN;
        if (!narrow) {
            for (uint q = 0; q < 4u; ++q) C[at + q] = out4[q];
        } else if ((at & 3u) == 0u && (pc.c & 7u) == 0u) {
            reinterpret_cast<device half4 *>(Ch)[at >> 2] = half4(out4);
        } else {
            for (uint q = 0; q < 4u; ++q) Ch[at + q] = half(out4[q]);
        }
    }
}

/* -- the descriptor-bound GEMMs: gemm_coopmat, gemm_batched, gemm_f16acc --------------- */

struct DescPush { uint M, N, K, sa, sb, sc, bt; };

/* ACC float is configuration 1 (fp16 x fp16 -> fp32); ACC half is configuration 0, what
 * NVIDIA's own kernels accumulate in (`gemm_coopmat_f16acc.comp`). */
template <typename ACC, bool BATCHED>
kernel void gemm_desc_t(device const half *A [[buffer(0)]],
                        device const half *B [[buffer(1)]],
                        device ACC *C [[buffer(2)]],
                        constant DescPush &pc [[buffer(3)]],
                        uint3 wg [[threadgroup_position_in_grid]]) {
    uint row = wg.y * TM, col = wg.x * TN;
    if (row >= pc.M || col >= pc.N) return;
    uint ao = 0u, bo = 0u, co = 0u;
    bool transposed = false;
    if (BATCHED) {
        uint batch = wg.z;
        ao = batch * pc.sa; bo = batch * pc.sb; co = batch * pc.sc;
        transposed = pc.bt != 0u;
    }
    simdgroup_matrix<ACC, 8, 8> acc[2];
    acc[0] = simdgroup_matrix<ACC, 8, 8>(ACC(0)); acc[1] = simdgroup_matrix<ACC, 8, 8>(ACC(0));
    simdgroup_half8x8 a[2], b[4];
    if (transposed) {
        for (uint k = 0; k < pc.K; k += TK) {
            load_a_tile(a, A, ao + row * pc.K + k, pc.K);
            load_b_tile(b, B, bo + col * pc.K + k, pc.K, true);
            mma_tile(acc, a, b);
        }
    } else {
        for (uint k = 0; k < pc.K; k += TK) {
            load_a_tile(a, A, ao + row * pc.K + k, pc.K);
            load_b_tile(b, B, bo + k * pc.N + col, pc.N, false);
            mma_tile(acc, a, b);
        }
    }
    simdgroup_store(acc[0], C + co + row * pc.N + col, pc.N);
    simdgroup_store(acc[1], C + co + row * pc.N + col + 8, pc.N);
}

template [[host_name("gemm_coopmat")]] kernel void gemm_desc_t<float, false>(device const half *, device const half *, device float *, constant DescPush &, uint3);
template [[host_name("gemm_batched")]] kernel void gemm_desc_t<float, true>(device const half *, device const half *, device float *, constant DescPush &, uint3);
template [[host_name("gemm_f16acc")]]  kernel void gemm_desc_t<half, false>(device const half *, device const half *, device half *, constant DescPush &, uint3);
