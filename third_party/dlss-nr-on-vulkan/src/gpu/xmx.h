/*
 * xmx.h — the public entry points of libxmx, the resident Vulkan compute runtime.
 *
 * The shared library needs no header: `nr_frame.c` and the Python bind its symbols by
 * name. This one is for the static build (`libdlssnr`, NR_STATIC_XMX), where nr_frame
 * calls these directly, and for checking the definitions in libxmx.c against one
 * declaration. Semantics are documented at the definitions.
 */
#ifndef XMX_H
#define XMX_H
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* device */
int xmx_open(void);
void xmx_close(void);
int xmx_adopt(void *instance, void *physical_device, void *device, void *queue, unsigned queue_family,
	      int cooperative_matrix, void *get_instance_proc_addr,
	      void (*lock)(void *), void (*unlock)(void *), void *lock_context);
int xmx_adopted(void);
int xmx_coopmat(void);
int xmx_portable(void);
int xmx_device_lost(void);
const char *xmx_error(void);
const char *xmx_device(void);
const char *xmx_path(void);
const char *xmx_memory(void);
int xmx_staging_mode(void);
size_t xmx_embedded_shader(const char *name);

/* the plain GEMM path */
int xmx_init(const char *spv_path);
int xmx_reserve(unsigned M, unsigned N, unsigned K, void **pa, void **pb, void **pc);
int xmx_gemm(unsigned M, unsigned N, unsigned K, const void *a, const void *b, void *c, unsigned iters);
int xmx_init_batched(const char *spv_path);
int xmx_reserve_bytes(unsigned long long a, unsigned long long b, unsigned long long c,
		      void **pa, void **pb, void **pc);
int xmx_gemm_batched(unsigned M, unsigned N, unsigned K, unsigned batch,
		     unsigned sa, unsigned sb, unsigned sc, unsigned bt);

/* the resident runtime */
int xmx_res_init(const char *gemm_spv, const char *unary_spv, const char *row_spv,
		 const char *history_spv, const char *tiled_spv, const char *staged_spv);
int xmx_specialize(unsigned mask);
unsigned xmx_specialized_count(void);
unsigned xmx_specialization(void);
int xmx_buf_create_kind(unsigned long long bytes, int kind);
int xmx_buf_create(unsigned long long bytes);
int xmx_buf_host_visible(int id);
int xmx_buf_zero(int id);
int xmx_buf_upload(int id, const void *src, unsigned long long offset, unsigned long long bytes);
int xmx_buf_download(int id, void *dst, unsigned long long offset, unsigned long long bytes);
unsigned long long xmx_buf_total_bytes(void);
void *xmx_buf_ptr(int id);
unsigned long long xmx_buf_bytes(int id);
int xmx_buf_destroy(int id);
int xmx_begin(void);
int xmx_abort(void);
int xmx_sync(int on);
int xmx_submit(void);
int xmx_rec_copy(int source, int target, unsigned long long bytes,
		 unsigned long long source_offset, unsigned long long target_offset);
int xmx_rec_gemm(int a, int b, int c, unsigned M, unsigned N, unsigned K, unsigned batch,
		 unsigned sa, unsigned sb, unsigned sc, unsigned bt,
		 unsigned lda, unsigned ldb, unsigned ldc,
		 unsigned oa, unsigned ob, unsigned oc);
int xmx_rec_unary(unsigned kind, int a, int b, int c, int d, unsigned n, unsigned channels,
		  float p0, unsigned batch, unsigned sa, unsigned sb, unsigned sc, unsigned k);
int xmx_rec_row(unsigned kind, int a, int b, int c, int d, unsigned rows, unsigned width,
		unsigned heads, unsigned scaled, unsigned stride, float cap);
int xmx_rec_history(int history, int motion, int out, unsigned pixels, unsigned channels,
		    unsigned height, unsigned width, unsigned absolute);
int xmx_graph_capture(void);
int xmx_graph_run(int id);
int xmx_graph_destroy(int id);

/* profiling */
int xmx_profile(int on);
void xmx_profile_reset(void);
double xmx_profile_ms(unsigned kind);
unsigned xmx_profile_count(unsigned kind);

#ifdef __cplusplus
}
#endif
#endif
