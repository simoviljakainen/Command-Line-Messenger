#ifndef GENERAL_H
    #define GENERAL_H

    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <stdbool.h>
    #include <unistd.h>
    #include <limits.h>
    #include <errno.h>
    #include <pthread.h>
    #include <inttypes.h>
    #include <time.h>
    #include <sys/time.h>
    #include <math.h>

    void handle_error(char *msg, int show_err, char *file, int line);
    uint8_t *pseudo_random_bytes(int bytes);
    char *generate_argon2id_hash(char *password);
    int verify_argon2id(char *hash, char *password);

    struct timespec nanosec_to_timespec(long nanosecs);
    long timespec_to_nanosec(struct timespec ts);
    struct timespec remainder_timespec(struct timespec t1, struct timespec t2);
    struct timespec get_time_interval(struct timeval start, struct timeval end);

    #define HANDLE_ERROR(msg, show_err) handle_error(msg, show_err, __FILE__, __LINE__);

#endif