#include <inc/socket_utilities.h>
#include <inc/message.h>
#include <inc/general.h>

uint16_t str_to_uint16_t(char *string){
    long int n;
    char *eptr;

    errno = 0;

    n = strtol(string, &eptr, 10);

    if(errno == ERANGE){
        HANDLE_ERROR("Under or overflow with the conversion: char * -> long", 1);
    }

    if(eptr == string){
        HANDLE_ERROR("Failed with the conversion: char * -> long", 0);
    }

    if(n > __UINT16_MAX__ || n < 0){
        HANDLE_ERROR("Under or overflow the conversion: long -> uint16_t", 0);
    }

    return (uint16_t)n;
}

in_addr_t str_to_bin_IP(char *string){
    in_addr_t address;
    
    if((inet_pton(AF_INET, string, &address)) != 1){
        fprintf(
            stderr,
            "Failed to convert the hosting IP (%s) char * -> in_addr_t\n",
            string
        );
        exit(EXIT_FAILURE);
    }

    return address;
}

char *message_to_ascii_packet(Msg *message, int *size){
    char *packet;

    int packet_size = MAX_USERNAME_LEN + ID_SIZE + strlen(message->msg) + 1;

    if((packet = malloc(packet_size)) == NULL){
        HANDLE_ERROR("Failed to allocate memory for a packet", 1);
    }

    int offset = 0; 
    /* Insert username */
    memcpy(packet, message->username, MAX_USERNAME_LEN); // saves the null byte
    offset += MAX_USERNAME_LEN;

    memcpy(packet + offset, message->id, ID_SIZE); // saves the null byte
    offset += ID_SIZE;

    memcpy(packet + offset, message->msg, strlen(message->msg) + 1); // saves the null byte

    *size = packet_size;

    return packet;
}

Msg ascii_packet_to_message(char *data_buffer){
    Msg message;

    int offset = 0;

    strncpy(message.username, data_buffer, MAX_USERNAME_LEN);
    offset += MAX_USERNAME_LEN;

    strncpy(message.id, data_buffer + offset, ID_SIZE);
    offset += ID_SIZE;

    strncpy(message.msg, data_buffer + offset, MAX_MSG_LEN);

    return message;
}

void read_message_into_queue(int socket, char *data_buffer){
    ssize_t received_bytes;

    int max_size = MAX_MSG_LEN + MAX_USERNAME_LEN + ID_SIZE;
    
    /* change byte order from big endian into host byte order */
    received_bytes = recv(socket, data_buffer, max_size, 0);

    if (received_bytes > 0){
        add_message_to_queue(
            ascii_packet_to_message(data_buffer),
            &read_head, &read_tail, &r_lock
        );
    }

    /* Handle other errors than connection reset */
    if(received_bytes == -1 && errno != ECONNRESET){
        HANDLE_ERROR("There was problem with recv", 1);
    }

    return;
}

void *write_to_socket(void *p_socket){
    int socket = *((int *)p_socket);

    free(p_socket);

    fd_set connected_socks, ready_socks;

    /* Initialize structs */
    FD_ZERO(&connected_socks);

    /* Add server_socket into set */
    FD_SET(socket, &connected_socks);

    Msg *outgoing_msg;
    char *ascii_packet;
    int packet_size;

    while(true){
        ready_socks = connected_socks;

        /* TODO Time out should be set */
        if(select(socket+1, NULL, &ready_socks, NULL, NULL) < 0){
            HANDLE_ERROR("There was problem with write select", 1);
        }

        if(FD_ISSET(socket, &ready_socks)){
            if((outgoing_msg = pop_msg_from_queue(&write_head, &w_lock)) != NULL){

                ascii_packet = message_to_ascii_packet(outgoing_msg, &packet_size);
                send(socket, ascii_packet, packet_size, 0);

                free(outgoing_msg);
                free(ascii_packet);
            }
        }
    }
  
    return NULL;
}

void *read_from_socket(void *p_socket){
    int socket = *((int *)p_socket);
    char data_buffer[256];

    free(p_socket);

    fd_set connected_socks, ready_socks;

    /* Initialize structs */
    FD_ZERO(&connected_socks);

    /* Add server_socket into set */
    FD_SET(socket, &connected_socks);

    while(true){
        ready_socks = connected_socks;

        /* TODO Time out should be set */
        if(select(socket+1, &ready_socks, NULL, NULL, NULL) < 0){
            HANDLE_ERROR("There was problem with read select", 1);
        }

        if(FD_ISSET(socket, &ready_socks)){
            read_message_into_queue(socket, data_buffer);
        }
    }

    return NULL;
}