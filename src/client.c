#include <inc/socket_utilities.h>
#include <inc/window_manager.h>
#include <inc/message.h>
#include <inc/general.h>

void start_client(char *host, char *port){

/*******************   SETTING UP THE CONNECTTION   *******************/

    /* Set IP and port */
    in_addr_t addr = str_to_bin_IP(host);
    int16_t port_num = str_to_uint16_t(port);

    /* create a new socket  int socket(int domain_of_the_socket, int type, int protocol = 0 default)*/
    /* AF_INET, internet protocol 4 address family. Can be e.g. AF_BLUETOOTH or like AF_UNIX */
    /* SOCK_STREAM -> TCP, SOCK_DGRAM -> UDP */
    int inet_socket = socket(AF_INET, SOCK_STREAM, 0);

    /* Address for the socket */
    struct sockaddr_in server_address;

    /* Specify the socket family - type of address */
    server_address.sin_family = AF_INET;

    /* Port to connect - needs to be converted from integer */
    /* htons, - convert values between host and network byte order */
    server_address.sin_port = htons(port_num);

    /* Server IP - INADDR_ANY = 0.0.0.0*/
    server_address.sin_addr.s_addr = addr;

    /* Connect to the server - SocketIn needs to be casted (polymorph?)*/
    int status = connect(
        inet_socket,
        (struct sockaddr *)&server_address,
        sizeof(server_address)
    );

    /* If binding succeeds, connect returns 0, -1 if error and errno */
    if(status){
        fprintf(stderr, "Failed to connect %s:%d\n", host, port_num);
        exit(EXIT_FAILURE);
    }

/**********************   CONNECTED TO SERVER   ***********************/

    /* Init the message queues */
    init_list(&read_head, &read_tail);
    init_list(&write_head, &write_tail);

    pthread_t message_sender, message_listener, user_interface;

    /* Allocating heap mem for socket num as it's sent to threads thread */
    int *sock_fd;

    if((sock_fd = (int *)malloc(sizeof(inet_socket))) == NULL){
        HANDLE_ERROR("Failed to allocate memory for socket fd", 1);
    }
    *sock_fd = inet_socket;

    /* One thread handles all the incoming messages, one outgoing and one is for the UI */
    pthread_create(&message_listener, NULL, read_from_socket, sock_fd);
    pthread_create(&message_sender, NULL, write_to_socket, sock_fd);
    pthread_create(&user_interface, NULL, run_ncurses_window, NULL);

    pthread_join(message_sender, NULL);
    pthread_join(message_listener, NULL);
    pthread_join(user_interface, NULL);

/***********************   CONNECTION CLOSED   ************************/

    free(sock_fd);

    /* Free queues */
    empty_list(&read_head);
    empty_list(&write_head);

    close(inet_socket);

    return;
}
