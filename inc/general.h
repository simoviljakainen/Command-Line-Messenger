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

    void handle_error(char *msg, int show_err, char *file, int line);

    #define HANDLE_ERROR(msg, show_err) handle_error(msg, show_err, __FILE__, __LINE__);

#endif