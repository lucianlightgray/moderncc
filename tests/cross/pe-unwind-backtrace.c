#include <stdio.h>

extern unsigned short __stdcall RtlCaptureStackBackTrace(unsigned long FramesToSkip,
    unsigned long FramesToCapture, void **BackTrace, unsigned long *BackTraceHash);

static void *frames[256];
static int captured;

int deepest(int seed) {
    volatile int pad[3];
    pad[0] = seed;
    captured = (int)RtlCaptureStackBackTrace(0, 256, frames, 0);
    return pad[0];
}
int frame_big(int x) {
    volatile int buf[4096];
    int i;
    for (i = 0; i < 4096; i++) buf[i] = x + i;
    return deepest(buf[0]) + buf[4095] - x - 4095;
}
int frame_mid(int x) {
    volatile int buf[40];
    buf[0] = x; buf[39] = x + 1;
    return frame_big(buf[0]) + buf[39] - x - 1;
}
int frame_small(int x) { volatile int a = x ^ 0; return frame_mid(a); }

int main(void) {
    void *fns[5]; const char *nm[5]; int hit[5]; int i, j, ok;
    int r = frame_small(7);
    fns[0]=(void*)deepest;     nm[0]="deepest";
    fns[1]=(void*)frame_big;   nm[1]="frame_big";
    fns[2]=(void*)frame_mid;   nm[2]="frame_mid";
    fns[3]=(void*)frame_small; nm[3]="frame_small";
    fns[4]=(void*)main;        nm[4]="main";
    for (i = 0; i < 5; i++) hit[i] = 0;
    for (j = 0; j < captured; j++) {
        void *f = frames[j];
        int best = -1;
        for (i = 0; i < 5; i++)
            if ((char *)f >= (char *)fns[i] &&
                (best < 0 || (char *)fns[i] > (char *)fns[best]))
                best = i;
        if (best >= 0 && (char *)f < (char *)fns[best] + 8192) hit[best] = 1;
    }
    ok = hit[1] && hit[2] && hit[3] && hit[4];
    printf("frames=%d", captured);
    for (i = 1; i < 5; i++) printf(" %s=%d", nm[i], hit[i]);
    printf(" r=%d ok=%d\n", r, ok);
    return ok ? 0 : 1;
}
