void s7_25_asctime(void) {
    struct tm t;
    memset(&t, 0, sizeof t);
    t.tm_wday = 3;
    t.tm_mon = 0;
    t.tm_mday = 1;
    t.tm_hour = 13;
    t.tm_min = 2;
    t.tm_sec = 3;
    t.tm_year = 97;
    printf("asctime=[%s]", asctime(&t));
}

void s7_25_strftime(void) {
    time_t t = 0;
    struct tm *g = gmtime(&t);
    char buf[256];
    strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", g);
    printf("s1=%s\n", buf);
    strftime(buf, sizeof buf, "%F|%T|%D|%R", g);
    printf("s2=%s\n", buf);
    strftime(buf, sizeof buf, "j=%j u=%u w=%w C=%C y=%y e=%e", g);
    printf("s3=%s\n", buf);
    strftime(buf, sizeof buf, "lit%%%n%tend", g);
    printf("s4=[%s]\n", buf);

    printf("cnt=%zu\n", strftime(buf, sizeof buf, "%Y", g));

    char small[4];
    printf("ovf=%zu\n", strftime(small, sizeof small, "%Y", g));
}

void s7_25_mktime_norm(void) {
    struct tm t;
    memset(&t, 0, sizeof t);
    t.tm_year = 100;
    t.tm_mon = 13;
    t.tm_mday = 1;
    t.tm_hour = 12;
    t.tm_isdst = -1;
    mktime(&t);
    printf("norm year=%d mon=%d mday=%d\n", t.tm_year, t.tm_mon, t.tm_mday);
}

void s7_25_difftime(void) {
    printf("diff=%d\n", difftime((time_t)100, (time_t)40) == 60.0);
    printf("diffneg=%d\n", difftime((time_t)40, (time_t)100) == -60.0);
    printf("cps_pos=%d\n", CLOCKS_PER_SEC > 0);
    printf("clock_ok=%d\n", clock() != (clock_t)-1);
}
