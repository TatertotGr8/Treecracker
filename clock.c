int64_t clock() {
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    return tp.tv_sec * (uint64_t) 1e9 + tp.tv_nsec;
}
