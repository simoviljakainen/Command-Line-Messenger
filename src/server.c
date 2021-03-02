
/* For close */
#include <unistd.h>

/* For creating sockets */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>

#include <pthread.h>
#include <signal.h>

#include <inc/socket_utilities.h>
#include <inc/message.h>

/* TODO change error numbers */

void *message_listener(void *socket);
void handle_connections(int server_socket);
int ping_client(int client_socket);
void handle_sigpipe(int _);
void *broadcast_message(void *);
int accept_connection(
    int server_socket, int *client_sockets, int client_index,
    struct sockaddr_in *connections);

void start_server(char *host, char *port){
    int status;
    struct sigaction act;

    /* Catch signal when client disconnects */
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
    status = bind(
        inet_socket,
        (struct sockaddr *)&server_address,
        sizeof(server_address)
    );

    if(status){
        fprintf(stderr, "Failed to bind %s:%d\n", host, port_num);
        exit(EXIT_FAILURE);
    }

    /* Convert address to string from binary - just to check if it's malformed */
    char ip_v4[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &server_address.sin_addr.s_addr, ip_v4, INET_ADDRSTRLEN-1);
    printf("Bound %s:%d\n", ip_v4, port_num);
    
    init_g(&read_head, &read_tail);

    /* FIXME increase the number of queued connections when threads are added
    Listen for connections */
    if(listen(inet_socket, 1) != 0){
        fprintf(stderr, "Failed to listen %s:%d\n", host, port_num);
        exit(EXIT_FAILURE);
    }

    printf("Listening for connections...\n");

    /* Main thread starts handling connections */
    handle_connections(inet_socket);

    close(inet_socket);

}

int accept_connection(
    int server_socket, int *client_sockets, int client_index,
    struct sockaddr_in *connections){

    /* Accept connection */
    socklen_t peer_con_size = sizeof(connections[client_index]);

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

    return 0;
}

/* One thread handles connections (main) - 
The thread will spawn another thread for every client*/
void handle_connections(int server_socket){
    char server_msg[] = "Your mother.";
    int client_index = 0;

    /* 1024 = Max connections that select can handle */
    int client_sockets[1024];
    struct sockaddr_in peer_connections[1024];

    /* Event driven - wait that a connection arrives */
    fd_set connected_socks, ready_socks;

    /* Initialize structs */
    FD_ZERO(&connected_socks);

    /* Add server_socket into set */
    FD_SET(server_socket, &connected_socks);

    while(1){
        ready_socks = connected_socks;

        /* TODO Time out should be set */
        if(select(server_socket+1, &ready_socks, NULL, NULL, NULL) < 0){
            HANDLE_ERROR("There was problem with read select connections", 1);
        }
        /* Select removes socks that are not ready from the set */
        /* Cycle through the part of the FD_SET - Finding modified socks */
        /* Client is attempting to connect */
        if(FD_ISSET(server_socket, &ready_socks)){
            accept_connection(
                server_socket,
                client_sockets,
                client_index,
                peer_connections
            );
            break; //for testing
        }
    }
    
    /* Send server data */ 
    send(client_sockets[client_index], server_msg, sizeof(server_msg), 0);

    /* Start thread for every connection */

    /* Make new thread for writing */
    pthread_t stdin_to_client, stdout_from_client;

    /* Allocating heap mem for socket num as it's sent to a thread */
    int *p_sock = (int *)malloc(sizeof(client_sockets[client_index]));
    *p_sock = client_sockets[client_index];
    pthread_create(&stdout_from_client, NULL, read_from_socket, p_sock);
    pthread_create(&stdin_to_client, NULL, broadcast_message, p_sock);
   
    client_index++;

    pthread_join(stdin_to_client, NULL);
    pthread_join(stdout_from_client, NULL);

    free(p_sock);

    /* Close the thread */

}

/* TODO  this adds overhead. some kind of timeout is needed */
int ping_client(int client_socket){
    ssize_t sent_bytes;
    sent_bytes = send(client_socket, "ping", sizeof("ping"), 0);

    printf("bytes : %ld\n", sent_bytes);
    if(sent_bytes == -1)
        printf("errno: %d", errno);
    return 1;
}

void handle_sigpipe(int _){
    /* NOTE THAT PRINTF might have caused the signal 
    TODO change printf to write as it's sigsafe */
    if(errno == ECONNRESET){
        printf("Client disconnected\n");
        /* TODO Remove client from list */

        /* Exit the thread */
        exit(EXIT_SUCCESS);
    }
    /* Dunno where the signal came from */
    printf("Should I catch this?\n");
    exit(EXIT_SUCCESS);
}

void *broadcast_message(void *s){
    int client_socket = *((int *)s);
    fd_set connected_socks, ready_socks;

    /* Establish write connection ?*/

    /* Initialize structs */
    FD_ZERO(&connected_socks);
    /* add the client_socket into set */
    FD_SET(client_socket, &connected_socks);

    Msg *outgoing_msg;
    /* The thread will check socket for message until client disconnects */
    while(1){

        ready_socks = connected_socks;

        /* check if client has disconnected */
        //ping_client(client_socket);

        /* TODO Time out should be set */
        if(select(client_socket+1, NULL, &ready_socks, NULL, NULL) < 0){
            HANDLE_ERROR("There was problem with read select", 1);
        }
        /* Select removes socks that are not ready from the set */
        /* Cycle through the part of the FD_SET - Finding modifed socks */
        /* only one sock per thread so no other looping needed */

        /* There is something to read from client */
        if(FD_ISSET(client_socket, &ready_socks)){
            if((outgoing_msg = pop_msg_from_queue(&read_head)) != NULL){
                send(client_socket, outgoing_msg->msg, 255, 0);
                free(outgoing_msg);
            }
        }

    }
}

void *message_listener(void *socket){
    int client_socket = *((int *)socket);
    char data_buffer[256];
    fd_set connected_socks, ready_socks;

    /* Establish write connection ?*/

    /* Initialize structs */
    FD_ZERO(&connected_socks);
    /* add the client_socket into set */
    FD_SET(client_socket, &connected_socks);

    /* The thread will check socket for message until client disconnects */
    printf("Listening for messages\n");

    while(1){

        ready_socks = connected_socks;

        /* check if client has disconnected */
        //ping_client(client_socket);

        /* TODO Time out should be set */
        if(select(client_socket+1, &ready_socks, NULL, NULL, NULL) < 0){
            HANDLE_ERROR("There was problem with read select", 1);
        }
        /* Select removes socks that are not ready from the set */
        /* Cycle through the part of the FD_SET - Finding modifed socks */
        /* only one sock per thread so no other looping needed */

        /* There is something to read from client */
        if(FD_ISSET(client_socket, &ready_socks)){
            //read_message_into_queue(client_socket, data_buffer);
        }

    }
}
