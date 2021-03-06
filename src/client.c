#include <inc/setting.h>
#include <inc/general.h>
#include <inc/socket_utilities.h>
#include <inc/window_manager.h>
#include <inc/message.h>

void start_client(char *host, char *port, char *pwd){

/*******************   SETTING UP THE CONNECTTION   *******************/

    /* Set IP and port */
    in_addr_t addr = str_to_bin_IP(host);
    int16_t port_num = str_to_uint16_t(port);

    /* Create a new socket int protocol = 0 default)*/
    /* SOCK_STREAM -> TCP, SOCK_DGRAM -> UDP */    
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if(server_socket == -1){
        HANDLE_ERROR("Failed to create a socket.", 1);
    }

    /* Create server address */
    struct sockaddr_in server_address;

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port_num);
    server_address.sin_addr.s_addr = addr;

    int status = connect(
        server_socket,
        (struct sockaddr *)&server_address,
        sizeof(server_address)
    );

    /* If binding succeeds, connect returns 0, -1 if error and errno */
    if(status){
        fprintf(
            stderr, "Failed to connect %s:%d - %s\n",
            host, port_num,
            strerror(errno)
        );
        exit(EXIT_FAILURE);
    }

/**********************   CONNECTED TO SERVER   ***********************/

    char *hash = generate_argon2id_hash(pwd);
    char server_response[256];

    send(server_socket, hash, strlen(hash) + 1, 0);
    
    if(read_one_packet(server_socket, server_response, 256)){
        close(server_socket);
        return;
    }

    if(strcmp(server_response, "100")){
        printf("Could not connect (%s). Closing client.\n", server_response);
        return;
    }

    free(hash);

    /* Handle disconnect messages etc */

    /* Init the message queues */
    init_list(&read_head, &read_tail);
    init_list(&write_head, &write_tail);
    
    pthread_t message_sender, message_listener, user_interface;

    /* Allocating heap mem for socket fd as it's sent to threads */
    int *sock_fd;

    if((sock_fd = (int *)malloc(sizeof(server_socket))) == NULL){
        HANDLE_ERROR("Failed to allocate memory for socket fd", 1);
    }
    *sock_fd = server_socket;

    pthread_create(&message_listener, NULL, read_from_socket, sock_fd);

    /* Read/write func frees the memory, hence the reallocation */
    if((sock_fd = (int *)malloc(sizeof(server_socket))) == NULL){
        HANDLE_ERROR("Failed to allocate memory for socket fd", 1);
    }
    *sock_fd = server_socket;

    pthread_create(&message_sender, NULL, write_to_socket, sock_fd);
    pthread_create(&user_interface, NULL, run_ncurses_window, NULL);

    pthread_join(message_sender, NULL);
    pthread_join(message_listener, NULL);
    pthread_join(user_interface, NULL);

/***********************   CONNECTION CLOSED   ************************/

    /* Free queues */
    empty_list(&read_head);
    empty_list(&write_head);

    close(server_socket);

    return;
}
