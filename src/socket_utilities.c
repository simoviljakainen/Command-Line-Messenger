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

void read_message_into_queue(int socket, char *data_buffer){
    ssize_t received_bytes;

    /* TODO parse message data*/
    while((received_bytes = recv(socket, data_buffer,
                sizeof(char)*255, 0)) > 0){
        add_message_to_queue(data_buffer, &read_head, &read_tail, &r_lock);
        break;
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

    printf("Write to socket\n");

    fd_set connected_socks, ready_socks;

    /* Initialize structs */
    FD_ZERO(&connected_socks);

    /* Add server_socket into set */
    FD_SET(socket, &connected_socks);

    Msg *outgoing_msg;

    while(true){
        ready_socks = connected_socks;

        /* TODO Time out should be set */
        if(select(socket+1, NULL, &ready_socks, NULL, NULL) < 0){
            HANDLE_ERROR("There was problem with write select", 1);
        }

        if(FD_ISSET(socket, &ready_socks)){
            if((outgoing_msg = pop_msg_from_queue(&write_head, &w_lock)) != NULL){
                send(socket, outgoing_msg->msg, 255, 0);
                free(outgoing_msg);
            }
        }
    }
  
    return NULL;
}

void *read_from_socket(void *p_socket){
    int socket = *((int *)p_socket);
    char data_buffer[256];

    free(p_socket);

    printf("Read from socket\n");

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