#include <inc/client.h>
#include <inc/general.h>
#include <inc/server.h>
#include <inc/socket_utilities.h>

#define INIT  //Initialize settings
#include <inc/setting.h>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    HANDLE_ERROR("Usage: ./clm -[h] -p port -[suwfm] arg", 0);
  }

  optind = 1;
  int opt;

  srand(time(NULL));

  while ((opt = getopt(argc, argv, "hcp:s:u:w:f:m:")) != -1) {
    switch (opt) {
      /* Host-mode */
      case 'h':
        connection.is_server = true;
        break;

        /* Connection port - needed for both client and host */
      case 'p':
        snprintf(
            connection.port, MAX_PORT_STR,
            "%s", optarg);
        break;

        /* Source IP - connection or binding for host */
      case 's':
        if (optarg)
          snprintf(
              connection.ipv4, MAX_IPV4_STR,
              "%s", optarg);
        break;

      /* Client's display name */
      case 'u':
        if (optarg)
          snprintf(
              user.username, MAX_USERNAME_LEN,
              "%s", optarg);
        break;

      /* Password for the chat room */
      case 'w':
        if (optarg)
          snprintf(
              connection.password, MAX_PASSWORD_LEN,
              "%s", optarg);
        break;

      /* client fps/cycles */
      case 'f':
        if (optarg)
          connection.fps = str_to_uint16_t(optarg);
        break;

      case 'm':
        if (optarg)
          connection.max_connections = str_to_uint16_t(optarg);
        break;

      case '?':
        printf("Unknown argument: %s.\n", optarg);
        exit(EXIT_FAILURE);
    }
  }

  if (connection.is_server)
    start_server();  // uses connection struct
  else
    start_client();  // uses connection and user structs

  return 0;
}
