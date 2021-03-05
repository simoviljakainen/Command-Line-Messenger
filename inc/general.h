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

    #define SALT_LEN 16
    #define HASH_LEN 32

    void handle_error(char *msg, int show_err, char *file, int line);
    uint8_t *pseudo_random_bytes(int bytes);
    char *generate_argonid_hash(char *password);
    int verify_argonid(char *hash, char *password);

    #define HANDLE_ERROR(msg, show_err) handle_error(msg, show_err, __FILE__, __LINE__);

#endif