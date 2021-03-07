#include <inc/setting.h>
#include <inc/general.h>
#include <inc/socket_utilities.h>
#include <inc/message.h>

void handle_connections(int server_socket);
void handle_sigpipe(int _);
int accept_connection(int server_socket);

void *message_listener(void *socket);
void *broadcast_message(void *_);

Client clients[FD_SETSIZE];
int client_index = 0;

pthread_mutex_t client_lock = PTHREAD_MUTEX_INITIALIZER;

void start_server(void){

/*******************   SETTING UP THE CONNECTTION   *******************/

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
    int clients_connected = 0;
    fd_set server_sockets, ready_socks;

    /* Initialize structs */
    FD_ZERO(&server_sockets);

    /* Add server_socket into set */
    FD_SET(server_socket, &server_sockets);

    while(clients_connected < connection.max_connections){
        ready_socks = server_sockets;

        if(select(server_socket+1, &ready_socks, NULL, NULL, NULL) < 0){
            HANDLE_ERROR("There was problem with read select connections", 1);
        }

        /* Client is attempting to connect */
        if(FD_ISSET(server_socket, &ready_socks)){
            if(!accept_connection(server_socket))
                clients_connected++; 
        }
    }

/*******************   ALL CONNECTIONS ACCEPTED   *******************/

    printf("All connections accepted. Waiting threads to finish\n");

    for(int i = 0; i < client_index; i++){
        pthread_join(clients[i].read_thread, NULL);
    }

    return;
}

int accept_connection(int server_socket){
    Client new_client;

    socklen_t addr_size = sizeof(new_client.addr);

    new_client.socket = accept(
        server_socket,
        (struct sockaddr *)&new_client.addr,
        &addr_size
    );

    char ip_v4[MAX_IPV4_STR];
    bin_IP_to_str(new_client.addr.sin_addr.s_addr, ip_v4);
    printf("Connection incoming from %s\n", ip_v4);

    char argon2id_hash[256];

    if(read_one_packet(new_client.socket, argon2id_hash, 256)){
        close(new_client.socket);
        return 1;
    }

    /* Drop connection - wrong password */
    if(verify_argon2id(argon2id_hash, connection.password)){
        send(
            new_client.socket,
            "401",
            sizeof("401"), MSG_NOSIGNAL
        );
        close(new_client.socket);

        return 1;
    }
    
    send(
        new_client.socket,
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

    if((sock_fd = (int *)malloc(sizeof(new_client.socket))) == NULL){
        HANDLE_ERROR("Failed to allocate memory for socket fd", 1);
    }

    *sock_fd = new_client.socket;

    /* Start message listening thread for the new client */
    pthread_create(
        &new_client.read_thread,
        NULL, message_listener, sock_fd
    );

    pthread_mutex_lock(&client_lock);
    clients[client_index++] = new_client;
    pthread_mutex_unlock(&client_lock);

    return 0;
}

void handle_disconnect(int index){
    char buffer[256];

    snprintf(
        buffer, 256,
        "------- Client(%d) has left the chat  -------",
        clients[index].socket
    );

    pthread_cancel(clients[index].read_thread);
    close(clients[index].socket);

    /* Remove the client from the clients arr */
    pthread_mutex_lock(&client_lock);

    for(int i = index; i < client_index; i++){
        clients[i] = clients[i + 1]; 
    }
    client_index--;

    pthread_mutex_unlock(&client_lock);
    
    /* Broadcast the lost boi */
    pthread_mutex_lock(&r_lock);

    Msg msg = {.username = "Server", .id = "0"};
    strncpy(msg.msg, buffer, MAX_MSG_LEN);
    
    add_message_to_queue(msg, &read_head, &read_tail, NULL);
    pthread_cond_signal(&message_ready);

    pthread_mutex_unlock(&r_lock);

    return;
}

/* Puts messages sent by client into a queue for broadcasts */
void *message_listener(void *p_socket){
    int socket = *((int *)p_socket);
    free(p_socket);

    fd_set connected_socks, ready_socks;        /* TODO Time out should be set */

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
                handle_disconnect(i);
            }
        }

        free(outgoing_msg);
        free(ascii_packet);
    }
}
