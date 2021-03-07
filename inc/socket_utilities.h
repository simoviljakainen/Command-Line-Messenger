#ifndef SOCK_UTIL
    #define SOCK_UTIL

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
    void read_message_to_buffer(int client_socket);
    char *message_to_ascii_packet(Msg *message, int *size);
    Msg ascii_packet_to_message(char *data_buffer);
    int read_one_packet(int socket, char *buffer, size_t buffer_size);

    typedef struct _client{
        int socket;
        struct sockaddr_in addr;
        pthread_t read_thread;
    }Client;

#endif