#include <inc/setting.h>
#include <inc/general.h>
#include <inc/socket_utilities.h>
#include <inc/message.h>

void handle_connections(int server_socket);
void handle_sigpipe(int _);
void accept_connection(int server_socket);

void *message_listener(void *socket);
void *broadcast_message(void *_);

Client clients[FD_SETSIZE];
int client_index = 0;

void start_server(void){

/*******************   SETTING UP THE CONNECTTION   *******************/

    /* Catch signal when client disconnects */
    struct sigaction act;

    act.sa_handler = handle_sigpipe;
    sigemptyset (&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGPIPE, &act, NULL);

    /* Set IP and port */
    in_addr_t addr = str_to_bin_IP(connection.ipv4);
    int16_t port_num = str_to_uint16_t(connection.port);

    /* Create socket */
    int inet_socket = socket(AF_INET, SOCK_STREAM, 0);

    if(inet_socket == -1){
        HANDLE_ERROR("Failed to create a socket.", 1);
    }

    /* Create server address */
    struct sockaddr_in server_address;

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port_num);
    server_address.sin_addr.s_addr = addr;

    /* Bind socket for listening */
    int status = bind(
        inet_socket,
        (struct sockaddr *)&server_address,
        sizeof(server_address)
    );

    if(status){
        fprintf(
            stderr, "Failed to bind %s:%d - %s\n",
            connection.ipv4, port_num,
            strerror(errno)
        );
        exit(EXIT_FAILURE);
    }

    /* Convert address to string from binary - just to check if it's malformed */
    char ip_v4[INET_ADDRSTRLEN];
    inet_ntop(
        AF_INET,
        &server_address.sin_addr.s_addr, ip_v4,
        INET_ADDRSTRLEN-1
    );
    printf("Bound %s:%d\n", ip_v4, port_num);
    

    /* FIXME increase the number of queued connections when threads are added
    Listen for connections */
    if(listen(inet_socket, 5) != 0){
        fprintf(stderr, "Failed to listen %s:%d\n", connection.ipv4, port_num);
        exit(EXIT_FAILURE);
    }

/*******************   LISTENING FOR CONNECTIONS   ********************/

    init_list(&read_head, &read_tail);
    
    printf("Listening for connections...\n");

    pthread_t broadcast;

    /* Start message broadcast thread */
    pthread_create(&broadcast, NULL, broadcast_message, NULL);

    /* Main thread starts handling connections */
    handle_connections(inet_socket);

    pthread_join(broadcast, NULL);

    empty_list(&read_head);
    close(inet_socket);

    return;
}

/* One thread handles connections (main) - 
The thread will start a new thread for every client*/
void handle_connections(int server_socket){

    fd_set server_sockets, ready_socks;

    /* Initialize structs */
    FD_ZERO(&server_sockets);

    /* Add server_socket into set */
    FD_SET(server_socket, &server_sockets);

    /* TODO limit CPU usage */
    while(client_index < connection.max_connections){
        ready_socks = server_sockets;

        /* TODO Time out should be set */
        if(select(server_socket+1, &ready_socks, NULL, NULL, NULL) < 0){
            HANDLE_ERROR("There was problem with read select connections", 1);
        }

        /* Client is attempting to connect */
        if(FD_ISSET(server_socket, &ready_socks)){
            accept_connection(server_socket);
        }
    }

/*******************   ALL CONNECTIONS ACCEPTED   *******************/

    printf("All connections accepted. Waiting threads to finish\n");

    for(int i = 0; i < client_index; i++){
        pthread_join(clients[i].read_thread, NULL);
    }

    return;
}

void accept_connection(int server_socket){

    socklen_t addr_size = sizeof(clients[client_index].addr);

    clients[client_index].socket = accept(
        server_socket,
        (struct sockaddr *)&clients[client_index].addr,
        &addr_size
    );

    char ip_v4[MAX_IPV4_STR];

    inet_ntop(
        AF_INET,
        &clients[client_index].addr.sin_addr.s_addr,
        ip_v4,
        MAX_IPV4_STR-1
    );

    printf("Connection incoming from %s\n", ip_v4);

    char argon2id_hash[256];

    if(read_one_packet(clients[client_index].socket, argon2id_hash, 256)){
        close(clients[client_index].socket);
        return;
    }

    /* Drop connection - wrong password */
    if(verify_argon2id(argon2id_hash, connection.password)){
        send(
            clients[client_index].socket,
            "401",
            sizeof("401"), MSG_NOSIGNAL
        );
        close(clients[client_index].socket);

        return;
    }
    
    send(
        clients[client_index].socket,
        "100",
        sizeof("100"), MSG_NOSIGNAL
    );

    printf("Connection accepted from %s\n", ip_v4);

    pthread_mutex_lock(&r_lock);

    add_message_to_queue((Msg) {
        .msg = "------- New connection accepted -------",
        .username = "Server",
        .id = "0"}, &read_head, &read_tail, NULL);
    
    pthread_cond_signal(&message_ready);

    pthread_mutex_unlock(&r_lock);

    /* Allocating heap mem for socket num as it's sent to a thread */
    int *sock_fd;

    if((sock_fd = (int *)malloc(sizeof(clients[client_index].socket))) == NULL){
        HANDLE_ERROR("Failed to allocate memory for socket fd", 1);
    }

    *sock_fd = clients[client_index].socket;

    /* Start message listening thread for the new client */
    pthread_create(
        &clients[client_index].read_thread,
        NULL, message_listener, sock_fd
    );

    client_index++;

    return;
}

void handle_sigpipe(int _){
    /* NOTE THAT PRINTF might have caused the signal 
    TODO change printf to write as it's sigsafe */
    if(errno == ECONNRESET){
        printf("Client disconnected\n");
        /* TODO Remove client from list */

        /* Exit the thread */
    }
    /* Dunno where the signal came from */
    printf("Should I catch this?\n");

    return;
}

int find_client_index(int socket){
    
    for(int i = 0; i < client_index; i++){
        if(clients[i].socket == socket)
            return i;
    }

    return -1;
}

void handle_disconnect(int socket){
    int index;

    if((index = find_client_index(socket)) == -1){
        HANDLE_ERROR("Couldn't find the client index.", 0);
    }


    pthread_cancel(clients[client_index].read_thread);

    /* Remove the client from the clients arr */

}

/* Puts messages sent by client into a queue for broadcasts */
void *message_listener(void *p_socket){
    int socket = *((int *)p_socket);
    free(p_socket);

    fd_set connected_socks, ready_socks;

    /* Initialize structs */
    FD_ZERO(&connected_socks);
    FD_SET(socket, &connected_socks);

    ssize_t received_bytes;
    Msg msg;

    int max_size = MAX_MSG_LEN + MAX_USERNAME_LEN + ID_SIZE;
    char data_buffer[max_size];

    while(true){
        ready_socks = connected_socks;

        /* TODO Time out should be set */
        if(select(socket+1, &ready_socks, NULL, NULL, NULL) < 0){
            HANDLE_ERROR("There was problem with read select", 1);
        }

        if(FD_ISSET(socket, &ready_socks)){
            received_bytes = recv(socket, data_buffer, max_size, 0);

            if (received_bytes > 0){
                msg = ascii_packet_to_message(data_buffer);
                snprintf(msg.id, ID_SIZE, "%d", socket);

                pthread_mutex_lock(&r_lock);

                add_message_to_queue(msg, &read_head, &read_tail, NULL);
                pthread_cond_signal(&message_ready);

                pthread_mutex_unlock(&r_lock);
            }

            /* Handle other errors than connection reset */
            if(received_bytes == -1 && errno != ECONNRESET){
                HANDLE_ERROR("There was problem with recv", 1);
            }
        }
    }

    return NULL;
}

/* Broadcasts every client message to every client connected */
void *broadcast_message(void *_){
    Msg *outgoing_msg;
    int size;
    char *ascii_packet;

    while(true){

        pthread_mutex_lock(&r_lock);

        if((outgoing_msg = pop_msg_from_queue(&read_head, NULL)) == NULL){
            pthread_cond_wait(&message_ready, &r_lock);
            
            /* Message should be ready now */
            outgoing_msg = pop_msg_from_queue(&read_head, NULL);
        }

        pthread_mutex_unlock(&r_lock);

        ascii_packet = message_to_ascii_packet(outgoing_msg, &size);
        
        for(int i = 0; i < client_index; i++){
            if(send(clients[i].socket, ascii_packet, size, MSG_NOSIGNAL) == -1){
                printf("socket %d has disconnected.\n", i);
            }
        }

        free(outgoing_msg);
        free(ascii_packet);
    }
}
