#include <inc/socket_utilities.h>
#include <inc/message.h>
#include <inc/general.h>

void *message_listener(void *socket);
void handle_connections(int server_socket, int max_connections);
void handle_sigpipe(int _);
void *broadcast_message(void *_);
int accept_connection(
    int server_socket, int *client_sockets, int client_index,
    struct sockaddr_in *connections, pthread_t *client_threads);

fd_set connected_sockets;
int max_socket_fd;

void start_server(char *host, char *port, int max_connections){

/*******************   SETTING UP THE CONNECTTION   *******************/

    /* Catch signal when client disconnects */
    struct sigaction act;

    act.sa_handler = handle_sigpipe;
    sigemptyset (&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGPIPE, &act, NULL);

    /* Set IP and port */
    in_addr_t addr = str_to_bin_IP(host);
    int16_t port_num = str_to_uint16_t(port);

    /* Create socket */
    int inet_socket = socket(AF_INET, SOCK_STREAM, 0);

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
            host, port_num,
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
        fprintf(stderr, "Failed to listen %s:%d\n", host, port_num);
        exit(EXIT_FAILURE);
    }

/*******************   LISTENING FOR CONNECTIONS   ********************/

    init_list(&read_head, &read_tail);
    FD_ZERO(&connected_sockets);
    max_socket_fd = 0;
    
    printf("Listening for connections...\n");

    pthread_t broadcast;

    /* Start message broadcast thread */
    pthread_create(&broadcast, NULL, broadcast_message, NULL);

    /* Main thread starts handling connections */
    handle_connections(inet_socket, max_connections);

    pthread_join(broadcast, NULL);

    empty_list(&read_head);
    close(inet_socket);

    return;
}

/* One thread handles connections (main) - 
The thread will spawn a new thread for every client*/
void handle_connections(int server_socket, int max_connections){
    int client_index = 0, status;

    /* 1024 = Max connections that select can handle */
    int client_sockets[FD_SETSIZE];
    struct sockaddr_in peer_connections[FD_SETSIZE];

    pthread_t client_threads[max_connections];

    fd_set server_sockets, ready_socks;

    /* Initialize structs */
    FD_ZERO(&server_sockets);

    /* Add server_socket into set */
    FD_SET(server_socket, &server_sockets);

    /* TODO limit CPU usage */
    /* TODO add client details into same structure */

    while(client_index < max_connections){
        ready_socks = server_sockets;

        /* TODO Time out should be set */
        if(select(server_socket+1, &ready_socks, NULL, NULL, NULL) < 0){
            HANDLE_ERROR("There was problem with read select connections", 1);
        }

        /* Client is attempting to connect */
        if(FD_ISSET(server_socket, &ready_socks)){
            status = accept_connection(
                server_socket,
                client_sockets,
                client_index,
                peer_connections,
                client_threads
            );

            if(!status)
                client_index++;
        }
    }

/*******************   ALL CONNECTTIONS ACCEPTED   *******************/

    printf("All connections accepted. Waiting threads to finish\n");

    for(int i = 0; i < max_connections; i++){
        pthread_join(client_threads[i], NULL);
    }

    return;
}

int accept_connection(
    int server_socket, int *client_sockets, int client_index,
    struct sockaddr_in *connections, pthread_t *client_threads){

    /* Accept connection */
    socklen_t peer_con_size = sizeof(connections[client_index]);

    /* TODO deny connection - return 1 */

    client_sockets[client_index] = accept(
        server_socket,
        (struct sockaddr *)&connections[client_index],
        &peer_con_size
    );

    char ip_v4[INET_ADDRSTRLEN];

    inet_ntop(
        AF_INET,
        &connections[client_index].sin_addr.s_addr,
        ip_v4,
        INET_ADDRSTRLEN-1
    );

    printf("Connection accepted from %s\n", ip_v4);

    /* Add client's socket into client list */
    FD_SET(client_sockets[client_index], &connected_sockets);
    max_socket_fd = client_sockets[client_index];

    /* Allocating heap mem for socket num as it's sent to a thread */
    int *sock_fd;

    if((sock_fd = (int *)malloc(sizeof(client_sockets[client_index]))) == NULL){
        HANDLE_ERROR("Failed to allocate memory for socket fd", 1);
    }

    *sock_fd = client_sockets[client_index];

    /* Start message listening thread for the new client */
    pthread_create(&client_threads[client_index], NULL, read_from_socket, sock_fd);

    return 0;
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

void *broadcast_message(void *_){
    fd_set ready_sockets;
    Msg *outgoing_msg;
    int size;
    char *ascii_packet;

    while(true){

        if((outgoing_msg = pop_msg_from_queue(&read_head, &r_lock)) != NULL){
            
            ascii_packet = message_to_ascii_packet(outgoing_msg, &size);

            ready_sockets = connected_sockets;
            
            if(select(max_socket_fd+1, NULL, &ready_sockets, NULL, NULL) < 0){
                HANDLE_ERROR("There was problem with broadcast select", 1);
            }
            
            /* Cycle through the part of the FD_SET - Finding ready socks */
            for(int i = 0; i < max_socket_fd+1; i++){
                if(FD_ISSET(i, &ready_sockets)){

                    /* TODO Insert right id into the msg */
                    send(i, ascii_packet, size, 0);
                }
            }          
            
            free(outgoing_msg);
            free(ascii_packet);
        }
    }
}
