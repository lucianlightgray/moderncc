/*
 * mandelbrot — escape-time Mandelbrot set, row-parallel.
 *
 * Ported from The Computer Language Benchmarks Game mandelbrot problem
 * (vendor/plb/bench/algorithm/mandelbrot/). The plb C entry (1-ffi.c) pulls in
 * OpenSSL just to MD5 the output image; this self-contained port drops that and
 * emits a deterministic checksum of the 1-bit-per-pixel PBM bitmap instead.
 *
 * The image rows are data-independent, so each of NT C11 <threads.h> workers
 * fills a disjoint band of rows (fork-join); the checksum is folded over the
 * finished bitmap in row order after the join, so the printed value is identical
 * for any NT (serial -DNT=1 included).
 *
 * Work scale: image is N x N (N = argv[1], run.sh default 2000 → 4M pixels x 50
 * iterations), comparable in cost to the spectral-norm kernels at the same N.
 */
#include <threads.h>

extern void *malloc(__SIZE_TYPE__);
extern void free(void *);
extern int atoi(const char *);
extern int printf(const char *, ...);

#ifndef NT
#define NT 4
#endif

#define MAXITER 50

#ifdef MCC_GPU_OFFLOAD
/*
 * Heterogeneous CPU + multi-GPU path (T-lin-10526), scheduled by a dynamic work
 * queue rather than a fixed split. A single shared atomic row cursor hands out
 * chunks: each CPU worker (one per core) pulls a small chunk, each GPU worker
 * pulls a larger chunk (to amortize dispatch launch) and runs it on its device.
 * Whoever finishes a chunk grabs the next, so throughput differences AND the
 * non-uniform per-row cost self-balance with no calibration -- a fast executor
 * simply does more chunks. The GPU kernel (mandelbrot_gpu.comp -> embedded
 * SPIR-V) mirrors mbrot_worker in precise double, so every chunk is bit-exact
 * and the checksum matches the serial reference. Notably, this kernel uses
 * double for that parity, and consumer GPUs run fp64 far slower than fp32, so
 * the many-core CPU is typically the fastest executor here.
 * Tunables (env): MCC_GPU_MAXDEV (cap GPUs), MCC_GPU_CHUNK / MCC_CPU_CHUNK
 * (rows per pull), MCC_CPU_THREADS (CPU worker count), MCC_GPU_VERBOSE.
 */
#include "mcc_gpu_offload.h"
#include "mandelbrot_gpu_spv.h"
#include <unistd.h> /* sysconf(_SC_NPROCESSORS_ONLN): use all CPU cores */
#include <time.h>   /* clock_gettime: time the compute region only */

extern int dprintf(int, const char *, ...); /* stderr: MCCGPU_MS + verbose split */

#ifndef MCC_GPU_MAX
#define MCC_GPU_MAX 8
#endif

static double mcc_now(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

struct mbrot_gpc {
	unsigned w, h, rowbytes, row_lo, row_hi;
};
#endif

struct band {
	unsigned char *bits;
	int w, h;
	int rowbytes;
	int lo, hi;
};

static int mbrot_worker(void *arg) {
	struct band *b = arg;
	int w = b->w, h = b->h, rowbytes = b->rowbytes;
	for (int y = b->lo; y < b->hi; y++) {
		double ci = 2.0 * y / h - 1.0;
		unsigned char *row = b->bits + (long)y * rowbytes;
		for (int x = 0; x < w; x++) {
			double cr = 2.0 * x / w - 1.5;
			double zr = 0, zi = 0;
			int i = 0;
			while (i < MAXITER && zr * zr + zi * zi <= 4.0) {
				double t = zr * zr - zi * zi + cr;
				zi = 2.0 * zr * zi + ci;
				zr = t;
				i++;
			}
			if (zr * zr + zi * zi <= 4.0)
				row[x >> 3] |= (unsigned char)(0x80 >> (x & 7));
		}
	}
	return 0;
}

#ifdef MCC_GPU_OFFLOAD
/*
 * One executor pulling from the shared cursor. g==NULL is a CPU worker; else a
 * GPU worker running chunks on device g into scratch gbuf (chunk*rowbytes uints).
 */
struct mcc_work {
	unsigned char *bits;
	int w, h, rowbytes, chunk;
	int *next;      /* shared atomic row cursor */
	mcc_gpu *g;     /* NULL => CPU */
	unsigned *gbuf; /* GPU chunk scratch */
	long done;      /* rows this worker computed (for MCC_GPU_VERBOSE) */
};

static int mcc_work_run(void *arg) {
	struct mcc_work *wk = arg;
	for (;;) {
		int c = __atomic_fetch_add(wk->next, wk->chunk, __ATOMIC_RELAXED);
		int hi;
		if (c >= wk->h)
			break;
		hi = c + wk->chunk;
		if (hi > wk->h)
			hi = wk->h;
		if (wk->g) {
			struct mbrot_gpc p;
			unsigned long gb = (unsigned long)(hi - c) * wk->rowbytes, j;
			unsigned groups;
			p.w = wk->w;
			p.h = wk->h;
			p.rowbytes = wk->rowbytes;
			p.row_lo = (unsigned)c;
			p.row_hi = (unsigned)hi;
			groups = (unsigned)((gb + MCC_GPU_LOCAL_SIZE - 1) / MCC_GPU_LOCAL_SIZE);
			if (mcc_gpu_run(wk->g, &p, sizeof p, groups) == 0) {
				unsigned long base = (unsigned long)c * wk->rowbytes;
				for (j = 0; j < gb; j++)
					wk->bits[base + j] = (unsigned char)(wk->gbuf[j] & 0xff);
			} else { /* device failed mid-run: cover the chunk on this CPU thread */
				struct band fb;
				fb.bits = wk->bits;
				fb.w = wk->w;
				fb.h = wk->h;
				fb.rowbytes = wk->rowbytes;
				fb.lo = c;
				fb.hi = hi;
				mbrot_worker(&fb);
			}
		} else {
			struct band b;
			b.bits = wk->bits;
			b.w = wk->w;
			b.h = wk->h;
			b.rowbytes = wk->rowbytes;
			b.lo = c;
			b.hi = hi;
			mbrot_worker(&b);
		}
		wk->done += hi - c;
	}
	return 0;
}
#endif

int main(int argc, char **argv) {
	int n = argc > 1 ? atoi(argv[1]) : 2000;
	if (n < 1)
		n = 1;
	int w = n, h = n;
	int rowbytes = (w + 7) / 8;
	long total = (long)rowbytes * h;
	unsigned char *bits = malloc(total);
	for (long i = 0; i < total; i++)
		bits[i] = 0;

#ifdef MCC_GPU_OFFLOAD
	{
		mcc_gpu gc[MCC_GPU_MAX];
		unsigned *gbuf[MCC_GPU_MAX];
		int ngpu = 0;
		int ncpu = (int)sysconf(_SC_NPROCESSORS_ONLN); /* all cores */
		int cursor = 0;
		int gpu_chunk, cpu_chunk, want, nw, wi, i;
		char *e;
		int verbose = getenv("MCC_GPU_VERBOSE") != 0;
		struct mcc_work *wk;
		thrd_t *wth;

		if ((e = getenv("MCC_CPU_THREADS"))) {
			int v = atoi(e);
			if (v > 0)
				ncpu = v;
		}
		if (ncpu < 1)
			ncpu = 1;

		/* Fine chunks so the shared cursor balances well; GPU chunk larger than
		   CPU chunk to amortize per-dispatch launch. */
		gpu_chunk = h / 32;
		if (gpu_chunk < 32) gpu_chunk = 32;
		if (gpu_chunk > 256) gpu_chunk = 256;
		if ((e = getenv("MCC_GPU_CHUNK"))) { int v = atoi(e); if (v > 0) gpu_chunk = v; }
		cpu_chunk = h / 1024;
		if (cpu_chunk < 1) cpu_chunk = 1;
		if (cpu_chunk > 8) cpu_chunk = 8;
		if ((e = getenv("MCC_CPU_CHUNK"))) { int v = atoi(e); if (v > 0) cpu_chunk = v; }

		want = mcc_gpu_num_devices();
		if ((e = getenv("MCC_GPU_MAXDEV"))) {
			int m = atoi(e);
			if (m >= 0 && m < want)
				want = m;
		}
		if (want > MCC_GPU_MAX)
			want = MCC_GPU_MAX;

		/* open each device and give it a chunk-sized buffer + pipeline */
		for (i = 0; i < want; i++) {
			mcc_gpu g;
			unsigned *bp;
			if (mcc_gpu_open_nth(&g, i))
				continue;
			bp = (unsigned *)mcc_gpu_buffer(&g, (unsigned long)gpu_chunk * rowbytes *
			                                        sizeof(unsigned));
			if (!bp || mcc_gpu_pipeline(&g, mandelbrot_gpu_spv, sizeof mandelbrot_gpu_spv,
			                            sizeof(struct mbrot_gpc))) {
				mcc_gpu_close(&g);
				continue;
			}
			gc[ngpu] = g;
			gbuf[ngpu] = bp;
			ngpu++;
		}

		/* one worker per GPU + ncpu CPU workers, all pulling the shared cursor */
		nw = ngpu + ncpu;
		wk = malloc((unsigned long)nw * sizeof *wk);
		wth = malloc((unsigned long)nw * sizeof *wth);
		for (wi = 0; wi < nw; wi++) {
			wk[wi].bits = bits;
			wk[wi].w = w;
			wk[wi].h = h;
			wk[wi].rowbytes = rowbytes;
			wk[wi].next = &cursor;
			wk[wi].done = 0;
			if (wi < ngpu) {
				wk[wi].g = &gc[wi];
				wk[wi].gbuf = gbuf[wi];
				wk[wi].chunk = gpu_chunk;
			} else {
				wk[wi].g = (mcc_gpu *)0;
				wk[wi].gbuf = (unsigned *)0;
				wk[wi].chunk = cpu_chunk;
			}
		}
		/*
		 * Warm each GPU with one throwaway dispatch so the timed region measures
		 * steady-state compute, not one-time first-use costs. Device/instance
		 * creation and pipeline compilation already happened above (untimed), and
		 * teardown happens below (untimed): in a long-running process the GPUs are
		 * initialized once, so only the compute is charged per run. The compute
		 * wall is reported as MCCGPU_MS on stderr (the harness reads it).
		 */
		{
			int wr = gpu_chunk < h ? gpu_chunk : h;
			unsigned long wb = (unsigned long)wr * rowbytes;
			unsigned wg = (unsigned)((wb + MCC_GPU_LOCAL_SIZE - 1) / MCC_GPU_LOCAL_SIZE);
			struct mbrot_gpc wp;
			wp.w = w;
			wp.h = h;
			wp.rowbytes = rowbytes;
			wp.row_lo = 0;
			wp.row_hi = (unsigned)wr;
			for (i = 0; i < ngpu; i++)
				mcc_gpu_run(&gc[i], &wp, sizeof wp, wg);
		}
		{
			double t0 = mcc_now(), t1;
			for (wi = 0; wi < nw; wi++)
				thrd_create(&wth[wi], mcc_work_run, &wk[wi]);
			for (wi = 0; wi < nw; wi++)
				thrd_join(wth[wi], 0);
			t1 = mcc_now();
			dprintf(2, "MCCGPU_MS=%.3f\n", (t1 - t0) * 1000.0);
		}

		if (verbose) {
			long cpu_done = 0;
			for (i = 0; i < ngpu; i++)
				dprintf(2, "  gpu[%d] %ld rows (chunk %d) on %s\n", i, wk[i].done,
				        gpu_chunk, gc[i].name);
			for (i = ngpu; i < nw; i++)
				cpu_done += wk[i].done;
			dprintf(2, "  cpu    %ld rows (chunk %d) on %d thread(s)\n", cpu_done,
			        cpu_chunk, ncpu);
		}
		for (i = 0; i < ngpu; i++)
			mcc_gpu_close(&gc[i]);
		free(wk);
		free(wth);
	}
#else
	struct band bands[NT];
	thrd_t th[NT];
	int nt = NT < h ? NT : h;
	if (nt < 1)
		nt = 1;
	int per = (h + nt - 1) / nt;
	for (int k = 0; k < nt; k++) {
		bands[k].bits = bits;
		bands[k].w = w;
		bands[k].h = h;
		bands[k].rowbytes = rowbytes;
		bands[k].lo = k * per;
		bands[k].hi = (k + 1) * per < h ? (k + 1) * per : h;
	}
	for (int k = 0; k < nt; k++)
		thrd_create(&th[k], mbrot_worker, &bands[k]);
	for (int k = 0; k < nt; k++)
		thrd_join(th[k], 0);
#endif

	unsigned long long sum = 1469598103934665603ULL;
	unsigned long long setbits = 0;
	for (long i = 0; i < total; i++) {
		unsigned char c = bits[i];
		sum = (sum ^ c) * 1099511628211ULL;
		for (int bt = 0; bt < 8; bt++)
			setbits += (c >> bt) & 1u;
	}
	free(bits);
	printf("mandelbrot %dx%d checksum=%llu setbits=%llu\n", w, h, sum, setbits);
	return 0;
}
