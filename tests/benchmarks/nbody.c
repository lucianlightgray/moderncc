/*
 * nbody — classic 5-body solar-system simulation (Sun + 4 gas giants).
 *
 * Ported from The Computer Language Benchmarks Game entry
 * vendor/plb/bench/algorithm/nbody/2.c (contributed by Christoph Bauer, sped up
 * by Petr Prokhorenkov). The physics is inherently sequential — each step's
 * pairwise force accumulation reads the state the previous step wrote — so this
 * port stays single-threaded. <threads.h> is still included and the NT macro is
 * still honored (the runner builds a -DNT=1 serial reference of every kernel),
 * but no threads are spawned: parallelism here would be invented, not ported.
 *
 * As in the original, bodies are rescaled by DT so each advance() runs as if
 * dt==1.0, then rescaled back before the final energy is read. Output is the
 * standard CLBG two-doubles form ("%.9f"): initial energy then final energy.
 *
 * Work scale: N = argv[1] (default 2000); the simulation runs N*1000 steps
 * (default 2,000,000), chosen so runtime is comparable to the spectral-norm
 * kernels at the suite's default N.
 *
 * <threads.h> leads and libc is hand-declared rather than #included: mcc's
 * cooperative backend (-DMCC_THREADS_COOP) defines its own once_flag, which
 * collides with the one glibc's <stdlib.h> pulls in. Declaring the few
 * functions we use keeps ONE source building under native, coop, gcc and clang.
 */
#include <threads.h>

extern int atoi(const char *);
extern int printf(const char *, ...);
extern double sqrt(double);

#ifndef NT
#define NT 4
#endif

#define PI 3.141592653589793
#define SOLAR_MASS (4 * PI * PI)
#define DAYS_PER_YEAR 365.24
#define NBODIES 5
#define DT 1e-2
#define RECIP_DT (1.0 / DT)

struct planet {
	double x, y, z;
	double vx, vy, vz;
	double mass;
};

static struct planet bodies[NBODIES] = {
	{0, 0, 0, 0, 0, 0, SOLAR_MASS},
	{4.84143144246472090e+00, -1.16032004402742839e+00,
	 -1.03622044471123109e-01, 1.66007664274403694e-03 * DAYS_PER_YEAR,
	 7.69901118419740425e-03 * DAYS_PER_YEAR,
	 -6.90460016972063023e-05 * DAYS_PER_YEAR,
	 9.54791938424326609e-04 * SOLAR_MASS},
	{8.34336671824457987e+00, 4.12479856412430479e+00, -4.03523417114321381e-01,
	 -2.76742510726862411e-03 * DAYS_PER_YEAR,
	 4.99852801234917238e-03 * DAYS_PER_YEAR,
	 2.30417297573763929e-05 * DAYS_PER_YEAR,
	 2.85885980666130812e-04 * SOLAR_MASS},
	{1.28943695621391310e+01, -1.51111514016986312e+01,
	 -2.23307578892655734e-01, 2.96460137564761618e-03 * DAYS_PER_YEAR,
	 2.37847173959480950e-03 * DAYS_PER_YEAR,
	 -2.96589568540237556e-05 * DAYS_PER_YEAR,
	 4.36624404335156298e-05 * SOLAR_MASS},
	{1.53796971148509165e+01, -2.59193146099879641e+01, 1.79258772950371181e-01,
	 2.68067772490389322e-03 * DAYS_PER_YEAR,
	 1.62824170038242295e-03 * DAYS_PER_YEAR,
	 -9.51592254519715870e-05 * DAYS_PER_YEAR,
	 5.15138902046611451e-05 * SOLAR_MASS}};

static void advance(int nbodies, struct planet *b) {
	for (int i = 0; i < nbodies; i++) {
		struct planet *bi = &b[i];
		for (int j = i + 1; j < nbodies; j++) {
			struct planet *bj = &b[j];
			double dx = bi->x - bj->x;
			double dy = bi->y - bj->y;
			double dz = bi->z - bj->z;
			double inv = 1.0 / sqrt(dx * dx + dy * dy + dz * dz);
			double mag = inv * inv * inv;
			bi->vx -= dx * bj->mass * mag;
			bi->vy -= dy * bj->mass * mag;
			bi->vz -= dz * bj->mass * mag;
			bj->vx += dx * bi->mass * mag;
			bj->vy += dy * bi->mass * mag;
			bj->vz += dz * bi->mass * mag;
		}
	}
	for (int i = 0; i < nbodies; i++) {
		b[i].x += b[i].vx;
		b[i].y += b[i].vy;
		b[i].z += b[i].vz;
	}
}

static double energy(int nbodies, struct planet *b) {
	double e = 0.0;
	for (int i = 0; i < nbodies; i++) {
		struct planet *bi = &b[i];
		e += 0.5 * bi->mass *
		     (bi->vx * bi->vx + bi->vy * bi->vy + bi->vz * bi->vz);
		for (int j = i + 1; j < nbodies; j++) {
			struct planet *bj = &b[j];
			double dx = bi->x - bj->x;
			double dy = bi->y - bj->y;
			double dz = bi->z - bj->z;
			double d = sqrt(dx * dx + dy * dy + dz * dz);
			e -= (bi->mass * bj->mass) / d;
		}
	}
	return e;
}

static void offset_momentum(int nbodies, struct planet *b) {
	double px = 0.0, py = 0.0, pz = 0.0;
	for (int i = 0; i < nbodies; i++) {
		px += b[i].vx * b[i].mass;
		py += b[i].vy * b[i].mass;
		pz += b[i].vz * b[i].mass;
	}
	b[0].vx = -px / SOLAR_MASS;
	b[0].vy = -py / SOLAR_MASS;
	b[0].vz = -pz / SOLAR_MASS;
}

static void scale_bodies(int nbodies, struct planet *b, double scale) {
	for (int i = 0; i < nbodies; i++) {
		b[i].mass *= scale * scale;
		b[i].vx *= scale;
		b[i].vy *= scale;
		b[i].vz *= scale;
	}
}

int main(int argc, char **argv) {
	int n = argc > 1 ? atoi(argv[1]) : 2000;
	if (n < 1)
		n = 1;
	long steps = (long)n * 1000;

	offset_momentum(NBODIES, bodies);
	printf("%.9f\n", energy(NBODIES, bodies));
	scale_bodies(NBODIES, bodies, DT);
	for (long i = 0; i < steps; i++)
		advance(NBODIES, bodies);
	scale_bodies(NBODIES, bodies, RECIP_DT);
	printf("%.9f\n", energy(NBODIES, bodies));
	return 0;
}
