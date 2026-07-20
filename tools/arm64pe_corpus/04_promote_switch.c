/* Large switch that shrinks when a value is register-promoted: this is the
   exact shape (a big case ladder where each case's store is elided) that hit
   the stale-.text tail bug on arm64-PE (commit 80ea9843). Meaningful only when
   the cross pair is built WITH the optimizer (see README: MCC_CONFIG_OPTIMIZER
   is a SILENT no-op in the default cross build). Harmless otherwise. */
int classify(int k) {
    int r = 0;
    switch (k) {
        case 0: r = 10; break;
        case 1: r = 11; break;
        case 2: r = 12; break;
        case 3: r = 13; break;
        case 4: r = 14; break;
        case 5: r = 15; break;
        case 6: r = 16; break;
        case 7: r = 17; break;
        case 8: r = 18; break;
        case 9: r = 19; break;
        case 100: r = 200; break;
        case 200: r = 400; break;
        case 300: r = 600; break;
        default: r = -1; break;
    }
    return r * 2 + 1;
}
