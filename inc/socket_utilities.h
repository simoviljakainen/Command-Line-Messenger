#ifndef SOCK_UTIL
    #define SOCK_UTIL

    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>

    /* For inet_ntop */
    #include <arpa/inet.h>

    /* Structures for address information */
    #include <netinet/in.h>

    #include <limits.h>
    #include <errno.h>

    #include <sys/select.h>

    uint16_t str_to_uint16_t(char *string);
    in_addr_t str_to_bin_IP(char *string);
    void read_message_to_buffer(int client_socket, char *data_buffer);
    void handle_error(char *msg, int show_err, char *file, int line);

    void *write_to_socket(void *s);
    void *read_from_socket(void *s);
    #define HANDLE_ERROR(msg, show_err) handle_error(msg, show_err, __FILE__, __LINE__);

#endif