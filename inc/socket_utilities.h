#ifndef SOCK_UTIL
    #define SOCK_UTIL

    #define MAX_PORT_STR 6
    #define MAX_IPV4_STR 16
    #define LOCAL_HOST "0.0.0.0"

    #include <unistd.h> //For close
    #include <signal.h>

    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/select.h>
    
    #include <arpa/inet.h> //For inet_ntop
    #include <netinet/in.h> //Structures for address information

    #include <inc/message.h>

    uint16_t str_to_uint16_t(char *string);
    in_addr_t str_to_bin_IP(char *string);
    void read_message_to_buffer(int client_socket, char *data_buffer);
    char *message_to_ascii_packet(Msg *message, int *size);
    Msg ascii_packet_to_message(char *data_buffer);

    void *write_to_socket(void *socket);
    void *read_from_socket(void *socket);

#endif