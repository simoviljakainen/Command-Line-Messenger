#include <inc/crypt.h>
#include <inc/general.h>
#include <inc/message.h>
#include <inc/setting.h>
#include <inc/socket_utilities.h>
#include <inc/window_manager.h>

void *write_to_server(void *p_socket);
void *read_from_server(void *p_socket);

gcry_cipher_hd_t aes256_gcm_handle;

void start_client(void) {
  /*******************   SETTING UP THE CONNECTTION   *******************/

  init_libgcrypt();

  /* Set IP and port */
  in_addr_t addr = str_to_bin_IP(connection.ipv4);
  int16_t port_num = str_to_uint16_t(connection.port);

  /* Create a new socket int protocol = 0 default)*/
  /* SOCK_STREAM -> TCP, SOCK_DGRAM -> UDP */
  int server_socket = socket(AF_INET, SOCK_STREAM, 0);

  if (server_socket == -1) {
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
      sizeof(server_address));

  /* If binding succeeds, connect returns 0, -1 if error and errno */
  if (status) {
    fprintf(
        stderr, "Failed to connect %s:%d - %s\n",
        connection.ipv4, port_num,
        strerror(errno));
    exit(EXIT_FAILURE);
  }

  /**********************   CONNECTED TO SERVER   ***********************/

  char *argon2id_hash = generate_argon2id_hash(connection.password);
  char server_response[MAX_BUFFER];

  send(server_socket, argon2id_hash, strlen(argon2id_hash) + 1, 0);

  if (read_one_packet(server_socket, server_response, MAX_BUFFER)) {
    close(server_socket);
    return;
  }

  if (strcmp(server_response, RESPONSE_OK)) {
    printf("Could not connect (%s). Closing client.\n", server_response);
    return;
  }

  free(argon2id_hash);

  /**********************   CONNECTION ACCEPTED   ***********************/

  init_AES_256_cipher(&aes256_gcm_handle);

  /* Init the message queues */
  init_list(&read_head, &read_tail);
  init_list(&write_head, &write_tail);

  pthread_t message_sender, message_listener, user_interface;

  /* Allocating heap mem for socket fd as it's sent to threads */
  int *sock_fd;

  if ((sock_fd = (int *)malloc(sizeof(server_socket))) == NULL) {
    HANDLE_ERROR("Failed to allocate memory for socket fd", 1);
  }
  *sock_fd = server_socket;

  pthread_create(&message_listener, NULL, read_from_server, sock_fd);

  /* Read/write func frees the memory, hence the reallocation */
  if ((sock_fd = (int *)malloc(sizeof(server_socket))) == NULL) {
    HANDLE_ERROR("Failed to allocate memory for socket fd", 1);
  }
  *sock_fd = server_socket;

  pthread_create(&message_sender, NULL, write_to_server, sock_fd);
  pthread_create(&user_interface, NULL, run_ncurses_window, NULL);

  pthread_join(user_interface, NULL);

  /* Close the other threads */
  pthread_cancel(message_sender);
  pthread_cancel(message_listener);

  /***********************   CONNECTION CLOSED   ************************/

  /* Free queues */
  empty_list(&read_head);
  empty_list(&write_head);

  close(server_socket);

  return;
}

/* Sends messages to server */
void *write_to_server(void *p_socket) {
  int socket = *((int *)p_socket), msg_count = 0;
  free(p_socket);

  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  Msg *outgoing_msg;
  char *ascii_packet, *enc_packet;
  int packet_size, new_size;

  while (true) {
    pthread_mutex_lock(&w_lock);

    if ((outgoing_msg = pop_msg_from_queue(&write_head, NULL)) == NULL) {
      pthread_cond_wait(&message_ready, &w_lock);

      /* Message should be ready now */
      outgoing_msg = pop_msg_from_queue(&write_head, NULL);
    }

    pthread_mutex_unlock(&w_lock);

    ascii_packet = message_to_ascii_packet(outgoing_msg, &packet_size);
    enc_packet = encrypt_packet(
        ascii_packet,
        packet_size, &new_size,
        &aes256_gcm_handle, ++msg_count);
    send(socket, enc_packet, new_size, 0);

    free(ascii_packet);
    free(enc_packet);
    free(outgoing_msg);
  }

  return NULL;
}

/* Reads messages coming from the server and puts them into queue */
void *read_from_server(void *p_socket) {
  int socket = *((int *)p_socket), msg_count = 0;
  free(p_socket);

  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  fd_set connected_socks, ready_socks;

  /* Initialize structs */
  FD_ZERO(&connected_socks);
  FD_SET(socket, &connected_socks);

  ssize_t received_bytes;
  Msg msg;

  char data_buffer[PACKET_MAX_BYTES], *packet;

  while (true) {
    ready_socks = connected_socks;

    if (select(socket + 1, &ready_socks, NULL, NULL, NULL) < 0) {
      HANDLE_ERROR("There was problem with read select", 1);
    }

    if (FD_ISSET(socket, &ready_socks)) {
      received_bytes = recv(socket, data_buffer, PACKET_MAX_BYTES, 0);

      if (received_bytes <= 0) {
        add_message_to_queue(
            compose_message(
                "Server closed the connection",
                "0",
                "/7:System"),
            &read_head, &read_tail, &r_lock);

        return NULL;
      }

      if (received_bytes < MIN_PACKET_SIZE)
        continue;  //rejected, malformed size

      packet = decrypt_packet(
          data_buffer, &aes256_gcm_handle, msg_count++);

      if (packet == NULL) {
        continue;
      }

      msg = ascii_packet_to_message(packet);
      add_message_to_queue(msg, &read_head, &read_tail, &r_lock);
      free(packet);
    }
  }

  return NULL;
}