#include <inc/setting.h>
#include <inc/general.h>
#include <inc/socket_utilities.h>
#include <inc/message.h>

in_addr_t str_to_bin_IP(char *string){
    in_addr_t address;
    
    if((inet_pton(AF_INET, string, &address)) != 1){
        HANDLE_ERROR("Failed to convert the IP char * -> in_addr_t", 1);
    }

    return address;
}

void bin_IP_to_str(in_addr_t ip, char *buffer){
    if(inet_ntop(AF_INET, &ip, buffer, MAX_IPV4_STR) == NULL){
        HANDLE_ERROR("Failed to convert the in_addr_t -> char *IP", 1);
    }
}


char *message_to_ascii_packet(Msg *message, int *size){
    char *packet;

    int packet_size = MAX_USERNAME_LEN + ID_SIZE + strlen(message->msg) + 1;

    if((packet = malloc(packet_size)) == NULL){
        HANDLE_ERROR("Failed to allocate memory for a packet", 1);
    }

    int offset = 0; 

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
    snprintf(message.username, MAX_USERNAME_LEN, "%s", data_buffer);
    offset += MAX_USERNAME_LEN;

    snprintf(message.id, ID_SIZE, "%s", data_buffer + offset);
    offset += ID_SIZE;

    snprintf(message.msg, MAX_MSG_LEN, "%s", data_buffer + offset);
    
    return message;
}

int read_one_packet(int socket, char *buffer, size_t buffer_size){
    fd_set ready_sockets;

    /* Initialize structs */
    FD_ZERO(&ready_sockets);
    FD_SET(socket, &ready_sockets);

    /* TODO 3 sec timeout */
    if(select(socket+1, &ready_sockets, NULL, NULL, &(struct timeval){.tv_sec = 3}) < 0){
        HANDLE_ERROR("There was problem with read select", 1);
    }

    if(FD_ISSET(socket, &ready_sockets)){
        if(recv(socket, buffer, buffer_size, 0) > 0)
            return 0;
    }
        
    return 1;
}