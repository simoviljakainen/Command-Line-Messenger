
/* TODO put headers in some other header file */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "message.h"
#include "window_manager.h"

int main(int argc, char *argv[]){
	pthread_t window_thread;

	init_g(&read_head, &read_tail);

	/* Open window manager*/
	pthread_create(&window_thread, NULL, run_ncurses_window, NULL);
	pthread_join(window_thread, NULL);

	char buffer[256];
#if 0
	while(1){
		fgets(buffer, 256, stdin);
		buffer[strlen(buffer)-1] = '\0';

		if(!strcmp(buffer, "stop"))
			break;

		add_message_to_queue(buffer, &read_head, &read_tail);
	}
	/*
	Msg *ptr = read_head;
	while(ptr != NULL){
		printf("%s\n", ptr->msg);
		ptr = ptr->next;
	}*/
#endif

    return 0;
}