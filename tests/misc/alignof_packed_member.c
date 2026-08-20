struct __attribute__((packed)) P { char c; int x; long l; };
struct Q { char c; int x; };
struct __attribute__((packed)) R { char c; int x __attribute__((aligned(8))); };

int main(void) {
	struct P p;
	struct Q q;
	struct R r;
	struct P *pp = &p;
	if (_Alignof(p.x) != 1) return 1;
	if (_Alignof(p.l) != 1) return 2;
	if (_Alignof(pp->x) != 1) return 3;
	if (_Alignof(q.x) != _Alignof(int)) return 4;
	if (_Alignof(r.x) != 8) return 5;
	if (_Alignof(p.x + 1) != _Alignof(int)) return 6;
	if (_Alignof(1 ? p.x : p.x) != _Alignof(int)) return 7;
	if (sizeof(p.x) != sizeof(int)) return 8;
	return 0;
}
