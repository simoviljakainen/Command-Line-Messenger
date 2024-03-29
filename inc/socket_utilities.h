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
    #include <inc/crypt.h>

    in_addr_t str_to_bin_IP(char *string);
    void bin_IP_to_str(in_addr_t ip, char *buffer);

    void read_message_to_buffer(int client_socket);
    char *message_to_ascii_packet(Msg *message, int *size);
    Msg ascii_packet_to_message(char *data_buffer);
    int read_one_packet(int socket, char *buffer, size_t buffer_size);

    typedef struct _client{
        int socket;
        uint16_t ctr;
        struct sockaddr_in addr;
        pthread_t read_thread;
        gcry_cipher_hd_t aes_gcm_handle;
    }Client;

#endif
