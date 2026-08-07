#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define NB 12

static double x[NB], y[NB], z[NB], vx[NB], vy[NB], vz[NB], m[NB];

static void advance(double dt)
{
	int i, j;
	for (i = 0; i < NB; i++) {
		for (j = i + 1; j < NB; j++) {
			double dx = x[i] - x[j];
			double dy = y[i] - y[j];
			double dz = z[i] - z[j];
			double d2 = dx * dx + dy * dy + dz * dz;
			double mag = dt / (d2 * sqrt(d2));
			vx[i] -= dx * m[j] * mag;
			vy[i] -= dy * m[j] * mag;
			vz[i] -= dz * m[j] * mag;
			vx[j] += dx * m[i] * mag;
			vy[j] += dy * m[i] * mag;
			vz[j] += dz * m[i] * mag;
		}
	}
	for (i = 0; i < NB; i++) {
		x[i] += dt * vx[i];
		y[i] += dt * vy[i];
		z[i] += dt * vz[i];
	}
}

static double energy(void)
{
	double e = 0.0;
	int i, j;
	for (i = 0; i < NB; i++) {
		e += 0.5 * m[i] * (vx[i] * vx[i] + vy[i] * vy[i] + vz[i] * vz[i]);
		for (j = i + 1; j < NB; j++) {
			double dx = x[i] - x[j];
			double dy = y[i] - y[j];
			double dz = z[i] - z[j];
			e -= m[i] * m[j] / sqrt(dx * dx + dy * dy + dz * dz);
		}
	}
	return e;
}

int main(int argc, char **argv)
{
	int n = argc > 1 ? atoi(argv[1]) : 200000;
	int i;
	for (i = 0; i < NB; i++) {
		x[i] = (i % 5) * 1.5 - 3.0 + i * 0.125;
		y[i] = (i % 7) * 0.75 - 2.0;
		z[i] = (i % 3) * 1.25 - 1.0;
		vx[i] = (i % 4) * 0.03125 - 0.05;
		vy[i] = (i % 6) * 0.015625 - 0.04;
		vz[i] = (i % 2) * 0.0625 - 0.03;
		m[i] = 0.5 + (i % 9) * 0.25;
	}
	printf("nbody-start %.9f\n", energy());
	for (i = 0; i < n; i++)
		advance(0.001);
	printf("nbody-end %.9f\n", energy());
	return 0;
}
