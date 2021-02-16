
/* for close */
#include <unistd.h>

/* For creating sockets */
#include <sys/types.h>
#include <sys/socket.h>

#include <pthread.h>

#include <inc/socket_utilities.h>

void *write_to_server(void *server_socket);
void *read_from_server(void *s);

void start_client(char *host, char *port){
    char data_buffer[256];

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

    /* Receive data from server - returs the lenght of the message if successful */
    /* Same as read bc no flags - Will wait for the message if the socket is not blocking*/
    ssize_t received_bytes = recv(inet_socket, data_buffer, sizeof(char)*255, 0);

    printf("This was in the mail: %s\nBytes: %lu\n", data_buffer, received_bytes);
    
    send(inet_socket, "sup", sizeof("sup"), 0);

    /* Make new thread for writing */
    pthread_t stdin_to_serv, stdout_from_serv;

    /* Allocating heap mem for socket num as it's sent to a thread */
    int *p_sock = (int *)malloc(sizeof(inet_socket));
    *p_sock = inet_socket;
    pthread_create(&stdin_to_serv, NULL, write_to_socket, p_sock);
    pthread_create(&stdout_from_serv, NULL, read_from_socket, p_sock);
    

    pthread_join(stdin_to_serv, NULL);
    pthread_join(stdout_from_serv, NULL);

    free(p_sock);

    /* Close the socket */
    close(inet_socket);

}
