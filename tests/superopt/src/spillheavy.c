extern int printf(const char *, ...);

struct body {
	double x, y, z, vx, vy, vz, mass;
};

static struct body bodies[5];

static void advance(int n, double dt)
{
	int i, j;

	for (i = 0; i < n; i++) {
		struct body *bi = &bodies[i];
		for (j = i + 1; j < n; j++) {
			struct body *bj = &bodies[j];
			double dx = bi->x - bj->x;
			double dy = bi->y - bj->y;
			double dz = bi->z - bj->z;
			double d2 = dx * dx + dy * dy + dz * dz;
			double mag = dt / (d2 * d2);
			double bm = bj->mass * mag;
			double im = bi->mass * mag;

			bi->vx -= dx * bm;
			bi->vy -= dy * bm;
			bi->vz -= dz * bm;
			bj->vx += dx * im;
			bj->vy += dy * im;
			bj->vz += dz * im;
		}
	}
	for (i = 0; i < n; i++) {
		bodies[i].x += dt * bodies[i].vx;
		bodies[i].y += dt * bodies[i].vy;
		bodies[i].z += dt * bodies[i].vz;
	}
}

int main(void)
{
	int i, k;

	for (i = 0; i < 5; i++) {
		bodies[i].x = i + 1;
		bodies[i].y = i + 2;
		bodies[i].z = i + 3;
		bodies[i].mass = i + 1;
	}
	for (k = 0; k < 100; k++)
		advance(5, 0.01);
	printf("%.6f\n", bodies[0].x + bodies[4].z);
	return 0;
}
