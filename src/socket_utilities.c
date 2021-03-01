#include <inc/socket_utilities.h>
#include <inc/message.h>

uint16_t str_to_uint16_t(char *string){
    long int n;
    char *eptr;

    /* VSCode doesn't find errno, but it's there dw */
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

/* TODO Change to dynamic string (data_buffer) */
void read_message_to_buffer(int client_socket, char *data_buffer){
    ssize_t received_bytes;

    while((received_bytes = recv(client_socket, data_buffer,
                sizeof(char)*255, 0)) > 0){
        printf(
            "This was in the mail: %s\nBytes: %lu\n",
            data_buffer,
            received_bytes
        );     
    }

    /* Handle other errors than connection reset */
    if(received_bytes == -1 && errno != ECONNRESET){
        HANDLE_ERROR("There was problem with recv", 1);
    }
}

void read_message_into_queue(int socket, char *data_buffer){
    ssize_t received_bytes;

    /* TODO parse message data*/
    while((received_bytes = recv(socket, data_buffer,
                sizeof(char)*255, 0)) > 0){ 
        add_message_to_queue(data_buffer, &read_head, &read_tail);
        break;
    }

    /* Handle other errors than connection reset */
    if(received_bytes == -1 && errno != ECONNRESET){
        HANDLE_ERROR("There was problem with recv", 1);
    }

    //Msg new_message;
    //strncpy(new_message.msg, data_buffer, MAX_MSG_LEN);

    return;
}

void handle_error(char *msg, int show_err, char *file, int line){
	fprintf(
		stderr,
		"ERROR(%d) %s:%d -- %s:%s\n",
		errno,
		file, line,
        (msg != NULL) ? msg : "-",
		(show_err) ? strerror(errno): "-"
	);
	exit(EXIT_FAILURE);
}

void *write_to_socket(void *s){
    /* TODO Allocate this string dynamically */
    char buffer_boi[256];
    int socket = *((int *)s);

    /* Establish write connection */
    printf("Write to socket\n");
    /* Listen for messages from the server */
    fd_set connected_socks, ready_socks;

    /* Initialize structs */
    FD_ZERO(&connected_socks);

    /* Add server_socket into set */
    FD_SET(socket, &connected_socks);

    while(1){
        ready_socks = connected_socks;

        fgets(buffer_boi, 256, stdin);
        buffer_boi[strlen(buffer_boi)-1] = '\0';

        /* TODO Time out should be set */
        if(select(socket+1, NULL, &ready_socks, NULL, NULL) < 0){
            HANDLE_ERROR("There was problem with write select", 1);
        }

        if(FD_ISSET(socket, &ready_socks)){
            send(socket, buffer_boi, 255, 0);
        }
    }
  
    return NULL;
}

void *read_from_socket(void *s){
    char data_buffer[256];
    int socket = *((int *)s);
    /* Listen for messages from the server */
    fd_set connected_socks, ready_socks;

    /* Initialize structs */
    FD_ZERO(&connected_socks);

    /* Add server_socket into set */
    FD_SET(socket, &connected_socks);
    printf("Read from socket\n");
    while(1){
        ready_socks = connected_socks;

        /* TODO Time out should be set */
        if(select(socket+1, &ready_socks, NULL, NULL, NULL) < 0){
            HANDLE_ERROR("There was problem with read select", 1);
        }
        /* Select removes socks that are not ready from the set */
        /* Cycle through the part of the FD_SET - Finding modified socks */
        /* Client is attempting to connect */
        if(FD_ISSET(socket, &ready_socks)){
            read_message_into_queue(socket, data_buffer);
        }
    }

    return NULL;
}