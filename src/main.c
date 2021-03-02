#include <inc/server.h>
#include <inc/client.h>
#include <inc/socket_utilities.h>
#include <inc/general.h>

int main(int argc, char *argv[]){
    bool host_mode = false, client_mode = false;
    char port[MAX_PORT_STR], ipv4[MAX_IPV4_STR] = LOCAL_HOST;

	if(argc < 2){
		HANDLE_ERROR("Usage: ./clm -[hc] -p -[s]", 0);
	}

	/* get cl arguments */
	optind = 1;
    int opt;

	while((opt = getopt(argc, argv, "hcp:s:")) != -1){
		
		switch(opt){

			/* Host-mode */
			case 'h':
				printf("Host-mode\n");
                host_mode = true;
				break;

			/* Client-mode*/
			case 'c':
				printf("Client-mode\n");
                client_mode = true;
				break;

            /* Connection port - needed for both client and host */
			case 'p':
				printf("Port is %s\n", optarg);
                strncpy(port, optarg, MAX_PORT_STR);
                break;

            /* Source IP - connection or binding for host */
			case 's':
				printf("IP is %s\n", (optarg != NULL ? optarg : LOCAL_HOST));
                (optarg != NULL) ? strncpy(ipv4, optarg, MAX_IPV4_STR-1) : "";
				break;

			case '?':
				printf("Unknown argument: %s.\n", optarg);
				exit(EXIT_FAILURE);
		}
	}

    if(host_mode){
        start_server(ipv4, port);

    }else if(client_mode){
        start_client(ipv4, port);

    }else{
		HANDLE_ERROR("No mode given [h = host, c = client]", 0);
    }

    return 0;
}