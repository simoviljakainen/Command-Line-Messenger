#include <inc/crypt.h>
#include <inc/general.h>
#include <inc/message.h>
#include <inc/setting.h>
#include <inc/socket_utilities.h>

void *handle_connections(void *p_socket);
void handle_sigpipe(int _);
int accept_connection(int server_socket);
void handle_disconnect(int index);

void *message_listener(void *socket);
void *broadcast_message(void *_);

int find_client_index(int socket);

Client clients[FD_SETSIZE];
int client_index = 0;

pthread_mutex_t client_lock = PTHREAD_MUTEX_INITIALIZER;

void start_server(void) {
  /*******************   SETTING UP THE CONNECTTION   *******************/

  init_libgcrypt();

  /* Set IP and port */
  in_addr_t addr = str_to_bin_IP(connection.ipv4);
  int16_t port_num = str_to_uint16_t(connection.port);

  /* Create socket */
  int inet_socket = socket(AF_INET, SOCK_STREAM, 0);

  if (inet_socket == -1) {
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
      sizeof(server_address));

  if (status) {
    fprintf(
        stderr, "Failed to bind %s:%d - %s\n",
        connection.ipv4, port_num,
        strerror(errno));
    exit(EXIT_FAILURE);
  }

  /* Convert address to string from binary - just to check if it's malformed */
  char ip_v4[INET_ADDRSTRLEN];
  inet_ntop(
      AF_INET,
      &server_address.sin_addr.s_addr, ip_v4,
      INET_ADDRSTRLEN - 1);
  printf("Bound %s:%d\n", ip_v4, port_num);

  /* FIXME increase the number of queued connections when threads are added
    Listen for connections */
  if (listen(inet_socket, 5) != 0) {
    fprintf(stderr, "Failed to listen %s:%d\n", connection.ipv4, port_num);
    exit(EXIT_FAILURE);
  }

  /*******************   LISTENING FOR CONNECTIONS   ********************/

  init_list(&read_head, &read_tail);

  printf("Listening for connections...\n");

  pthread_t broadcaster, connection_handler;

  /* Start message broadcast thread */
  pthread_create(&broadcaster, NULL, broadcast_message, NULL);

  int *sock_fd;

  if ((sock_fd = (int *)malloc(sizeof(inet_socket))) == NULL) {
    HANDLE_ERROR("Failed to allocate memory for socket fd", 1);
  }
  *sock_fd = inet_socket;

  pthread_create(&connection_handler, NULL, handle_connections, sock_fd);

  char buffer[MAX_BUFFER], command[MAX_BUFFER], args[MAX_BUFFER];
  int index;
  while (true) {
    fgets(buffer, MAX_BUFFER, stdin);
    sscanf(buffer, "%s %s", command, args);

    if (!strcmp(command, C_QUIT)) {
      break;

    } else if (!strcmp(command, C_KICK)) {
      if ((index = find_client_index(atoi(args))) != -1)
        handle_disconnect(index);
    }
  }

  pthread_cancel(broadcaster);
  pthread_cancel(connection_handler);

  /* Close all client connections */
  for (int i = 0; i <= client_index; i++) {
    pthread_cancel(clients[i].read_thread);
    close(clients[i].socket);
  }

  empty_list(&read_head);
  close(inet_socket);

  return;
}

/* One thread handles connections - 
The thread will start a new thread for every client*/
void *handle_connections(void *p_socket) {
  int server_socket = *((int *)p_socket);
  free(p_socket);

  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  int clients_connected = 0;
  fd_set server_sockets, ready_socks;

  /* Initialize structs */
  FD_ZERO(&server_sockets);

  /* Add server_socket into set */
  FD_SET(server_socket, &server_sockets);

  while (clients_connected < connection.max_connections) {
    ready_socks = server_sockets;

    if (select(server_socket + 1, &ready_socks, NULL, NULL, NULL) < 0) {
      HANDLE_ERROR("There was problem with read select connections", 1);
    }

    /* Client is attempting to connect */
    if (FD_ISSET(server_socket, &ready_socks)) {
      if (!accept_connection(server_socket))
        clients_connected++;
    }
  }

  return NULL;
}

int accept_connection(int server_socket) {
  Client new_client;

  socklen_t addr_size = sizeof(new_client.addr);

  new_client.socket = accept(
      server_socket,
      (struct sockaddr *)&new_client.addr,
      &addr_size);

  char ip_v4[MAX_IPV4_STR];
  bin_IP_to_str(new_client.addr.sin_addr.s_addr, ip_v4);
  printf("Connection incoming from %s\n", ip_v4);

  char argon2id_hash[MAX_BUFFER];

  if (read_one_packet(new_client.socket, argon2id_hash, MAX_BUFFER)) {
    close(new_client.socket);
    return 1;
  }

  /* Drop connection - wrong password */
  if (verify_argon2id(argon2id_hash, connection.password)) {
    send(
        new_client.socket,
        RESPONSE_FAIL,
        sizeof(RESPONSE_FAIL), MSG_NOSIGNAL);
    close(new_client.socket);

    return 1;
  }

  /**********************   CONNECTION ACCEPTED   **********************/

  send(
      new_client.socket,
      RESPONSE_OK,
      sizeof(RESPONSE_OK), MSG_NOSIGNAL);

  printf("Connection accepted from %s\n", ip_v4);

  pthread_mutex_lock(&r_lock);

  add_message_to_queue(
      compose_message(
          "New connection accepted",
          "0",
          "/7:Server"),
      &read_head, &read_tail, NULL);

  pthread_cond_signal(&message_ready);

  pthread_mutex_unlock(&r_lock);

  /* Allocating heap mem for socket num as it's sent to a thread */
  int *sock_fd;

  if ((sock_fd = (int *)malloc(sizeof(new_client.socket))) == NULL) {
    HANDLE_ERROR("Failed to allocate memory for socket fd", 1);
  }

  *sock_fd = new_client.socket;
  init_AES_256_cipher(&new_client.aes_gcm_handle);
  new_client.ctr = 0;

  /* Start message listening thread for the new client */
  pthread_create(
      &new_client.read_thread,
      NULL, message_listener, sock_fd);

  pthread_mutex_lock(&client_lock);
  clients[client_index++] = new_client;
  pthread_mutex_unlock(&client_lock);

  return 0;
}

void handle_disconnect(int index) {
  char buffer[MAX_BUFFER];

  snprintf(
      buffer, MAX_BUFFER,
      "Client(%d) has left the chat.",
      clients[index].socket);

  /* Don't cancel yourself */
  if (pthread_self() != clients[index].read_thread)
    pthread_cancel(clients[index].read_thread);

  close(clients[index].socket);
  clean_cipher(&clients[index].aes_gcm_handle);

  /* Remove the client from the clients arr */
  pthread_mutex_lock(&client_lock);

  for (int i = index; i < client_index; i++) {
    clients[i] = clients[i + 1];
  }
  client_index--;

  pthread_mutex_unlock(&client_lock);

  /* Broadcast the lost boi */
  pthread_mutex_lock(&r_lock);

  add_message_to_queue(
      compose_message(buffer, "0", "/7:Server"),
      &read_head, &read_tail, NULL);
  pthread_cond_signal(&message_ready);

  pthread_mutex_unlock(&r_lock);

  return;
}

int find_client_index(int socket) {
  for (int i = 0; i <= client_index; i++) {
    if (clients[i].socket == socket)
      return i;
  }
  return -1;
}

/* Puts messages sent by client into a queue for broadcasts */
void *message_listener(void *p_socket) {
  int index, socket = *((int *)p_socket);
  free(p_socket);

  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  fd_set connected_socks, ready_socks;

  /* Initialize structs */
  FD_ZERO(&connected_socks);
  FD_SET(socket, &connected_socks);

  ssize_t received_bytes;
  Msg msg;

  char data_buffer[PACKET_MAX_BYTES], *packet;

  index = find_client_index(socket);

  while (true) {
    ready_socks = connected_socks;

    /* TODO Time out should be set */
    if (select(socket + 1, &ready_socks, NULL, NULL, NULL) < 0) {
      HANDLE_ERROR("There was problem with read select", 1);
    }

    if (FD_ISSET(socket, &ready_socks)) {
      received_bytes = recv(socket, data_buffer, PACKET_MAX_BYTES, 0);

      /* There was a connection error or it was orderly closed */
      if (received_bytes <= 0) {
        if (index != -1)
          handle_disconnect(index);

        return NULL;
      }

      if (received_bytes < MIN_PACKET_SIZE)
        continue;  //rejected, malformed size

      packet = decrypt_packet(
          data_buffer,
          &clients[index].aes_gcm_handle, clients[index].ctr++);

      if (packet == NULL) {
        printf("Malformed message from %d idx: %d\n", socket, index);
        continue;
      }

      msg = ascii_packet_to_message(packet);
      snprintf(msg.id, ID_SIZE, "%d", socket);
      free(packet);

      pthread_mutex_lock(&r_lock);

      add_message_to_queue(msg, &read_head, &read_tail, NULL);
      pthread_cond_signal(&message_ready);

      pthread_mutex_unlock(&r_lock);
    }
  }

  return NULL;
}

/* Broadcasts every client message to every client connected */
void *broadcast_message(void *_) {
  Msg *outgoing_msg;
  int size, new_size, msg_count = 0;
  char *ascii_packet, *enc_packet;

  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  while (true) {
    pthread_mutex_lock(&r_lock);

    if ((outgoing_msg = pop_msg_from_queue(&read_head, NULL)) == NULL) {
      pthread_cond_wait(&message_ready, &r_lock);

      /* Message should be ready now */
      outgoing_msg = pop_msg_from_queue(&read_head, NULL);
    }

    pthread_mutex_unlock(&r_lock);

    ascii_packet = message_to_ascii_packet(outgoing_msg, &size);

    for (int i = 0; i < client_index; i++) {
      enc_packet = encrypt_packet(
          ascii_packet,
          size, &new_size,
          &clients[i].aes_gcm_handle, ++msg_count);

      if (send(clients[i].socket, enc_packet, new_size, MSG_NOSIGNAL) == -1) {
        handle_disconnect(i);
      }

      free(enc_packet);
    }

    free(outgoing_msg);
    free(ascii_packet);
  }
}
